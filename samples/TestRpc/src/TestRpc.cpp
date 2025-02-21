
#include "test_server.h"

#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "rpc/basic/custom_rpc_proxy.hpp"

#include "rpc/basic/rpc_client.hpp"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>

using namespace network;
using namespace network::rpc_service;
#if 0
TEST(rpc_test, test_add) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        rpc_client client("127.0.0.1", 9000);

        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::int32_t op1 = 1;
        std::int32_t op2 = 2;
        {
            auto result = client.call<std::int32_t>("add", op1, op2);
            EXPECT_EQ(op1 + op2, result);
        }

        {
            auto result = client.call<2000, std::int32_t>("add", op2, op1);
            EXPECT_EQ(op1 + op2, result);
        }
        // test return value type not matched
        EXPECT_THROW((client.call<2000, std::string>("add", op2, op1)), std::invalid_argument);
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}

TEST(rpc_test, test_translate) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        rpc_client client("127.0.0.1", 9000);
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }

        auto result = client.call<std::string>("translate", "hello");
        EXPECT_TRUE(result == "HELLO");
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}

TEST(rpc_test, test_hello) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        rpc_client client("127.0.0.1", 9000);
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        client.call("hello", "purecpp");
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}


TEST(rpc_test, test_get_person_name) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        rpc_client client("127.0.0.1", 9000);
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::string name = "tom";
        auto result      = client.call<std::string>("get_person_name", person { 1, name, 20 });
        EXPECT_EQ(name, result);
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}
TEST(rpc_test, test_get_person) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        rpc_client client("127.0.0.1", 9000);
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        auto result = client.call<50, person>("get_person");
        EXPECT_EQ("tom", result.name);
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}

TEST(rpc_test, test_async_client) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    rpc_client client("127.0.0.1", 9000);
    bool r = client.connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    client.set_error_callback([](asio::error_code ec) { std::cout << ec.message() << std::endl; });

    auto f = client.async_call<FUTURE>("get_person");

    EXPECT_EQ("tom", f.get().as<person>().name);
    auto fu = client.async_call<FUTURE>("hello", "purecpp");
    fu.get().as(); // no return
}

static std::vector<std::uint8_t> s_file_data(1024 * 1024, 'a');
TEST(rpc_test, test_upload) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    rpc_client client("127.0.0.1", 9000);
    bool r = client.connect(1);
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }
    std::string file_path = "test.file";

    EXPECT_TRUE(Fundamental::fs::WriteFile(file_path, s_file_data.data(), s_file_data.size()));
    std::ifstream file(file_path, std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t file_len = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string conent;
    conent.resize(file_len);
    file.read(&conent[0], file_len);

    {
        auto f = client.async_call<FUTURE>("upload", "test", conent);
        EXPECT_NO_THROW((f.get().as()));
    }
    {
        auto f = client.async_call<FUTURE>("upload", "test1", conent);
        EXPECT_NO_THROW((f.get().as()));
    }
}

TEST(rpc_test, test_download) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    rpc_client client("127.0.0.1", 9000);
    bool r = client.connect(1);
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    auto f       = client.async_call<FUTURE>("download", "test");
    auto content = f.get().as<std::string>();
    EXPECT_TRUE(s_file_data.size() == content.size() &&
                ::memcmp(s_file_data.data(), content.data(), content.size()) == 0);
}

TEST(rpc_test, test_echo) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    rpc_client client("127.0.0.1", 9000);
    bool r = client.connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    {
        dummy1 d1 { 42, "test" };
        auto result = client.call<dummy1>("get_dummy", d1);
        EXPECT_TRUE(d1.id == result.id);
        EXPECT_TRUE(d1.str == result.str);
    }

    {
        auto result = client.call<std::string>("echo", "test");
        EXPECT_EQ(result, "test");
    }

    {
        auto result = client.call<std::string>("delay_echo", "test");
        EXPECT_EQ(result, "test");
    }
}

