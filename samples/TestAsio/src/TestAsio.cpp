

#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "network/server/io_context_pool.hpp"
#include "network/services/echo/echo_client.hpp"
#include "network/services/echo/echo_connection.hpp"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_codec.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
int main(int argc, char* argv[]) {
    auto p = ::getenv("CLIENT");
    if (!p) {
        FINFO("you can set env \"CLIENT\" to perform as a client role");
        try {
            // Check command line arguments.
            if (argc != 4) {
                std::cerr << "Usage: echo_server <address> <port> <threads>\n";
                std::cerr << "  For IPv4, try:\n";
                std::cerr << "    receiver 0.0.0.0 54885 1 .\n";
                std::cerr << "  For IPv6, try:\n";
                std::cerr << "    receiver 0::0 54885 1 .\n";
                return 1;
            }
            network::io_context_pool::s_excutorNums = std::stoi(argv[3]);
            network::io_context_pool::Instance().start();
            Fundamental::Application::Instance().exitStarted.Connect(
                [&]() { network::io_context_pool::Instance().stop(); });
            network::io_context_pool::Instance().notify_sys_signal.Connect(
                [](std::error_code code, std::int32_t signo) { Fundamental::Application::Instance().Exit(); });
            // Initialise the server.
            using asio::ip::tcp;
            tcp::resolver resolver(network::io_context_pool::Instance().get_io_context());
            auto endpoints = resolver.resolve(argv[1], argv[2]);
            if (endpoints.empty()) {
                FERR("resolve failed");
                return 1;
            }
            network::echo::EchoServer s(*endpoints.begin());

            s.Start();
            Fundamental::Application::Instance().Loop();
        } catch (std::exception& e) {
            FERR("exception: {}", e.what());
        }

        return 0;
    } else {
        using asio::ip::tcp;
        try {
            if (argc != 3) {
                std::cerr << "Usage: echo_client <host> <port>\n";
                return 1;
            }

            asio::io_context io_context;
            std::string host = argv[1];
            std::string port = argv[2];
            tcp::resolver resolver(io_context);

            auto endpoints = resolver.resolve(host, port);

            network::echo::echo_client c(io_context, endpoints);
            if (::getenv("USE_TRAFFIC_PROXY")) {
                network::proxy::ProxyFrame frame_;
                network::proxy::TrafficProxyRequest request;
                request.proxyServiceName = "test_tcp";
                request.field            = ::getenv("USE_TRAFFIC_PROXY");
                request.token            = "test_tcp_token";
                network::proxy::TrafficEncoder::EncodeProxyFrame(frame_, request);
                auto buffers_ = frame_.ToAsioBuffers();
                std::vector<std::uint8_t> f;
                for (auto& i : buffers_)
                    f.insert(f.end(), (const std::uint8_t*)i.data(), (const std::uint8_t*)i.data() + i.size());
                c.SetTrafficProxyData(std::move(f));
            }
            std::thread t([&io_context]() { io_context.run(); });
            auto maxSize = 1024;
            char line[maxSize - 1];
            bool isRunning = true;
            c.wait_connect();
            while (c.connected() && isRunning && std::cin.getline(line, maxSize + 1)) {
                network::echo::EchoMsg msg;
                auto len = std::strlen(line);
                if (0 == len) continue;
                FINFO("read size:{}", len);
                if (strncmp(line, "#", 1) == 0) {
                    FWARN("disconnected request");
                    len       = 0;
                    isRunning = false;
                }
                auto bufLen = len + network::echo::EchoMsg::kTimeStampSize;
                msg.msg.resize(bufLen);
                msg.header.v = htobe32(bufLen);
                msg.TimeStamp() =
                    htobe64((Fundamental::Timer::GetTimeNow<std::chrono::seconds, std::chrono::system_clock>()));
                std::memcpy(msg.Data(), line, len);
                c.write(msg);
            }
            t.join();
            c.close();
        } catch (std::exception& e) {
            FERR("exception: {}", e.what());
        }

        return 0;
    }
}
