
#include "fundamental/basic/log.h"
#include "fundamental/data_storage/data_storage.hpp"
#include "fundamental/delay_queue/delay_queue.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
using namespace Fundamental;
DelayQueue queue;
bool exit_flag = false;
std::mutex mutex;
std::condition_variable cv;
void WakeUp() {
    std::scoped_lock<std::mutex> locker(mutex);
    cv.notify_one();
}
TEST(data_storage_test, basic) {
    memory_storage storage(&queue);
    std::string test_table = "table";
    std::string test_key   = "key";
    std::string test_key2  = "key2";
    std::string test_data  = "data";
    storage_config test_config;
    test_config.expired_time_msec = 15;
    storage.expired_signal().Connect([](std::string_view table, std::string_view key) -> Fundamental::SignalBrokenType {
        FINFO("table:{} key:{} is expired", table, key);
        return Fundamental::SignalBrokenType(false);
    });
    EXPECT_TRUE(storage.persist_data(test_table, test_key, test_data, test_config));
    EXPECT_TRUE(storage.persist_data(test_table, test_key2, test_data, test_config));
    EXPECT_TRUE(storage.update_key_expired_time(test_table, test_key, -5));
    EXPECT_TRUE(storage.update_key_expired_time(test_table, test_key2, 5));
    std::this_thread::sleep_for(std::chrono::milliseconds(12));
    EXPECT_TRUE(storage.table_size(test_table) == 1);
    EXPECT_TRUE(storage.has_key(test_table, test_key2));
    EXPECT_TRUE(!storage.has_key(test_table, test_key));
    std::this_thread::sleep_for(std::chrono::milliseconds(12));
    EXPECT_TRUE(storage.table_size(test_table) == 0);
}

int main(int argc, char* argv[]) {
    queue.SetStateChangedCallback([]() { WakeUp(); });
    std::thread t([&]() {
        while (!exit_flag) {
            queue.HandleEvent();
            auto next_timeout = queue.GetNextTimeoutMsec();
            if (next_timeout < 0) next_timeout = 1;
            std::unique_lock<std::mutex> locker(mutex);
            cv.wait_for(locker, std::chrono::milliseconds(next_timeout), [&]() { return !exit_flag; });
        }
    });
    Fundamental::Logger::LoggerInitOptions options;
    options.minimumLevel         = Fundamental::LogLevel::debug;
    options.logFormat            = "%^[%L]%H:%M:%S.%e%$[%t] %v ";
    options.logOutputProgramName = "test";
    options.logOutputPath        = "output";
    Fundamental::Logger::Initialize(std::move(options));
    ::testing::InitGoogleTest(&argc, argv);

    auto ret  = RUN_ALL_TESTS();
    exit_flag = true;
    WakeUp();
    t.join();
    return ret;
}