TEST(rpc_test, test_call_with_timeout) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    rpc_client client;
    client.async_connect("127.0.0.1", 9000);

    try {
        auto result = client.call<50, person>("get_person");
        std::cout << result.name << std::endl;
        client.close();
        [[maybe_unused]] bool r = client.connect();
        EXPECT_TRUE(r);
        result = client.call<50, person>("get_person");
        std::cout << result.name << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "test_call_with_timeout:throw " << ex.what() << std::endl;
        EXPECT_TRUE(false);
    }
}

TEST(rpc_test, test_connect) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    rpc_client client;
    client.enable_auto_reconnect(); // automatic reconnect
    client.enable_auto_heartbeat(); // automatic heartbeat
    [[maybe_unused]] bool r = client.connect("127.0.0.1", 9000);
    EXPECT_TRUE(r && client.has_connected());
}

TEST(rpc_test, test_callback) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 200); });
    rpc_client client;
    client.enable_auto_reconnect();
    client.enable_auto_heartbeat();
    [[maybe_unused]] bool r = client.connect("127.0.0.1", 9000);
    EXPECT_TRUE(r);
    std::atomic<std::size_t> count = 200;
    for (size_t i = 0; i < 100; i++) {
        std::string test = "test" + std::to_string(i + 1);
        // set timeout 100ms
        FDEBUGS << "post echo:" << test;
        client.async_call<150>(
            "delay_echo",
            [&](const asio::error_code& ec, string_view data) {
                Fundamental::ScopeGuard g([&]() { --count; });
                if (ec) {
                    FINFOS << "delay_echo timeout:" << ec.value() << " " << ec.message();
                    return;
                }

                if (has_error(data)) {
                    FINFOS << "delay_echo error msg:" << get_error_msg(data);
                } else {
                    auto str = get_result<std::string>(data);
                    FWARNS << "delay_echo " << str;
                }
            },
            test);

        std::string test1 = "test" + std::to_string(i + 2);
        FDEBUGS << "post delay_echo:" << test1;
        // zero means no timeout check, no param means using default timeout(5s)
        client.async_call<150>(
            "echo",
            [&](const asio::error_code& ec, string_view data) {
                Fundamental::ScopeGuard g([&]() { --count; });
                if (ec) {
                    FINFOS << "echo timeout:" << ec.value() << " " << ec.message();
                    return;
                }

                if (has_error(data)) {
                    FINFOS << "echo error msg:" << get_error_msg(data);
                } else {
                    auto str = get_result<std::string>(data);
                    FWARNS << "echo " << str;
                }
            },
            test1);
    }
    while (count.load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST(rpc_test, test_sub1) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    bool success = true;
    do {
        rpc_client client;
        client.enable_auto_reconnect();
        client.enable_auto_heartbeat();
        bool r = client.connect("127.0.0.1", 9000);
        if (!r) {
            success = false;
            break;
        }
        std::atomic<std::size_t> target_count = 0;
        client.subscribe("key", [&](string_view data) {
            msgpack_codec codec;
            try {
                auto msg = codec.unpack<std::string>(data.data(), data.size());
                std::cout << "key_1:" << msg << "\n";
                target_count++;
            } catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
                success = false;
            }
        });

        client.subscribe("key", "sub_key", [&](string_view data) {
            msgpack_codec codec;
            try {
                person p = codec.unpack<person>(data.data(), data.size());
                std::cout << "sub1 by key:" << p.name << "\n";
                target_count++;
            } catch (const std::exception& e) {
                success = false;
                std::cerr << e.what() << '\n';
            }
        });
        client.subscribe("key", "sub_key2", [&](string_view data) {
            msgpack_codec codec;
            try {
                person p = codec.unpack<person>(data.data(), data.size());
                std::cout << "sub1 by key_2:" << p.name << "\n";
                target_count++;
            } catch (const std::exception& e) {
                success = false;
                std::cerr << e.what() << '\n';
            }
        });
        rpc_client client2;
        client2.enable_auto_reconnect();
        client2.enable_auto_heartbeat();
        r = client2.connect("127.0.0.1", 9000);
        if (!r) {
            success = false;
            break;
        }

        client2.subscribe("key", [&](string_view data) {
            msgpack_codec codec;
            try {
                auto msg = codec.unpack<std::string>(data.data(), data.size());
                std::cout << "key2:" << msg << "\n";
                target_count++;
            } catch (const std::exception& e) {
                success = false;
                std::cerr << e.what() << '\n';
            }
        });

        rpc_client client3;
        client3.enable_auto_reconnect();
        client3.enable_auto_heartbeat();
        r = client3.connect("127.0.0.1", 9000);
        if (!r) {
            success = false;
            break;
        }

        person p { 10, "jack", 21 };
        client3.publish("key", "hello from client");
        client3.publish_by_token("key", "sub_key", p);
        while (success && target_count.load() < 10)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (0);
    EXPECT_EQ(success, true);
}
TEST(rpc_test, test_proxy) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });

    rpc_client client("127.0.0.1", std::stoi(kProxyServicePort));
    client.set_proxy(std::make_shared<network::rpc_service::CustomRpcProxy>(kProxyServiceName, kProxyServiceField,
                                                                            kProxyServiceToken));
    bool r = client.connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    {
        dummy1 d1 { 42, "test" };
        auto result = client.call<dummy1>("get_dummy", d1);
        EXPECT_TRUE(d1.id == result.id);
        EXPECT_TRUE(d1.str == result.str);
    }

    {
        auto result = client.call<std::string>("echo", "test");
        EXPECT_EQ(result, "test");
    }
}

