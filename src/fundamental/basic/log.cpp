#include "log.h"

#include <condition_variable>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <thread>
#include <unordered_map>
#if TARGET_PLATFORM_WINDOWS
    #include "spdlog/sinks/windebug_sink.h"
    #include <windows.h>

    #include <dbghelp.h>
    #include <iostream>

    #pragma comment(lib, "dbghelp.lib")
#endif
namespace Fundamental
{
namespace details
{
struct CalcFileNameHelper;
class NativeLogSink;
static LogLevel s_minimumLevel                                       = LogLevel::trace;
static std::string s_customLoggerName                                = "logger";
static std::string s_logOutputPath                                   = "logs";
static std::string s_logOutputProgramName                            = "";
static std::string s_logOutputFileExt                                = ".log";
static std::string s_logFormat                                       = "%^%Y-%m-%d %H:%M:%S.%e %l%$ [thread:%t]: %v ";
static std::int32_t s_rotationHour                                   = 23;
static std::int32_t s_rotationMin                                    = 0;
static std::size_t s_logFileLimitSize                                = 10 * 1024 * 1024; // 10M
static std::shared_ptr<NativeLogSink> s_nativeLogSink                = nullptr;
static ErrorHandlerType s_errorHandler                               = nullptr;
static LogMessageCatchFunc s_catchHandler                            = nullptr;
static std::shared_ptr<spdlog::logger> s_loggerStorage               = spdlog::stdout_color_st("console");
static std::shared_ptr<spdlog::pattern_formatter> s_formatterStorage = std::make_shared<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "");
struct LogGuard
{
    LogGuard()
    { // default init
        spdlog::set_level(static_cast<spdlog::level::level_enum>(details::s_minimumLevel));
        spdlog::set_pattern(details::s_logFormat);
    }
    ~LogGuard()
    {
        Logger::Release();
    }
};
using namespace spdlog;
void PrepareLogdir(const std::string& logPath)
{
    spdlog::drop_all();
    try
    {
        bool ret = std::filesystem::create_directories(logPath);
        if (!ret && !std::filesystem::is_directory(logPath))
        {
            std::cout << "create directory " << logPath << " failed" << std::endl;
            std::abort();
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "create directory " << logPath << " failed:" << e.what() << std::endl;
        std::abort();
    }
}
class NativeLogSink SPDLOG_FINAL : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
public:
    // create daily file sink which rotates on given time
    NativeLogSink(filename_t base_filename, int rotation_hour, int rotation_minute, std::size_t logFileLimitSize) :
    m_baseFilename(std::move(base_filename)), m_logFileLimitSize(logFileLimitSize), m_rotationH(rotation_hour), m_rotationM(rotation_minute)
    {
        if (rotation_hour < 0 || rotation_hour > 23 || rotation_minute < 0 || rotation_minute > 59)
        {
            m_rotationH = 0;
            m_rotationM = 0;
        }
        m_rotationTp = _next_rotation_tp();
        if (!m_baseFilename.empty())
            ReOpenFile();
    }

    ~NativeLogSink() override
    {
        StopFileOutputThread();
    }

    void SetMsgCatcher(const LogMessageCatchFunc& catcher)
    {
        m_catcher = catcher;
    }

    void StartFileOutputThread()
    {
        if (m_baseFilename.empty())
            return;
        if (m_bFileWriterWorking)
            return;
        m_pActualWriteCache  = &m_writeCache[0];
        m_bFileWriterWorking = true;
        m_thread             = std::make_unique<std::thread>(&NativeLogSink::Run, this);
    }

    void StopFileOutputThread()
    {
        if (m_baseFilename.empty())
            return;
        if (!m_bFileWriterWorking)
            return;
        m_bFileWriterWorking = false;
        if (m_thread && m_thread->joinable())
            m_thread->join();
        m_thread.reset();
    }

protected:
    void _sink_it(const spdlog::details::log_msg& msg) override
    {
        if (m_catcher)
            m_catcher(static_cast<LogLevel>(msg.level), msg.formatted.data(), msg.formatted.size());
        if (!m_bFileWriterWorking)
            return;
        std::string data(msg.formatted.data(), msg.formatted.size());
        std::scoped_lock<std::mutex> locker(m_threadMutex);
        (*m_pActualWriteCache).emplace_back(std::move(data));
        m_threadCV.notify_one();
    }

    void _flush() override
    {
    }

    void Run()
    {
        std::unique_lock<std::mutex> locker(m_threadMutex);
        while (m_bFileWriterWorking)
        {
            if ((*m_pActualWriteCache).empty())
                m_threadCV.wait_for(locker, std::chrono::milliseconds(20));
            if ((*m_pActualWriteCache).empty())
                continue;
            // switch cache
            std::list<std::string>& m_handleList = *m_pActualWriteCache;
            m_pActualWriteCache                  = (m_pActualWriteCache == &m_writeCache[0]) ? &m_writeCache[1] : &m_writeCache[0];
            // release mutex
            locker.unlock();
            if (std::chrono::system_clock::now() >= m_rotationTp)
            {
                ReOpenFile();
                m_rotationTp = _next_rotation_tp();
            }
            for (auto& item : m_handleList)
                WriteMsg(item);
            m_handleList.clear();
            m_fileHelper.flush();
            if (m_logFileLimitSize > 0 && m_writtenSize >= m_logFileLimitSize)
            {
                ReOpenFile();
            }
            // acquire mutex
            locker.lock();
        }
        // fllush msgs
        if (m_pActualWriteCache == &m_writeCache[0])
        {
            for (auto& item : m_writeCache[1])
                WriteMsg(item);
            for (auto& item : m_writeCache[0])
                WriteMsg(item);
        }
        else
        {
            for (auto& item : m_writeCache[0])
                WriteMsg(item);
            for (auto& item : m_writeCache[1])
                WriteMsg(item);
        }
        m_writeCache[0].clear();
        m_writeCache[1].clear();
    }

private:
    void ReOpenFile()
    {
        try
        {
            m_fileHelper.close();
            if (!m_lastLogFileName.empty() && m_logFileLimitSize > 0)
            { // rename log filename
                filename_t basename, ext;
                std::tie(basename, ext) = spdlog::details::file_helper::split_by_extenstion(m_lastLogFileName);
                std::conditional<std::is_same<filename_t::value_type, char>::value, fmt::MemoryWriter, fmt::WMemoryWriter>::type w;
                w.write(SPDLOG_FILENAME_T("{}-{}{}"), basename, m_fileIndex, ext);
                auto newName = w.str();
                try
                {
                    std::filesystem::rename(m_lastLogFileName, newName);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "rename " << m_lastLogFileName << " to " << newName << " failed " << e.what() << std::endl;
                }
                ++m_fileIndex;
                m_writtenSize = 0;
            }
            auto newFileName = CalcFilename(m_baseFilename);
            m_fileHelper.open(newFileName);
            if (newFileName != m_lastLogFileName)
            { // reset file status
                m_fileIndex       = 1;
                m_writtenSize     = 0;
                m_lastLogFileName = newFileName;
            }
        }
        catch (const std::exception& e)
        {
            auto errorHandler = Logger::GetErrorHandler();
            if (errorHandler)
                errorHandler(e.what());
        }
    }

    void WriteMsg(const std::string& msg)
    {
        try
        {
            m_fileHelper.write(msg);
            m_writtenSize += msg.size();
        }
        catch (const std::exception& e)
        {
            auto errorHandler = Logger::GetErrorHandler();
            if (errorHandler)
                errorHandler(e.what());
        }
    }
    static filename_t CalcFilename(const filename_t& filename)
    {
        std::tm tm = spdlog::details::os::localtime();
        filename_t basename, ext;
        std::tie(basename, ext) = spdlog::details::file_helper::split_by_extenstion(filename);
        std::conditional<std::is_same<filename_t::value_type, char>::value, fmt::MemoryWriter, fmt::WMemoryWriter>::type w;
        w.write(SPDLOG_FILENAME_T("{}_{:04d}-{:02d}-{:02d}{}"), basename, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, ext);
        return w.str();
    }

    std::chrono::system_clock::time_point _next_rotation_tp()
    {
        auto now           = std::chrono::system_clock::now();
        time_t tnow        = std::chrono::system_clock::to_time_t(now);
        tm date            = spdlog::details::os::localtime(tnow);
        date.tm_hour       = m_rotationH;
        date.tm_min        = m_rotationM;
        date.tm_sec        = 0;
        auto rotation_time = std::chrono::system_clock::from_time_t(std::mktime(&date));
        if (rotation_time > now)
        {
            return rotation_time;
        }
        return { rotation_time + std::chrono::hours(24) };
    }

    filename_t m_baseFilename;
    std::size_t m_logFileLimitSize = 0;
    int m_rotationH;
    int m_rotationM;
    std::chrono::system_clock::time_point m_rotationTp;
    spdlog::details::file_helper m_fileHelper;
    filename_t m_lastLogFileName;
    std::size_t m_fileIndex   = 1;
    std::size_t m_writtenSize = 0;

    bool m_bFileWriterWorking = false;
    std::mutex m_threadMutex;
    std::condition_variable m_threadCV;
    std::list<std::string>* m_pActualWriteCache = nullptr;
    std::list<std::string> m_writeCache[2];
    std::unique_ptr<std::thread> m_thread;
    LogMessageCatchFunc m_catcher;
};

class NativeCatchSink SPDLOG_FINAL : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
};
} // namespace details

