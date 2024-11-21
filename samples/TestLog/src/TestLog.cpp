
#include "fundamental/basic/log.h"
#include <chrono>
#include <iostream>
int main(int argc, char** argv)
{
    Fundamental::Logger::PrintBackTrace();
    std::size_t kTargetCount = 120;
    while (!Fundamental::Logger::IsDebuggerAttached() && kTargetCount != 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        kTargetCount--;
    }
    

    Fundamental::Logger::ConfigLogger("programName", "test");
    Fundamental::Logger::ConfigLogger("outputPath", "output/logs");
    // change hour and min to test rotation logic
    Fundamental::Logger::ConfigLogger("rotationHour", "19");
    Fundamental::Logger::ConfigLogger("rotationMin", "33");
    Fundamental::Logger::ConfigLogger("fileExt", ".txt");
    Fundamental::Logger::ConfigLogger("minimumLevel", "0");
    Fundamental::Logger::ConfigLogger("logFileLimitSize", "10000");
    Fundamental::Logger::ConfigLogger("logFormat", "%^[%L]%H:%M:%S.%e%$[%t] %v ");
    int cnt1 = 1000;
    std::string teststr(100, 'c');
    
    auto errorHandler = [](const std::string& msg) {
        std::cerr << "------------bad bad-------"
                  << "-------------" << msg << std::endl;
    };
    Fundamental::Logger::SetErrorHandler(errorHandler);
    auto msgCatcher = [](Fundamental::LogLevel level, const char* str, std::size_t size) {
        std::cout << "catch msg->level:(" << static_cast<int>(level) << ") " << std::string(str, size);
    };
    Fundamental::Logger::SetCatchHandler(msgCatcher);

    Fundamental::Logger::Initialize();
    //while (true)
    //{
    //    FINFO("{}", teststr);
    //    std::this_thread::sleep_for(std::chrono::seconds(10));
    //}
    std::cout << "APP_NAME:" << APP_NAME << std::endl;
    FTRACE("test trace");
    FTRACE("{}", 123);
    FDEBUG("{}", "TEST");
    FINFO("{}", 12312);
    FWARN("{}", "TEST");
    FERR("{}", "TEST");
    FFAIL("{}", "TEST");
    FFAIL("TEST111111111");
    int counts = 100;
    while (counts-- > 0)
    {
        FERR("---bad------{}{}", counts);
        FINFO("---------{}", counts);
        FERR("aaaaa   {}", counts);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    do
    {
        FASSERT_ACTION(false, break, "{}", "test assert return ");
        FERR("should not output");
    } while (0);
    FASSERT(false, "{}", "test assert");
    FERR("should not reach");
    return 0;
}