TEST(rpc_test, test_auto_reconnect) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 50); });

    try {
        rpc_client client("127.0.0.1", 9000);
        client.enable_auto_reconnect();
        client.set_reconnect_delay(10);
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::int32_t cnt = 3;
        while (cnt > 0) {
            --cnt;
            try
            {
                client.call<void>("auto_disconnect", cnt);
            }
            catch(...)
            {
                
            }
            
            
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}
#endif
#ifndef RPC_DISABLE_SSL
TEST(rpc_test, test_ssl) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });

    try {
        rpc_client client("127.0.0.1", 9000);
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::size_t cnt = 10;
        while (cnt > 0) {
            --cnt;
            client.call<std::string>("echo", std::to_string(cnt) + "test nossl");
        }
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
    std::cout << "finish no ssl" << std::endl;
    try {
        rpc_client client("127.0.0.1", 9000);
        client.enable_ssl("server.crt");
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::size_t cnt = 10;
        while (cnt > 0) {
            --cnt;
            client.call<std::string>("echo", std::to_string(cnt) + "test");
        }
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }

    std::cout << "finish ssl" << std::endl;
}
// TEST(rpc_test, test_ssl_proxy) {
//     Fundamental::Timer check_timer;
//     Fundamental::ScopeGuard check_guard(
//         [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });

//     rpc_client client("127.0.0.1", std::stoi(kProxyServicePort));
//     client.enable_ssl("server.crt");
//     client.set_proxy(std::make_shared<network::rpc_service::CustomRpcProxy>(kProxyServiceName, kProxyServiceField,
//                                                                             kProxyServiceToken));
//     bool r = client.connect();
//     if (!r) {
//         EXPECT_TRUE(false && "connect timeout");
//         return;
//     }

//     {
//         dummy1 d1 { 42, "test" };
//         auto result = client.call<dummy1>("get_dummy", d1);
//         EXPECT_TRUE(d1.id == result.id);
//         EXPECT_TRUE(d1.str == result.str);
//     }

//     {
//         auto result = client.call<std::string>("echo", "test");
//         EXPECT_EQ(result, "test");
//     }
// }
#endif

int main(int argc, char** argv) {
    Fundamental::Logger::LoggerInitOptions options;
    options.minimumLevel = Fundamental::LogLevel::debug;
    options.logFormat    = "%^[%L]%H:%M:%S.%e%$[%t] %v ";

    Fundamental::Logger::Initialize(std::move(options));
    ::testing::InitGoogleTest(&argc, argv);
    run_server();
    auto ret = RUN_ALL_TESTS();
    exit_server();
    return ret;
}