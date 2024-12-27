#pragma once
#ifdef _MSC_VER
    #pragma warning(disable : 4996) // disable warning 4996
#endif
#include "spdlog/spdlog.h"
#ifdef _MSC_VER
    #pragma warning(default : 4996) // enable warning 4996 back
#endif

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>

namespace Fundamental {

enum LogLevel {
    trace    = 0,
    debug    = 1,
    info     = 2,
    warn     = 3,
    err      = 4,
    critical = 5,
    off      = 6
};
using ErrorHandlerType    = std::function<void(const std::string&)>;
using LogMessageCatchFunc = std::function<void(LogLevel level, const char* /*str*/, std::size_t /*size*/)>;
namespace details {
class NativeLogSink;
}
class Logger {
    friend class details::NativeLogSink;
    template <typename Arg1, typename... Args>
    friend std::string StringFormat(const char* fmt, const Arg1& arg1, const Args&... args);

public:
    struct LoggerInitOptions {
        static constexpr LogLevel kDefaultLogLevel                = LogLevel::trace;
        static constexpr const char* kDefaultCustomLoggerName     = "logger";
        static constexpr const char* kDefaultLogOutputPath        = "logs";
        static constexpr const char* kDefaultLogOutputProgramName = "";
        static constexpr const char* kDefaultLogOutputFileExt     = ".log";
        static constexpr const char* kDefaultLogFormat            = "%^%Y-%m-%d %H:%M:%S.%e %l%$ [thread:%t]: %v";
        static constexpr std::int32_t kDefaultRotationHour        = 23;
        static constexpr std::int32_t kDefaultRotationMin         = 0;
        static constexpr std::size_t kDefaultLogFileLimitSize     = 10 * 1024 * 1024; // 10M
        // 2 month
        static constexpr std::int64_t kDefaultMaxLogReserveTimeSec =
            60 /*min*/ * 60 /*hour*/ * 24 /*day*/ * 30 /*mon*/ * 2;
        std::string loggerName = kDefaultCustomLoggerName;
        // minimum log level
        std::optional<LogLevel> minimumLevel;
        // log output path
        std::optional<std::string> logOutputPath;
        // log output filebasename
        std::optional<std::string> logOutputProgramName;
        // log file's ext
        std::optional<std::string> logOutputFileExt;
        // control logs' head format
        std::optional<std::string> logFormat;

        // eg: if the rotationHour=17 rotationMin=0
        // when the day's time come to 17:00
        // new file destination will be used
        //  log file's rotation hour in a day
        std::optional<std::int32_t> rotationHour;
        // log file's rotation minute in a day
        std::optional<std::int32_t> rotationMin;
        // single log file's limit size
        std::optional<std::size_t> logFileLimitSize;
        // llean up log files that have existed for more than maxLogReserveTimeSec
        std::optional<std::int64_t> maxLogReserveTimeSec;
        // control log to console
        bool enableConsoleOutput = true;
        //
        ErrorHandlerType errorHandler = nullptr;
        // catch all log msg
        LogMessageCatchFunc catchHandler = nullptr;
    };

public:
    Logger(const std::string_view& format_str = "%^[%L]%$ %v");
    ~Logger();

    template <typename T>
    inline void LogOutput(LogLevel level, const T& data) {
        loggerStorage->log(static_cast<spdlog::level::level_enum>(level), data);
    }

    template <typename Arg1, typename... Args>
    inline void LogOutput(LogLevel level, const char* fmt, const Arg1& arg1, const Args&... args) {
        loggerStorage->log(static_cast<spdlog::level::level_enum>(level), fmt, arg1,
                           std::forward<const Args&>(args)...);
    }
    static void Initialize(LoggerInitOptions options, Logger* logger = s_defaultLogger);
    static void Release(Logger* logger = s_defaultLogger);
    static bool IsDebuggerAttached();
    static void PrintBackTrace();
    static spdlog::pattern_formatter* GetStringFormatter();
    static void TestLogInstance();

public:
    static Logger* s_defaultLogger;

private:
    // for raw string format
    static spdlog::pattern_formatter* s_formatter;

    ErrorHandlerType errorHandler;
    // log output
    std::shared_ptr<details::NativeLogSink> nativeLogSink = nullptr;
    std::shared_ptr<spdlog::logger> loggerStorage         = nullptr;
};

inline std::string StringFormat() {
    return "";
}

template <typename T>
inline std::string StringFormat(const T& data) {
    spdlog::details::log_msg msg;
    msg.raw << data;
    return std::string(msg.formatted.data(), msg.formatted.size());
}

inline std::string StringFormat(const char* data) {
    return std::string(data, strlen(data));
}

template <typename Arg1, typename... Args>
inline std::string StringFormat(const char* fmt, const Arg1& arg1, const Args&... args) {
    try {
        spdlog::details::log_msg msg;
        msg.raw.write(fmt, arg1, args...);
        Logger::GetStringFormatter()->format(msg);
        return std::string(msg.formatted.data(), msg.formatted.size());
    } catch (const std::exception& e) {
        if (Logger::s_defaultLogger->errorHandler) Logger::s_defaultLogger->errorHandler(e.what());
        return "";
    }
}

class LoggerStream final {
public:
    explicit LoggerStream(Logger* logger, LogLevel level);
    explicit LoggerStream(Logger* logger, LogLevel level, const char* fileName, const char* funcName,
                          std::int32_t line);
    ~LoggerStream();
    std::stringstream& stream() {
        return ss_;
    }

private:
    Logger* const loggerRef = nullptr;
    const LogLevel level_;
    std::stringstream ss_;
};
} // namespace Fundamental
#define STR_H(x)      #x
#define STR_HELPER(x) STR_H(x)

