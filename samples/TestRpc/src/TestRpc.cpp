
#include "test_server.h"

#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "rpc/proxy/custom_rpc_proxy.hpp"

#include "fundamental/application/application.hpp"
#include "rpc/rpc_client.hpp"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>

using namespace network;
using namespace network::rpc_service;
static Fundamental::ThreadPool& s_test_pool = Fundamental::ThreadPool::Instance<101>();
// TEST(rpc_test, test_echo_stream_proxy_mutithread) {
//     std::vector<Fundamental::ThreadPoolTaskToken<void>> tasks;
//     auto nums      = 100;
//     auto task_func = []() {
//         auto client=rpc_client::make_shared();
//         client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName,
//         kProxyServiceField,
//                                                                                 kProxyServiceToken));
//         [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
//         EXPECT_TRUE(r && client->has_connected());
//         auto stream = client->upgrade_to_stream("test_echo_stream");
//         EXPECT_TRUE(stream != nullptr);
//         std::size_t cnt  = 5;
//         std::string base = "msg ";
//         while (cnt != 0) {
//             {
//                 auto ret = stream->Write(base + std::to_string(cnt));
//                 if (!ret) {
//                     FDEBUG(ret);
//                     EXPECT_TRUE(ret);
//                 }
//             }
//             --cnt;
//             std::string tmp;
//             {
//                 auto ret = stream->Read(tmp, 0);
//                 if (!ret) {
//                     FDEBUG(ret);
//                     EXPECT_TRUE(ret);
//                 }
//             }
//             FINFO("mutithread echo msg:{}", tmp);
//         }
//         {
//             auto ret = stream->WriteDone();
//             if (!ret) {
//                 FDEBUG(ret);
//                 EXPECT_TRUE(ret);
//             }
//         }
//         {
//             auto ret = !stream->Finish(0);
//             if (!ret) {
//                 FDEBUG(ret);
//                 EXPECT_TRUE(ret);
//             }
//         }
//     };
//     while (nums > 0) {
//         tasks.emplace_back(s_test_pool.Enqueue(task_func));
//         nums--;
//     }
//     for (auto& f : tasks)
//         EXPECT_NO_THROW(f.resultFuture.get());
// }

TEST(rpc_test, test_echo_proxy_mutithread) {
    std::vector<Fundamental::ThreadPoolTaskToken<void>> tasks;
    auto nums      = 40;
    auto task_func = []() {
        auto client=rpc_client::make_shared();
        // client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName,
        // kProxyServiceField,
        //                                                                         kProxyServiceToken));
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        std::size_t cnt  = 5;
        std::string base = "msg ";
        if (!r) return;
        while (cnt != 0) {
            auto str = base + std::to_string(cnt);
            auto ret = client->call<std::string>("echo", str);
            EXPECT_EQ(str, ret);
            --cnt;
        }
    };
    while (nums > 0) {
        tasks.emplace_back(s_test_pool.Enqueue(task_func));
        nums--;
    }
    for (auto& f : tasks)
        EXPECT_NO_THROW(f.resultFuture.get());
}
#if 0
TEST(rpc_test, test_connect) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    auto client=rpc_client::make_shared();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
}

TEST(rpc_test, test_add) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");

        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::int32_t op1 = 1;
        std::int32_t op2 = 2;
        {
            auto result = client->call<std::int32_t>("add", op1, op2);
            EXPECT_EQ(op1 + op2, result);
        }

        {
            auto result = client->call<2000, std::int32_t>("add", op2, op1);
            EXPECT_EQ(op1 + op2, result);
        }
        // test return value type not matched
        EXPECT_THROW((client->call<2000, std::string>("add", op2, op1)), std::invalid_argument);
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}

TEST(rpc_test, test_translate) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }

        auto result = client->call<std::string>("translate", "hello");
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
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        client->call("hello", "purecpp");
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}

TEST(rpc_test, test_get_person_name) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    try {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::string name = "tom";
        auto result      = client->call<std::string>("get_person_name", person { 1, name, 20 });
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
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        auto result = client->call<50, person>("get_person");
        EXPECT_EQ("tom", result.name);
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}

TEST(rpc_test, test_async_client) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    auto client=rpc_client::make_shared("127.0.0.1", "9000");
    bool r = client->connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    client->set_error_callback([](asio::error_code ec) { std::cout << ec.message() << std::endl; });

    auto f = client->async_call("get_person");

    EXPECT_EQ("tom", f.get().as<person>().name);
    auto fu = client->async_call("hello", "purecpp");
    fu.get().as(); // no return
}

