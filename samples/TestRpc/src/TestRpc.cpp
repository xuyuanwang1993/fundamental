
#include "test_server.h"

#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/log.h"

#include "rpc/basic/rpc_client.hpp"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>

using namespace network;
using namespace network::rpc_service;

TEST(rpc_test, test_add) {
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
#if 0
TEST(rpc_test, test_get_person) {
    try {
        rpc_client client("127.0.0.1", 9000);
        bool r = client.connect();
        if (!r) {
            EXPECT_TRUE(false && "connect timeout");
            return;
        }

        auto result = client.call<person>("get_person");
        EXPECT_EQ("tom", result.name);
    } catch (const std::exception& e) {
        std::cout << __func__ << ":" << e.what() << std::endl;
    }
}

TEST(rpc_test, test_async_client) {
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
#endif
static std::vector<std::uint8_t> s_file_data(1024 * 1024, 'a');
TEST(rpc_test, test_upload) {
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
    rpc_client client("127.0.0.1", 9000);
    bool r = client.connect(1);
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    auto f       = client.async_call<FUTURE>("download", "test");
    auto content = f.get().as<std::string>();
    EXPECT_TRUE(s_file_data.size() == content.size() && ::memcmp(s_file_data.data(), content.data(), content.size()) == 0);
}

TEST(rpc_test, test_echo) {
    rpc_client client("127.0.0.1", 9000);
    bool r = client.connect();
    if (!r) {
        EXPECT_TRUE(false && "connect timeout");
        return;
    }

    {
        dummy1 d1 { 42, "test" };
        auto result = client.call<dummy1>("get_dummy", d1);
        EXPECT_TRUE(d1.id==result.id);
        EXPECT_TRUE(d1.str==result.str);
    }

    {
        auto result = client.call<std::string>("echo", "test");
        EXPECT_EQ(result,"test");
    }

    {
        auto result = client.call<std::string>("delay_echo", "test");
        EXPECT_EQ(result,"test");
    }
}


TEST(rpc_test, test_call_with_timeout) {
    rpc_client client;
    client.async_connect("127.0.0.1", 9000);

    try {
        auto result = client.call<50, person>("get_person");
        std::cout << result.name << std::endl;
        client.close();
        [[maybe_unused]] bool r = client.connect();
        result                  = client.call<50, person>("get_person");
        std::cout << result.name << std::endl;
    } catch (const std::exception& ex) {
        std::cout << ex.what() << std::endl;
    }

    std::string str;
    std::cin >> str;
}
#if 0
TEST(rpc_test, test_connect) {
    rpc_client client;
    client.enable_auto_reconnect(); // automatic reconnect
    client.enable_auto_heartbeat(); // automatic heartbeat
    [[maybe_unused]] bool r = client.connect("127.0.0.1", 9000);
    int count               = 0;
    while (true) {
        if (client.has_connected()) {
            std::cout << "connected ok\n";
            break;
        } else {
            std::cout << "connected failed: " << count++ << "\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
TEST(rpc_test, test_callback) {
    rpc_client client;
    [[maybe_unused]] bool r = client.connect("127.0.0.1", 9000);

    for (size_t i = 0; i < 10; i++) {
        std::string test = "test" + std::to_string(i + 1);
        // set timeout 100ms
        client.async_call<10000>(
            "delay_echo",
            [](const asio::error_code& ec, string_view data) {
                if (ec) {
                    std::cout << ec.value() << " timeout" << std::endl;
                    return;
                }

                auto str = as<std::string>(data);
                std::cout << "delay echo " << str << '\n';
            },
            test);

        std::string test1 = "test" + std::to_string(i + 2);
        // zero means no timeout check, no param means using default timeout(5s)
        client.async_call<0>(
            "echo",
            [](const asio::error_code& ec, string_view data) {
                auto str = as<std::string>(data);
                std::cout << "echo " << str << '\n';
            },
            test1);
    }

    client.run();
}

TEST(rpc_test, test_sub1) {
    rpc_client client;
    client.enable_auto_reconnect();
    client.enable_auto_heartbeat();
    bool r = client.connect("127.0.0.1", 9000);
    if (!r) {
        return;
    }

    client.subscribe("key", [](string_view data) { std::cout << data << "\n"; });

    client.subscribe("key", "048a796c8a3c6a6b7bd1223bf2c8cee05232e927b521984ba417cb2fca6df9d1", [](string_view data) {
        msgpack_codec codec;
        person p = codec.unpack<person>(data.data(), data.size());
        std::cout << p.name << "\n";
    });

    client.subscribe("key1", "048a796c8a3c6a6b7bd1223bf2c8cee05232e927b521984ba417cb2fca6df9d1",
                     [](string_view data) { std::cout << data << "\n"; });

    bool stop = false;
    std::thread thd1([&client, &stop] {
        while (true) {
            try {
                if (client.has_connected()) {
                    int r = client.call<int>("add", 2, 3);
                    std::cout << "add result: " << r << "\n";
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (const std::exception& ex) {
                std::cout << ex.what() << "\n";
            }
        }
    });

    /*rpc_client client1;
    bool r1 = client1.connect("127.0.0.1", 9000);
    if (!r1) {
            return;
    }

    person p{10, "jack", 21};
    client1.publish("key", "hello subscriber");
    client1.publish_by_token("key", "sub_key", p);

    std::thread thd([&client1, p] {
            while (true) {
                    try {
                            client1.publish("key", "hello subscriber");
                            client1.publish_by_token("key", "unique_token", p);
                    }
                    catch (const std::exception& ex) {
                            std::cout << ex.what() << "\n";
                    }
            }
    });
  */

    std::string str;
    std::cin >> str;
}
TEST(rpc_test, test_multiple_thread) {
    std::vector<std::shared_ptr<rpc_client>> cls;
    std::vector<std::shared_ptr<std::thread>> v;
    for (int j = 0; j < 4; ++j) {
        auto client = std::make_shared<rpc_client>();
        cls.push_back(client);
        bool r = client->connect("127.0.0.1", 9000);
        if (!r) {
            return;
        }

        for (size_t i = 0; i < 2; i++) {
            person p { 1, "tom", 20 };
            v.emplace_back(std::make_shared<std::thread>([client] {
                person p { 1, "tom", 20 };
                for (size_t i = 0; i < 1000000; i++) {
                    client->async_call<0>(
                        "get_name",
                        [](const asio::error_code& ec, string_view data) {
                            if (ec) {
                                std::cout << ec.message() << '\n';
                            }
                        },
                        p);

                    // auto future = client->async_call<FUTURE>("get_name", p);
                    // auto status = future.wait_for(std::chrono::seconds(2));
                    // if (status == std::future_status::deferred) {
                    //	std::cout << "deferred\n";
                    //}
                    // else if (status == std::future_status::timeout) {
                    //	std::cout << "timeout\n";
                    //}
                    // else if (status == std::future_status::ready) {
                    //}

                    // client->call<std::string>("get_name", p);
                }
            }));
        }
    }

    std::string str;
    std::cin >> str;
}
TEST(rpc_test, test_threads) {
    rpc_client client;
    bool r = client.connect("127.0.0.1", 9000);
    if (!r) {
        return;
    }

    std::thread thd1([&client] {
        for (size_t i = 0; i < 1000000; i++) {
            auto future = client.async_call<FUTURE>("echo", "test");
            auto status = future.wait_for(std::chrono::seconds(2));
            if (status == std::future_status::timeout) {
                std::cout << "timeout\n";
            } else if (status == std::future_status::ready) {
                std::string content = future.get().as<std::string>();
            }

            std::this_thread::sleep_for(std::chrono::microseconds(2));
        }
        std::cout << "thread2 finished" << '\n';
    });

    std::thread thd2([&client] {
        for (size_t i = 1000000; i < 2 * 1000000; i++) {
            client.async_call(
                "get_int",
                [i](asio::error_code ec, string_view data) {
                    if (ec) {
                        std::cout << ec.message() << '\n';
                        return;
                    }
                    int r = as<int>(data);
                    if (r != i) {
                        std::cout << "error not match" << '\n';
                    }
                },
                i);
            std::this_thread::sleep_for(std::chrono::microseconds(2));
        }

        std::cout << "thread2 finished" << '\n';
    });

    for (size_t i = 2 * 1000000; i < 3 * 1000000; i++) {
        auto future = client.async_call<FUTURE>("echo", "test");
        auto status = future.wait_for(std::chrono::seconds(2));
        if (status == std::future_status::timeout) {
            std::cout << "timeout\n";
        } else if (status == std::future_status::ready) {
            std::string content = future.get().as<std::string>();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(2));
    }
    std::cout << "thread finished" << '\n';

    std::string str;
    std::cin >> str;
}
TEST(rpc_test, benchmark_test) {
    rpc_client client;
    bool r = client.connect("127.0.0.1", 9000);
    if (!r) {
        return;
    }

    for (size_t i = 0; i < 1000000; i++) {
        client.async_call(
            "echo",
            [i](asio::error_code ec, string_view data) {
                if (ec) {
                    return;
                }
            },
            "hello wolrd");
    }

    std::getchar();
    std::cout << "benchmark test finished\n";
}
#endif
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);
    run_server();
    auto ret = RUN_ALL_TESTS();
    exit_server();
    return ret;
}