
#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/md5_utils.hpp"
#include "test_server.h"
#include <iostream>
#include <optional>

#include <gtest/gtest.h>

int main(int argc, char* argv[]) {

    Fundamental::fs::SwitchToProgramDir(argv[0]);
    Fundamental::Logger::LoggerInitOptions options;
    options.minimumLevel         = Fundamental::LogLevel::debug;
    options.logFormat            = "%^[%L]%H:%M:%S.%e%$[%t] %v ";
    options.logOutputProgramName = "test";
    options.logOutputPath        = "output";
    // Fundamental::Logger::Initialize(std::move(options));
    // ::testing::InitGoogleTest(&argc, argv);
    run_server();
    // auto ret = RUN_ALL_TESTS();
    exit_server();
    // FINFO("finish all test");
    return 0;
}