static std::vector<std::uint8_t> s_file_data(1024 * 1024, 'a');
TEST(rpc_test, test_upload) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    auto client=rpc_client::make_shared("127.0.0.1", "9000");
    bool r = client->connect(1);
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
        auto f = client->async_call("upload", "test", conent);
        EXPECT_NO_THROW((f.get().as()));
    }
    {
        auto f = client->async_call("upload", "test1", conent);
        EXPECT_NO_THROW((f.get().as()));
    }
}

TEST(rpc_test, test_download) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    auto client=rpc_client::make_shared("127.0.0.1", "9000");
    bool r = client->connect(1);
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    auto f       = client->async_call("download", "test");
    auto content = f.get().as<std::string>();
    EXPECT_TRUE(s_file_data.size() == content.size() &&
                ::memcmp(s_file_data.data(), content.data(), content.size()) == 0);
}

TEST(rpc_test, test_echo) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    auto client=rpc_client::make_shared("127.0.0.1", "9000");
    bool r = client->connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    {
        dummy1 d1 { 42, "test" };
        auto result = client->call<dummy1>("get_dummy", d1);
        EXPECT_TRUE(d1.id == result.id);
        EXPECT_TRUE(d1.str == result.str);
    }

    {
        auto result = client->call<std::string>("echo", "test");
        EXPECT_EQ(result, "test");
    }

    {
        auto result = client->call<std::string>("delay_echo", "test", 50);
        EXPECT_EQ(result, "test");
    }
}

TEST(rpc_test, test_call_with_timeout) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    auto client=rpc_client::make_shared();
    client->async_connect("127.0.0.1", "9000");

    try {
        auto result = client->call<50, person>("get_person");
        std::cout << result.name << std::endl;
        client->close();
        [[maybe_unused]] bool r = client->connect();
        EXPECT_TRUE(r);
        result = client->call<50, person>("get_person");
        std::cout << result.name << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "test_call_with_timeout:throw " << ex.what() << std::endl;
        EXPECT_TRUE(false);
    }
}

TEST(rpc_test, test_callback) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 200); });
    auto client=rpc_client::make_shared();
    client->enable_auto_reconnect();
    client->enable_timeout_check();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r);
    std::atomic<std::size_t> count    = 200;
    static std::atomic_bool is_failed = false;
    Fundamental::Application::Instance().exitStarted.Connect([&]() { is_failed.exchange(true); });
    for (size_t i = 0; i < 100; i++) {
        std::string test = "test" + std::to_string(i + 1);
        // set timeout 100ms
        FDEBUGS << "post echo:" << test;
        client->async_call("delay_echo", test, 50).async_wait([&](const asio::error_code& ec, string_view data) {
            Fundamental::ScopeGuard g([&]() { --count; });
            if (ec) {
                FINFOS << "delay_echo timeout:" << ec.value() << " " << ec.message();
                is_failed.exchange(true);
                return;
            }

            if (has_error(data)) {
                is_failed.exchange(true);
                FINFOS << "delay_echo error msg:" << get_error_msg(data);
            } else {
                auto str = get_result<std::string>(data);
                FWARNS << "delay_echo " << str;
            }
        });

        std::string test1 = "test" + std::to_string(i + 2);
        FDEBUGS << "post delay_echo:" << test1;
        // zero means no timeout check, no param means using default timeout(5s)
        client->async_call("echo", test1).async_wait([&](const asio::error_code& ec, string_view data) {
            Fundamental::ScopeGuard g([&]() { --count; });
            if (ec) {
                FINFOS << "echo timeout:" << ec.value() << " " << ec.message();
                is_failed.exchange(true);
                return;
            }

            if (has_error(data)) {
                is_failed.exchange(true);
                FINFOS << "echo error msg:" << get_error_msg(data);
            } else {
                auto str = get_result<std::string>(data);
                FWARNS << "echo " << str;
            }
        });
    }
    while (count.load() != 0 && !is_failed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(!is_failed);
}