static details::LogGuard s_guard;
spdlog::logger* Logger::s_logger               = details::s_loggerStorage.get();
spdlog::pattern_formatter* Logger::s_formatter = details::s_formatterStorage.get();
Logger::Logger()
{
}

Logger::~Logger()
{
}

void Logger::ConfigLogger(const std::string_view& itemName, const std::string_view& value)
{
    enum ConfigItemType : std::uint32_t
    {
        OutputPathItem,
        ProgramNameItem,
        LogFormatItem,
        RotationHourItem,
        RotationMinItem,
        FileExtItem,
        MinimumLevelItem,
        LogFileLimitSize
    };
    static std::unordered_map<std::string_view, ConfigItemType> s_configTypeMap = {
        { "outputPath", OutputPathItem },
        { "programName", ProgramNameItem },
        { "logFormat", LogFormatItem },
        { "rotationHour", RotationHourItem },
        { "rotationMin", RotationMinItem },
        { "logFileLimitSize", LogFileLimitSize },
        { "fileExt", FileExtItem },
        { "minimumLevel", MinimumLevelItem },
    };
    auto typeIter = s_configTypeMap.find(itemName);
    if (typeIter == s_configTypeMap.end())
    {
        std::cerr << "unsupported log config item-> " << itemName << ":" << value << std::endl;
        return;
    }
    switch (typeIter->second)
    {
    case OutputPathItem:
        details::s_logOutputPath = value;
        break;
    case ProgramNameItem:
        details::s_logOutputProgramName = value;
        break;
    case LogFormatItem:
        details::s_logFormat = value;
        break;
    case RotationHourItem:
        try
        {
            details::s_rotationHour = std::stoi(value.data());
        }
        catch (const std::exception&)
        {
            std::cerr << "invalid " << typeIter->first << " str " << value << std::endl;
        }
        break;
    case RotationMinItem:
        try
        {
            details::s_rotationMin = std::stoi(value.data());
        }
        catch (const std::exception&)
        {
            std::cerr << "invalid " << typeIter->first << " str " << value << std::endl;
        }
        break;
    case LogFileLimitSize:
        try
        {
            details::s_logFileLimitSize = std::stoull(value.data());
        }
        catch (const std::exception&)
        {
            std::cerr << "invalid " << typeIter->first << " str " << value << std::endl;
        }
        break;
    case FileExtItem:
        details::s_logOutputFileExt = value;
        break;
    case MinimumLevelItem:
        try
        {
            details::s_minimumLevel = static_cast<LogLevel>(std::stoi(value.data()));
        }
        catch (const std::exception&)
        {
            std::cerr << "invalid " << typeIter->first << " str " << value << std::endl;
        }
        break;
    default:
        std::cerr << "unsupported log config item-> " << itemName << ":" << value << std::endl;
        break;
    }
}

