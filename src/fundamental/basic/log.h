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
#include <sstream>
#include <string_view>
namespace Fundamental
{

enum LogLevel
{
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
;
class Logger
{
public:
     Logger();
     ~Logger();

    template <typename T>
    static inline void LogOutput(LogLevel level, const T& data)
    {
        s_logger->log(static_cast<spdlog::level::level_enum>(level), data);
    }

    template <typename Arg1, typename... Args>
    static inline void LogOutput(LogLevel level, const char* fmt, const Arg1& arg1, const Args&... args)
    {
        s_logger->log(static_cast<spdlog::level::level_enum>(level), fmt, arg1, std::forward<const Args&>(args)...);
    }

    /*
     * outputPath[string] logoutput path
     * programName[string] logoutput filebasename
     * logminLevel[int] logoutput minimum level
     * logFormat[string] control logs' head format
     * fileExt[string] log file's ext
     * rotationHour[int] log file's rotation hour in a day
     * rotationMin[int] log file's rotation minute in a day
     * logFileLimitSize[size_t] single log file's limit size
     *
     */
     static void ConfigLogger(const std::string_view& itemName, const std::string_view& value);

     static void Initialize(bool enableConsoleOutput = true);
     static void Release();
     static void SetErrorHandler(const ErrorHandlerType& handler);
     static void SetCatchHandler(const LogMessageCatchFunc& handler);
     static ErrorHandlerType GetErrorHandler();
     static bool IsDebuggerAttached();
     static void PrintBackTrace();
     static spdlog::pattern_formatter* GetStringFormatter();

private:
     static spdlog::pattern_formatter* s_formatter;
     static spdlog::logger* s_logger;
};

template <typename T>
inline std::string StringFormat(const T& data)
{
    spdlog::details::log_msg msg;
    msg.raw << data;
    Logger::GetStringFormatter()->format(msg);
    return std::string(msg.formatted.data(), msg.formatted.size());
}

template <typename Arg1, typename... Args>
inline std::string StringFormat(const char* fmt, const Arg1& arg1, const Args&... args)
{
    spdlog::details::log_msg msg;
    msg.raw.write(fmt, arg1, args...);
    Logger::GetStringFormatter()->format(msg);
    return std::string(msg.formatted.data(), msg.formatted.size());
}

} // namespace Fundamental

#ifndef DISABLE_TRACE
    #define STR_H(x)      #x
    #define STR_HELPER(x) STR_H(x)
    #ifdef _MSC_VER
        #define FTRACE(...) Fundamental::Logger::LogOutput(Fundamental::LogLevel::trace, "[ " __FILE__ "(" STR_HELPER(__LINE__) ") ] " __VA_ARGS__)
    #else
        #define FTRACE(...) Fundamental::Logger::LogOutput(Fundamental::LogLevel::trace, "[ " __FILE__ ":" STR_HELPER(__LINE__) " ] " __VA_ARGS__)
    #endif
#else
    #define FTRACE(...) (void)0
#endif

#ifndef DISABLE_FLOG
    #define FLOG(logLevel, ...) Fundamental::Logger::LogOutput(logLevel, ##__VA_ARGS__)
#else
    #define FLOG(logLevel, ...) (void)0
#endif // !DISABLE_FLOG

#ifndef NDEBUG
    #define FDEBUG(...) FLOG(Fundamental::LogLevel::debug, ##__VA_ARGS__)
#else
    #define FDEBUG(...) (void)0
#endif
#define FINFO(...) FLOG(Fundamental::LogLevel::info, ##__VA_ARGS__)
#define FERR(...)  FLOG(Fundamental::LogLevel::err, ##__VA_ARGS__)
#define FFAIL(...) FLOG(Fundamental::LogLevel::critical, ##__VA_ARGS__)
#define FWARN(...) FLOG(Fundamental::LogLevel::warn, ##__VA_ARGS__)

#define FASSERT(_check, ...)                                                                                                                          \
    if (!(_check))                                                                                                                                          \
    {                                                                                                                                                       \
        Fundamental::Logger::LogOutput(Fundamental::LogLevel::critical, "[" __FILE__ ":"                                                        \
                                                                                    "(" STR_HELPER(__LINE__) ")] [check:" #_check "] " __VA_ARGS__); \
        Fundamental::Logger::PrintBackTrace();                                                                                                     \
        assert((_check));                                                                                                                                   \
    }

#define FASSERT_ACTION(_check, _action, ...)\
    if (!(_check))                                                                                                                                          \
    {                                                                                                                                                       \
        Fundamental::Logger::LogOutput(Fundamental::LogLevel::critical, "[" __FILE__ ":"                                                        \
                                                                                    "(" STR_HELPER(__LINE__) ")] [check:" #_check "] " __VA_ARGS__); \
        _action;                                                                                                                                            \
    }