#ifndef DISABLE_FLOG
    #define FLOG(logger, logLevel, ...) (logger)->LogOutput(logLevel, __VA_ARGS__)
    #define FLOG_DEBUGINFO(logger, logLevel, ...)                                                                      \
        do {                                                                                                           \
            auto __debugInfo__ = Fundamental::StringFormat("[" __FILE__ ":{}"                                          \
                                                           "(" STR_HELPER(__LINE__) ")] ",                             \
                                                           __func__);                                                  \
            (logger)->LogOutput(logLevel, __debugInfo__ + Fundamental::StringFormat(__VA_ARGS__));                     \
        } while (0)
#else
    #define FLOG(logger, logLevel, ...)           (void)0
    #define FLOG_DEBUGINFO(logger, logLevel, ...) (void)0
#endif // !DISABLE_FLOG

#ifndef NDEBUG
    #define FDEBUG(...)           FLOG(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::debug, __VA_ARGS__)
    #define FDEBUG_I(logger, ...) FLOG(logger, Fundamental::LogLevel::debug, __VA_ARGS__)
    #define FASSERT_THROWN(_check, ...)                                                                                \
        if (!(_check)) {                                                                                               \
            auto __debugInfo__ = Fundamental::StringFormat("[" __FILE__ ":{}"                                          \
                                                           "(" STR_HELPER(__LINE__) ")] [####check####:" #_check "] ", \
                                                           __func__);                                                  \
            throw std::runtime_error(__debugInfo__ + Fundamental::StringFormat(__VA_ARGS__));                          \
        }
#else
    #define FDEBUG(...)                 (void)0
    #define FDEBUG_I(logger, ...)       (void)0
    #define FASSERT_THROWN(_check, ...) (void)0
#endif

#ifndef DISABLE_TRACE
    #define FTRACE(...)                                                                                                \
        FLOG_DEBUGINFO(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::trace, ##__VA_ARGS__)
#else
    #define FTRACE(...) (void)0
#endif

#define FINFO(...) FLOG(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::info, ##__VA_ARGS__)
#define FERR(...)  FLOG_DEBUGINFO(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::err, ##__VA_ARGS__)
#define FFAIL(...) FLOG_DEBUGINFO(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::critical, ##__VA_ARGS__)
#define FWARN(...) FLOG_DEBUGINFO(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::warn, ##__VA_ARGS__)

#define FDEBUGS Fundamental::LoggerStream(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::debug).stream()
#define FINFOS  Fundamental::LoggerStream(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::info).stream()
#define FERRS                                                                                                          \
    Fundamental::LoggerStream(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::err, __FILE__, __func__,    \
                              __LINE__)                                                                                \
        .stream()
#define FFAILS                                                                                                         \
    Fundamental::LoggerStream(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::critical, __FILE__,         \
                              __func__, __LINE__)                                                                      \
        .stream()
#define FWARNS                                                                                                         \
    Fundamental::LoggerStream(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::warn, __FILE__, __func__,   \
                              __LINE__)                                                                                \
        .stream()
#define FTRACES                                                                                                        \
    Fundamental::LoggerStream(Fundamental::Logger::s_defaultLogger, Fundamental::LogLevel::trace, __FILE__, __func__,  \
                              __LINE__)                                                                                \
        .stream()

#ifndef DISABLE_ASSERT
    #define FASSERT(_check, ...) FASSERT_THROWN(_check, ##__VA_ARGS__)

    #define FASSERT_ACTION(_check, _action, ...)                                                                       \
        if (!(_check)) {                                                                                               \
            Fundamental::Logger::s_defaultLogger->LogOutput(Fundamental::LogLevel::critical,                           \
                                                            "[" __FILE__ ":"                                           \
                                                            "(" STR_HELPER(__LINE__) ")] [####check####:" #_check      \
                                                                                     "] " __VA_ARGS__);                \
            _action;                                                                                                   \
        }
#else
    #define FASSERT(_check, ...)                 (void)0
    #define FASSERT_ACTION(_check, _action, ...) (void)0
#endif

// add  more logger instance
#define FINFO_I(logger, ...) FLOG(logger, Fundamental::LogLevel::info, ##__VA_ARGS__)
#define FERR_I(logger, ...)  FLOG_DEBUGINFO(logger, Fundamental::LogLevel::err, ##__VA_ARGS__)
#define FFAIL_I(logger, ...) FLOG_DEBUGINFO(logger, Fundamental::LogLevel::critical, ##__VA_ARGS__)
#define FWARN_I(logger, ...) FLOG_DEBUGINFO(logger, Fundamental::LogLevel::warn, ##__VA_ARGS__)

#define FDEBUGS_I(logger) Fundamental::LoggerStream(logger, Fundamental::LogLevel::debug).stream()
#define FINFOS_I(logger)  Fundamental::LoggerStream(logger, Fundamental::LogLevel::info).stream()
#define FERRS_I(logger)                                                                                                \
    Fundamental::LoggerStream(logger, Fundamental::LogLevel::err, __FILE__, __func__, __LINE__).stream()
#define FFAILS_I(logger)                                                                                               \
    Fundamental::LoggerStream(logger, Fundamental::LogLevel::critical, __FILE__, __func__, __LINE__).stream()
#define FWARNS_I(logger)                                                                                               \
    Fundamental::LoggerStream(logger, Fundamental::LogLevel::warn, __FILE__, __func__, __LINE__).stream()

/*end of file*/