TEST(rpc_test, test_proxy) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });

    auto client=rpc_client::make_shared("127.0.0.1", "9000");
    client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName, kProxyServiceField,
                                                                            kProxyServiceToken));
    bool r = client->connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    {
        dummy1 d1 { 42, "test" };
        auto result = client->call<dummy1>("get_dummy", d1);
        EXPECT_TRUE(d1.id == result.id);
        EXPECT_TRUE(d1.str == result.str);
    }

    {
        auto result = client->call<std::string>("echo", "test");
        EXPECT_EQ(result, "test");
    }
}
TEST(rpc_test, test_auto_reconnect) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 50); });

    try {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        client->enable_auto_reconnect();
        client->set_reconnect_delay(10);
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::int32_t cnt = 3;
        while (cnt > 0) {
            --cnt;
            try {
                client->call<void>("auto_disconnect", cnt);
            } catch (...) {
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}
TEST(rpc_test, test_sub1) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    static bool success = true;
    Fundamental::Application::Instance().exitStarted.Connect([&]() { success = (false); });
    do {
        auto client=rpc_client::make_shared();
        client->enable_auto_reconnect();
        client->enable_timeout_check();
        bool r = client->connect("127.0.0.1", "9000");
        if (!r) {
            success = false;
            break;
        }
        std::atomic<std::size_t> target_count = 0;
        client
            .subscribe("key",
                       [&](string_view data) {
                           msgpack_codec codec;
                           try {
                               auto msg = codec.unpack<std::string>(data.data(), data.size());
                               std::cout << "key_1:" << msg << "\n";
                               target_count++;
                           } catch (const std::exception& e) {
                               std::cerr << e.what() << '\n';
                               success = false;
                           }
                       })
            .get();
        client
            .subscribe("key_p",
                       [&](string_view data) {
                           msgpack_codec codec;
                           try {
                               auto msg = codec.unpack<person>(data.data(), data.size());
                               std::cout << "key_p:" << msg.name << "\n";
                               target_count++;
                           } catch (const std::exception& e) {
                               std::cerr << e.what() << '\n';
                               success = false;
                           }
                       })
            .get();
        rpc_client client2;
        client2.enable_auto_reconnect();
        client2.enable_timeout_check();
        r = client2.connect("127.0.0.1", "9000");
        if (!r) {
            success = false;
            break;
        }

        client2
            .subscribe("key",
                       [&](string_view data) {
                           msgpack_codec codec;
                           try {
                               auto msg = codec.unpack<std::string>(data.data(), data.size());
                               std::cout << "key2:" << msg << "\n";
                               target_count++;
                               if (target_count.load() > 4) {
                                   client2.unsubscribe("key");
                               }
                           } catch (const std::exception& e) {
                               success = false;
                               std::cerr << e.what() << '\n';
                           }
                       })
            .get();

        rpc_client client3;
        client3.enable_auto_reconnect();
        client3.enable_timeout_check();
        r = client3.connect("127.0.0.1", "9000");
        if (!r) {
            success = false;
            break;
        }

        person p { 10, "jack_client", 21 };
        client3.publish("key", "publish msg from client").get();
        while (success && target_count.load() < 10)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (0);
    EXPECT_EQ(success, true);
}

TEST(rpc_test, basice_rpc_stream_test) {
    auto client=rpc_client::make_shared();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
    auto ptr = client->upgrade_to_stream("test_stream");
    EXPECT_TRUE(ptr != nullptr);
}

TEST(rpc_test, basice_rpc_stream_read_write) {
    auto client=rpc_client::make_shared();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
    auto stream = client->upgrade_to_stream("test_stream");
    EXPECT_TRUE(stream != nullptr);
    person p;
    p.id            = 0;
    p.age           = 10;
    p.name          = "jack ";
    std::size_t cnt = 0;
    while (cnt < 5) {
        p.id = cnt;
        p.age += cnt;
        p.name += std::to_string(cnt);
        EXPECT_TRUE(stream->Write(p));
        ++cnt;
    }
    EXPECT_TRUE(stream->WriteDone());
    std::size_t read_cnt = 0;
    while (stream->Read(p, 0)) {
        ++read_cnt;
        FINFO("id:{},age:{},name:{}", p.id, p.age, p.name);
    }
    EXPECT_TRUE(read_cnt == cnt);
    EXPECT_TRUE(!stream->Finish(0));
}

TEST(rpc_test, basice_rpc_stream_read_only) {
    auto client=rpc_client::make_shared();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
    auto stream = client->upgrade_to_stream("test_read_stream");
    EXPECT_TRUE(stream != nullptr);
    person p;
    p.id            = 0;
    p.age           = 10;
    p.name          = "jack ";
    std::size_t cnt = 0;
    // test delay read
    EXPECT_TRUE(!stream->Read(p, 10));
    while (stream->Read(p, 0)) {
        FINFO("id:{},age:{},name:{}", p.id, p.age, p.name);
    }
    while (cnt < 2) {
        p.id = cnt;
        p.age += cnt;
        p.name += std::to_string(cnt);
        EXPECT_TRUE(stream->Write(p));
        ++cnt;
    }
    EXPECT_TRUE(stream->WriteDone());

    EXPECT_TRUE(!stream->Finish(0));
}

TEST(rpc_test, basice_rpc_stream_write_only) {
    auto client=rpc_client::make_shared();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
    auto stream = client->upgrade_to_stream("test_write_stream");
    EXPECT_TRUE(stream != nullptr);
    person p;
    p.id            = 0;
    p.age           = 10;
    p.name          = "jack ";
    std::size_t cnt = 0;
    while (cnt < 2) {
        p.id = cnt;
        p.age += cnt;
        p.name += std::to_string(cnt);
        EXPECT_TRUE(stream->Write(p));
        ++cnt;
    }
    EXPECT_TRUE(stream->WriteDone());
    while (stream->Read(p, 0)) {
        FINFO("id:{},age:{},name:{}", p.id, p.age, p.name);
    }
    EXPECT_TRUE(!stream->Finish(0));
}

TEST(rpc_test, test_broken_rpc_stream) {
    auto client=rpc_client::make_shared();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
    auto stream = client->upgrade_to_stream("test_broken_stream");
    EXPECT_TRUE(stream != nullptr);
    person p;
    std::size_t cnt = 0;
    while (cnt < 2) {
        stream->Write(p);
        ++cnt;
    }
    stream->WriteDone();
    while (stream->Read(p, 0)) {
    }
    EXPECT_TRUE(stream->Finish(0));
}

TEST(rpc_test, test_echo_stream) {
    auto client=rpc_client::make_shared();
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
    auto stream = client->upgrade_to_stream("test_echo_stream");
    EXPECT_TRUE(stream != nullptr);
    std::size_t cnt  = 10;
    std::string base = "msg ";
    while (cnt != 0) {
        EXPECT_TRUE(stream->Write(base + std::to_string(cnt)));
        --cnt;
        std::string tmp;
        EXPECT_TRUE(stream->Read(tmp));
        FINFO("echo msg:{}", tmp);
    }
    EXPECT_TRUE(stream->WriteDone());
    EXPECT_TRUE(!stream->Finish(0));
}
TEST(rpc_test, test_obj_echo) {
    auto client=rpc_client::make_shared("127.0.0.1", "9000");
    client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName, kProxyServiceField,
                                                                            kProxyServiceToken));
    bool r = client->connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }
    std::int32_t c = 0;
    std::string str;
    str = std::string(781, 'a');
    try {
        auto ret = client->call<10000, std::string>("echo", str);
        if (str != ret) {
            FERR("error finished {}", c);
        }
    } catch (const std::exception& e) {
        FERR("exception {}->{}", c, e.what());
    }
    std::int32_t max_call_times = 20;
    while (c < max_call_times) {
        str.push_back('a');
        try {
            auto ret = client->call<10000, std::string>("echo", str);
            if (str != ret) {
                FERR("error finished {}", c);
                break;
            }
        } catch (const std::exception& e) {
            FERR("exception {}->{}", c, e.what());
            break;
        }
        ++c;
    }
    EXPECT_TRUE(c >= max_call_times);
}