void Logger::Initialize(bool enableConsoleOutput)
{
    try
    {
        std::list<spdlog::sink_ptr> initList;
        if (enableConsoleOutput)
        {
#if TARGET_PLATFORM_WINDOWS
            initList.emplace_back(std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>());
            initList.emplace_back(std::make_shared<spdlog::sinks::windebug_sink_mt>());
#elif TARGET_PLATFORM_LINUX
            initList.emplace_back(std::make_shared<spdlog::sinks::ansicolor_stdout_sink_st>());
#endif
        }
        std::string fileName;
        if (!details::s_logOutputPath.empty() && !details::s_logOutputProgramName.empty())
        {
            details::PrepareLogdir(details::s_logOutputPath);
            fileName = details::s_logOutputPath + "/" + details::s_logOutputProgramName + details::s_logOutputFileExt;
        }
        { // init file writer
            details::s_nativeLogSink = std::make_shared<details::NativeLogSink>(fileName, details::s_rotationHour, details::s_rotationMin, details::s_logFileLimitSize);
            details::s_nativeLogSink->SetMsgCatcher(details::s_catchHandler);
            details::s_nativeLogSink->StartFileOutputThread();
            initList.emplace_back(details::s_nativeLogSink);
        }

        details::s_loggerStorage = spdlog::create(details::s_customLoggerName, initList.begin(), initList.end());
        Logger::s_logger   = details::s_loggerStorage.get();
        spdlog::set_level(static_cast<spdlog::level::level_enum>(details::s_minimumLevel));
        spdlog::set_pattern(details::s_logFormat);
        if (details::s_errorHandler)
        {
            spdlog::set_error_handler([](const std::string& msg) {
                details::s_errorHandler(msg);
            });
            ;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << " log initialize failed!" << e.what() << std::endl;
    }
}

void Logger::Release()
{
    if (details::s_nativeLogSink)
        details::s_nativeLogSink->StopFileOutputThread();
    spdlog::drop_all();
}

ErrorHandlerType Logger::GetErrorHandler()
{
    return details::s_errorHandler;
}

bool Logger::IsDebuggerAttached()
{
#if TARGET_PLATFORM_WINDOWS
    return ::IsDebuggerPresent();
#else
    // alwasy return true
    return true;
#endif
}

void Logger::PrintBackTrace()
{
#if TARGET_PLATFORM_WINDOWS
    HANDLE process = GetCurrentProcess();
    HANDLE thread  = GetCurrentThread();

    CONTEXT context      = {};
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    SymInitialize(process, NULL, TRUE);

    STACKFRAME64 stack = {};
    DWORD imageType    = IMAGE_FILE_MACHINE_I386;

    #ifdef _M_IX86
    imageType              = IMAGE_FILE_MACHINE_I386;
    stack.AddrPC.Offset    = context.Eip;
    stack.AddrPC.Mode      = AddrModeFlat;
    stack.AddrFrame.Offset = context.Ebp;
    stack.AddrFrame.Mode   = AddrModeFlat;
    stack.AddrStack.Offset = context.Esp;
    stack.AddrStack.Mode   = AddrModeFlat;
    #elif _M_X64
    imageType              = IMAGE_FILE_MACHINE_AMD64;
    stack.AddrPC.Offset    = context.Rip;
    stack.AddrPC.Mode      = AddrModeFlat;
    stack.AddrFrame.Offset = context.Rsp;
    stack.AddrFrame.Mode   = AddrModeFlat;
    stack.AddrStack.Offset = context.Rsp;
    stack.AddrStack.Mode   = AddrModeFlat;
    #endif

    for (size_t i = 0; i < 62; ++i)
    {
        BOOL result = StackWalk64(
            imageType, process, thread, &stack, &context, NULL,
            SymFunctionTableAccess64, SymGetModuleBase64, NULL);

        if (!result)
        {
            break;
        }

        DWORD64 displacement      = 0;
        IMAGEHLP_SYMBOL64* symbol = (IMAGEHLP_SYMBOL64*)malloc(sizeof(IMAGEHLP_SYMBOL64) + 256);
        symbol->SizeOfStruct      = sizeof(IMAGEHLP_SYMBOL64);
        symbol->MaxNameLength     = 255;

        if (SymGetSymFromAddr64(process, stack.AddrPC.Offset, &displacement, symbol))
        {
            std::cout << "pc[" << stack.AddrPC.Offset << "]: " << symbol->Name << std::endl;
        }

        free(symbol);
    }

    SymCleanup(process);
#endif
}

spdlog::pattern_formatter* Logger::GetStringFormatter()
{
    return Logger::s_formatter;
}

void Logger::TestLogInstance()
{
    std::cout<<"internal log instance "<<details::s_loggerStorage.get()<<std::endl;
    std::cout<<"spd log storage "<<&spdlog::details::registry::instance()<<std::endl;
}

void Logger::SetErrorHandler(const ErrorHandlerType& handler)
{
    details::s_errorHandler = handler;
}

void Logger::SetCatchHandler(const LogMessageCatchFunc& handler)
{
    details::s_catchHandler = handler;
    if (details::s_nativeLogSink)
        details::s_nativeLogSink->SetMsgCatcher(details::s_catchHandler);
}
} // namespace Fundamental