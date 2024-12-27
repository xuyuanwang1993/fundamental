
#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/log.h"
#include <chrono>
#include <iostream>
void TestDefaultLogger();
void TestMutiInstance(int index);
int main(int argc, char** argv) {
    TestDefaultLogger();
    TestMutiInstance(1);
    TestMutiInstance(2);
    return 0;
}
void TestDefaultLogger() {
    Fundamental::Logger::PrintBackTrace();
    std::size_t kTargetCount = 120;
    while (!Fundamental::Logger::IsDebuggerAttached() && kTargetCount != 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        kTargetCount--;
    }
    Fundamental::Logger::LoggerInitOptions options;
    options.logOutputProgramName = "test";
    options.logOutputPath        = "output/logs";
    // change hour and min to test rotation logic
    options.rotationHour         = 19;
    options.rotationMin          = 33;
    options.logOutputFileExt     = ".txt";
    options.minimumLevel         = Fundamental::LogLevel::debug;
    options.logFileLimitSize     = 1000;
    options.logFormat            = "%^[%L]%H:%M:%S.%e%$[%t] %v ";
    options.maxLogReserveTimeSec = 100;
    options.errorHandler         = [](const std::string& msg) {
        std::cerr << "------------bad bad-------"
                  << "-------------" << msg << std::endl;
    };
    options.catchHandler = [](Fundamental::LogLevel level, const char* str, std::size_t size) {
        std::cout << "catch msg->level:(" << static_cast<int>(level) << ") " << std::string(str, size);
    };

    Fundamental::Logger::Initialize(std::move(options));

    std::cout << "APP_NAME:" << APP_NAME << std::endl;
    FINFOS << "stream output info";
    FERRS << "stream output error";
    FFAILS << "stream output failed";
    FWARNS << "stream output warn";
    FTRACE("test trace");
    FTRACE("{}", 123);
    FDEBUG("{}", "TEST");
    FINFO("{}", 12312);
    FWARN("{}", "TEST");
    FERR("{}", "TEST");
    FFAIL("{}", "TEST");
    FFAIL("TEST111111111");
    int counts = 100;
    while (counts-- > 0) {
        FERR("---bad------{}{}", counts);
        FINFO("---------{}", counts);
        FERR("aaaaa   {}", counts);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    do {
        FASSERT_ACTION(false, break, "{}", "test assert return ");
        FERR("should not output");
    } while (0);
    try {
        FASSERT(false, "{}", "test assert");
        FERR("should not reach");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
}
void TestMutiInstance(int index) {
    Fundamental::Logger newLogger;
    Fundamental::Logger::LoggerInitOptions options;
    std::string tag              = std::to_string(index);
    options.logOutputProgramName = "test" + tag;
    options.logOutputPath        = "output/logs" + tag;
    // change hour and min to test rotation logic
    options.rotationHour         = 19;
    options.rotationMin          = 33;
    options.logOutputFileExt     = ".txt" + tag;
    options.minimumLevel         = Fundamental::LogLevel::debug;
    options.logFileLimitSize     = 10000;
    options.logFormat            = "%^[%L]%H:%M:%S.%e%$ %v ";
    options.maxLogReserveTimeSec = 1;
    options.errorHandler         = [=](const std::string& msg) {
        std::cerr << tag << "------------bad bad-------"
                  << "-------------" << msg << std::endl;
    };
    options.catchHandler = [=](Fundamental::LogLevel level, const char* str, std::size_t size) {
        std::cout << tag << " catch msg->level:(" << static_cast<int>(level) << ") " << std::string(str, size);
    };

    Fundamental::Logger::Initialize(std::move(options), &newLogger);
    FDEBUGS_I(&newLogger) << "stream output debug";
    FINFOS_I(&newLogger) << "stream output info";
    FERRS_I(&newLogger) << "stream output error";
    FFAILS_I(&newLogger) << "stream output failed";
    FWARNS_I(&newLogger) << "stream output warn";

    FDEBUG_I(&newLogger, "{}", "TEST");
    FINFO_I(&newLogger, "{}", 12312);
    FWARN_I(&newLogger, "{}", "TEST");
    FERR_I(&newLogger, "{}", "TEST");
    FFAIL_I(&newLogger, "{}", "TEST");
    FFAIL_I(&newLogger, "TEST111111111");
    int counts = 100;
    while (counts-- > 0) {
        FERR_I(&newLogger, "---bad------{}{}", counts);
        FINFO_I(&newLogger, "---------{}", counts);
        FERR_I(&newLogger, "aaaaa   {}", counts);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}