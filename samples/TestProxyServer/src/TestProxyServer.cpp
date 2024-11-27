

#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "network/server/io_context_pool.hpp"
#include "network/services/proxy_server/agent_service/agent_client.hpp"
#include "network/services/proxy_server/proxy_connection.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
int main(int argc, char* argv[])
{
    auto p = ::getenv("CLIENT");
    if (!p)
    {
        FINFO("you can set env \"CLIENT\" to perform as a client role");
        try
        {
            // Check command line arguments.
            if (argc != 4)
            {
                std::cerr << "Usage: echo_server <address> <port> <threads>\n";
                std::cerr << "  For IPv4, try:\n";
                std::cerr << "    receiver 0.0.0.0 80 1 .\n";
                std::cerr << "  For IPv6, try:\n";
                std::cerr << "    receiver 0::0 80 1 .\n";
                return 1;
            }
            network::io_context_pool::s_excutorNums = std::stoi(argv[3]);
            network::io_context_pool::Instance().start();
            // Initialise the server.
            network::proxy::ProxyServer s(argv[1], argv[2]);
            s.Start();
            Fundamental::Application::Instance().Loop();
        }
        catch (std::exception& e)
        {
            FERR("exception: {}", e.what());
        }

        return 0;
    }
    else
    {
        using asio::ip::tcp;
        try
        {
            if (argc != 5)
            {
                std::cerr << "Usage: echo_client <host> <port> <mode> <section>\n";
                return 1;
            }

            network::io_context_pool::s_excutorNums = 2;
            network::io_context_pool::Instance().start();
            network::proxy::rpc::AgentClient client;
            std::string mode    = argv[3];
            std::string section = argv[4];
            if (mode == "update")
            {
                auto maxSize = 1024;
                char line[maxSize - 1];
                std::string msg;
                while (std::cin.getline(line, maxSize + 1))
                {
                    auto len = std::strlen(line);
                    if (0 == len)
                        continue;
                    msg = std::string(line, len);
                    break;
                }
                network::proxy::rpc::AgentUpdateContext context;
                context.host            = argv[1];
                context.service         = argv[2];
                context.request.id      = "test";
                context.request.section = section;
                context.request.data    = msg;
                using namespace network::proxy::rpc;
                context.cb = [](bool bSuccess, AgentClientToken token, AgentResponse&& res) {
                    FINFO("update success:{} token:{}", bSuccess, token);
                    FINFO("code:{}", res.code);
                    Fundamental::Application::Instance().Exit();
                };
                auto token = client.Update(context);
                FINFO("create task token:{}", token);
            }
            else
            {
                network::proxy::rpc::AgentQueryContext context;
                context.host            = argv[1];
                context.service         = argv[2];
                context.request.id      = "test";
                context.request.section = section;
                using namespace network::proxy::rpc;
                context.cb = [](bool bSuccess, AgentClientToken token, AgentResponse&& res) {
                    FINFO("update success:{} token:{}", bSuccess, token);
                    FINFO("code:{} msg:{} size:{} result:{}", res.code, res.msg, res.data.size(),
                          std::string(res.data.data(), res.data.data() + res.data.size()));
                    Fundamental::Application::Instance().Exit();
                };
                token=client.Query(context);
                FINFO("create task token:{}", token);
            }
            Fundamental::Application::Instance().Loop();
        }
        catch (std::exception& e)
        {
            FERR("exception: {}", e.what());
        }

        return 0;
    }
}