TEST(rpc_test, test_timeout_echo) {
    auto client=rpc_client::make_shared("127.0.0.1", "9000");
    bool r = client->connect();
    client->enable_timeout_check(true, 1);
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    {
        auto test_str = std::string(1024 * 1024 * 10, 'a');
        auto result   = client->async_timeout_call("delay_echo", 200, test_str, 400);
        EXPECT_ANY_THROW(result.get());
    }
}

TEST(rpc_test, test_echo_stream_mutithread) {
    std::vector<Fundamental::ThreadPoolTaskToken<void>> tasks;
    auto nums      = 40;
    auto task_func = []() {
        auto client=rpc_client::make_shared();

        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_echo_stream");
        EXPECT_TRUE(stream != nullptr);
        std::size_t cnt  = 5;
        std::string base = "msg ";
        while (cnt != 0) {
            EXPECT_TRUE(stream->Write(base + std::to_string(cnt)));
            --cnt;
            std::string tmp;
            EXPECT_TRUE(stream->Read(tmp));
            FINFO("mutithread echo msg:{}", tmp);
        }
        EXPECT_TRUE(stream->WriteDone());
        EXPECT_TRUE(!stream->Finish(0));
    };
    while (nums > 0) {
        tasks.emplace_back(s_test_pool.Enqueue(task_func));
        nums--;
    }
    for (auto& f : tasks)
        EXPECT_NO_THROW(f.resultFuture.get());
}

TEST(rpc_test, test_echo_stream_proxy_mutithread) {
    std::vector<Fundamental::ThreadPoolTaskToken<void>> tasks;
    auto nums      = 40;
    auto task_func = []() {
        auto client=rpc_client::make_shared();
        client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName, kProxyServiceField,
                                                                                kProxyServiceToken));
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_echo_stream");
        EXPECT_TRUE(stream != nullptr);
        std::size_t cnt  = 5;
        std::string base = "msg ";
        while (cnt != 0) {
            EXPECT_TRUE(stream->Write(base + std::to_string(cnt)));
            --cnt;
            std::string tmp;
            EXPECT_TRUE(stream->Read(tmp));
            FINFO("mutithread echo msg:{}", tmp);
        }
        EXPECT_TRUE(stream->WriteDone());
        EXPECT_TRUE(!stream->Finish(0));
    };
    while (nums > 0) {
        tasks.emplace_back(s_test_pool.Enqueue(task_func));
        nums--;
    }
    for (auto& f : tasks)
        EXPECT_NO_THROW(f.resultFuture.get());
}

TEST(rpc_test, test_control_stream) {
    {
        auto client=rpc_client::make_shared();
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_control_stream");
        EXPECT_TRUE(stream != nullptr);
        std::string base = "msg ";
        DelayControlStream echo_request;
        echo_request.cmd           = "echo";
        echo_request.msg           = "echo msg";
        echo_request.process_delay = 200;
        DelayControlStream set_request;
        set_request.cmd           = "set";
        set_request.process_delay = 100; // 5s
        set_request.msg           = "";
        stream->Write(set_request);
        // stream->EnableAutoHeartBeat(true,30);
        EXPECT_TRUE(stream->Write(echo_request));
        EXPECT_TRUE(stream->WriteDone());
        std::string echo_msg;
        EXPECT_FALSE(stream->Read(echo_msg, 0));
        // error code !=0
        EXPECT_TRUE(stream->Finish(0));
    }
    {
        auto client=rpc_client::make_shared();
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_control_stream");
        EXPECT_TRUE(stream != nullptr);
        std::string base = "msg ";
        DelayControlStream echo_request;
        echo_request.cmd           = "echo";
        echo_request.msg           = "echo msg";
        echo_request.process_delay = 200;
        DelayControlStream set_request;
        set_request.cmd           = "set";
        set_request.process_delay = 100; // 5s
        set_request.msg           = "";
        stream->Write(set_request);
        stream->EnableAutoHeartBeat(true, 30);
        EXPECT_TRUE(stream->Write(echo_request));
        EXPECT_TRUE(stream->WriteDone());
        std::string echo_msg;
        EXPECT_TRUE(stream->Read(echo_msg, 0));
        // error code !=0
        EXPECT_FALSE(stream->Finish(0));
    }
    {//test read some
        auto client=rpc_client::make_shared();
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_control_stream");
        EXPECT_TRUE(stream != nullptr);
        std::string base = "msg ";
        DelayControlStream echo_request;
        echo_request.cmd           = "echo";
        echo_request.msg           = std::string(1024*1024*10,'a');
        echo_request.process_delay = 4;
        DelayControlStream set_request;
        set_request.cmd           = "set";
        set_request.process_delay = 40; 
        set_request.msg           = "";
        stream->Write(set_request);
        EXPECT_TRUE(stream->Write(echo_request));
        EXPECT_TRUE(stream->WriteDone());
        std::string echo_msg;
        EXPECT_TRUE(stream->Read(echo_msg, 0));
        // error code !=0
        EXPECT_FALSE(stream->Finish(0));
    }
    {//test read some
        auto client=rpc_client::make_shared();
        client->config_tcp_no_delay();
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_control_stream");
        stream->EnableAutoHeartBeat(true, 5);
        EXPECT_TRUE(stream != nullptr);
        std::string base = "msg ";
        DelayControlStream echo_request;
        echo_request.cmd           = "echo";
        echo_request.msg           = std::string(1024*1024*10,'a');
        echo_request.process_delay = 4;
        DelayControlStream set_request;
        set_request.cmd           = "set";
        set_request.process_delay = 6; 
        set_request.msg           = "";
        stream->Write(set_request);
        EXPECT_TRUE(stream->Write(echo_request));
        EXPECT_TRUE(stream->WriteDone());
        std::string echo_msg;
        EXPECT_TRUE(stream->Read(echo_msg, 0));
        // error code !=0
        EXPECT_FALSE(stream->Finish(0));
    }
        {//test read some
        auto client=rpc_client::make_shared();
        client->config_tcp_no_delay();
        [[maybe_unused]] bool r = client->connect("::", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_control_stream");
        EXPECT_TRUE(stream != nullptr);
        std::string base = "msg ";
        DelayControlStream echo_request;
        echo_request.cmd           = "echo";
        echo_request.msg           = std::string(1024*1024*10,'a');
        echo_request.process_delay = 4;
        DelayControlStream set_request;
        set_request.cmd           = "set";
        set_request.process_delay = 6; 
        set_request.msg           = "";
        stream->Write(set_request);
        stream->Write(echo_request);
        stream->WriteDone();
        std::string echo_msg;
        EXPECT_FALSE(stream->Read(echo_msg, 0));
        // error code !=0
        EXPECT_TRUE(stream->Finish(0));
    }
}

#endif

#if !defined(RPC_DISABLE_SSL) && 0
TEST(rpc_test, test_ssl) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });

    try {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        client->enable_ssl("server.crt");
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::size_t cnt = 10;
        while (cnt > 0) {
            --cnt;
            client->call<std::string>("echo", std::to_string(cnt) + "test");
        }
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }

    std::cout << "finish ssl" << std::endl;
    try {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }
        std::size_t cnt = 10;
        while (cnt > 0) {
            --cnt;
            client->call<std::string>("echo", std::to_string(cnt) + "test nossl");
        }
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
    std::cout << "finish no ssl" << std::endl;
}
TEST(rpc_test, test_ssl_proxy) {
    Fundamental::Timer check_timer;
    Fundamental::ScopeGuard check_guard(
        [&]() { EXPECT_LE(check_timer.GetDuration<Fundamental::Timer::TimeScale::Millisecond>(), 100); });
    {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        client->enable_ssl("server.crt");
        client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName, kProxyServiceField,
                                                                                kProxyServiceToken));
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }

        {
            dummy1 d1 { 42, "test" };
            auto result = client->call<dummy1>("get_dummy", d1);
            EXPECT_TRUE(d1.id == result.id);
            EXPECT_TRUE(d1.str == result.str);
        }

        {
            auto result = client->call<std::string>("echo", "test");
            EXPECT_EQ(result, "test");
        }
    }
    {
        auto client=rpc_client::make_shared("127.0.0.1", "9000");
        client->enable_ssl("", network::rpc_service::rpc_client_ssl_level_optional);
        client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName, kProxyServiceField,
                                                                                kProxyServiceToken));
        bool r = client->connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }

        {
            dummy1 d1 { 42, "test" };
            auto result = client->call<dummy1>("get_dummy", d1);
            EXPECT_TRUE(d1.id == result.id);
            EXPECT_TRUE(d1.str == result.str);
        }

        {
            auto result = client->call<std::string>("echo", "test");
            EXPECT_EQ(result, "test");
        }
    }
}
TEST(rpc_test, test_ssl_proxy_echo_stream) {
    auto client=rpc_client::make_shared();
    client->enable_ssl("server.crt");
    client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName, kProxyServiceField,
                                                                            kProxyServiceToken));
    [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
    EXPECT_TRUE(r && client->has_connected());
    auto stream = client->upgrade_to_stream("test_echo_stream");
    EXPECT_TRUE(stream != nullptr);
    std::size_t cnt  = 5;
    std::string base = "msg ";
    while (cnt != 0) {
        EXPECT_TRUE(stream->Write(base + std::to_string(cnt)));
        --cnt;
        std::string tmp;
        EXPECT_TRUE(stream->Read(tmp));
        FINFO("ssl/proxy echo msg:{}", tmp);
    }
    EXPECT_TRUE(stream->WriteDone());
    EXPECT_TRUE(!stream->Finish(0));
}
TEST(rpc_test, test_ssl_concept) {
    // sudo tcpdump -i any -n -vv -X port 9000
    {
        auto client=rpc_client::make_shared();
        client->enable_ssl("server.crt", network::rpc_service::rpc_client_ssl_level_optional);
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_echo_stream");
        EXPECT_TRUE(stream != nullptr);
        std::string base = "1111";
        EXPECT_TRUE(stream->Write(base));
        EXPECT_TRUE(stream->WriteDone());
        EXPECT_TRUE(!stream->Finish(0));
    }
    {
        auto client=rpc_client::make_shared();
        client->enable_ssl("server.crt_none", network::rpc_service::rpc_client_ssl_level_optional);
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_echo_stream");
        EXPECT_TRUE(stream != nullptr);
        std::string base = "1111";
        EXPECT_TRUE(stream->Write(base));
        EXPECT_TRUE(stream->WriteDone());
        EXPECT_TRUE(!stream->Finish(0));
    }
    {
        auto client=rpc_client::make_shared();
        [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
        EXPECT_TRUE(r && client->has_connected());
        auto stream = client->upgrade_to_stream("test_echo_stream");
        EXPECT_TRUE(stream != nullptr);
        std::string base = "1111";
        EXPECT_TRUE(stream->Write(base));
        EXPECT_TRUE(stream->WriteDone());
        EXPECT_TRUE(!stream->Finish(0));
    }
}
#endif
#if 1

// TEST(rpc_test, test_ssl_proxy_echo_stream_mutithread) {
//     std::vector<Fundamental::ThreadPoolTaskToken<void>> tasks;
//     auto nums      = s_test_pool.Count();
//     auto task_func = []() {
//         auto client=rpc_client::make_shared();
//         //client->enable_ssl("server.crt");
//         client->set_proxy(network::rpc_service::CustomRpcProxy::make_shared(kProxyServiceName,
//         kProxyServiceField,
//                                                                                 kProxyServiceToken));
//         [[maybe_unused]] bool r = client->connect("127.0.0.1", "9000");
//         EXPECT_TRUE(r && client->has_connected());
//         auto stream = client->upgrade_to_stream("test_echo_stream");
//         EXPECT_TRUE(stream != nullptr);
//         std::size_t cnt  = 5;
//         std::string base = "msg ";
//         while (cnt != 0) {
//             EXPECT_TRUE(stream->Write(base + std::to_string(cnt)));
//             --cnt;
//             std::string tmp;
//             EXPECT_TRUE(stream->Read(tmp));
//             FINFO("ssl/proxy echo msg:{}", tmp);
//         }
//         EXPECT_TRUE(stream->WriteDone());
//         EXPECT_TRUE(!stream->Finish(0));
//     };
//     while (nums > 0) {
//         tasks.emplace_back(s_test_pool.Enqueue(task_func));
//         nums--;
//     }
//     for (auto& f : tasks)
//         EXPECT_NO_THROW(f.resultFuture.get());
// }
#endif

int main(int argc, char** argv) {
    int mode = 0;
    if (argc > 1) mode = std::stoi(argv[1]);
    Fundamental::fs::SwitchToProgramDir(argv[0]);
    Fundamental::Logger::LoggerInitOptions options;
    options.minimumLevel         = Fundamental::LogLevel::debug;
    options.logFormat            = "%^[%L]%H:%M:%S.%e%$[%t] %v ";
    options.logOutputProgramName = "test";
    options.logOutputPath        = "output";
    Fundamental::Logger::Initialize(std::move(options));
    s_test_pool.Spawn(4);
    if (mode == 0) {
        ::testing::InitGoogleTest(&argc, argv);
        run_server();
        auto ret = RUN_ALL_TESTS();
        exit_server();
        FINFO("finish all test");
        return ret;
    } else if (mode == 1) {
        std::promise<void> sync_p;
        server_task(sync_p);
        sync_p.get_future().get();
        exit_server();
    } else {
        ::testing::InitGoogleTest(&argc, argv);
        network::io_context_pool::s_excutorNums = 10;
        network::io_context_pool::Instance().start();

        return RUN_ALL_TESTS();
    }
    return 0;
}