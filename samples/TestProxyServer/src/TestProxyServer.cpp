

#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "network/server/io_context_pool.hpp"
#include "network/services/proxy_server/agent_service/agent_client.hpp"
#include "network/services/proxy_server/proxy_connection.hpp"
#include "network/services/proxy_server/traffic_proxy_service/traffic_proxy_manager.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <utility>
void InitTrafficProxyManager();
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
                std::cerr << "    receiver 0.0.0.0 4885 1 \n";
                std::cerr << "  For IPv6, try:\n";
                std::cerr << "    receiver 0::0 4885 1 \n";
                return 1;
            }
            network::io_context_pool::s_excutorNums = std::stoi(argv[3]);
            network::io_context_pool::Instance().start();
            InitTrafficProxyManager();
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
                std::random_device rd;
                std::int32_t len = (rd() % 2000) + 100;
                std::string msg(len, 'c');
                network::proxy::rpc::AgentUpdateContext context;
                context.host            = argv[1];
                context.service         = argv[2];
                context.request.id      = "test";
                context.request.section = section;
                context.request.data    = msg;
                FDEBUG("id:{} section:{} msg:{}", context.request.id.Dump(),
                       context.request.section.Dump(),
                       context.request.data.Dump());
                FDEBUG("str id:{} section:{} msg:{}", context.request.id.DumpAscii(),
                       context.request.section.DumpAscii(),
                       context.request.data.DumpAscii());
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
                          Fundamental::Utils::BufferDumpAscii(res.data.data(), res.data.size()));
                    // decode data
                    network::proxy::AgentQueryResponse response;
                    Fundamental::BufferReader<network::proxy::ProxySizeType> reader;
                    reader.SetBuffer(res.data.data(), res.data.size());
                    try
                    {
                        reader.ReadValue(&response.timestamp);
                        reader.ReadRawMemory(response.data);
                        FINFO("time:{}  data:{}", Fundamental::Timer::ToTimeStr(response.timestamp),
                              response.data.ToString());
                    }
                    catch (const std::exception&)
                    {
                    }

                    Fundamental::Application::Instance().Exit();
                };
                auto token = client.Query(context);
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

void InitTrafficProxyManager()
{
    using namespace network::proxy;
    auto& manager = TrafficProxyManager::Instance();
    { // add http proxy
        TrafficProxyHostInfo host;
        host.token = "test_http_token";
        {
            TrafficProxyHost hostRecord;
            hostRecord.host    = "www.baidu.com";
            hostRecord.service = "http";
            host.hosts.emplace(TrafficProxyDataType("www.baidu.com"), std::move(hostRecord));
        }
        {
            TrafficProxyHost hostRecord;
            hostRecord.host    = "github.com";
            hostRecord.service = "http";
            host.hosts.emplace(TrafficProxyDataType("github.com"), std::move(hostRecord));
        }
        manager.UpdateTrafficProxyHostInfo(TrafficProxyDataType("test_http"), std::move(host));
    }
    { // add normal tcp
        TrafficProxyHostInfo host;
        host.token = "test_tcp_token";
        {
            TrafficProxyHost hostRecord;
            hostRecord.host    = "127.0.0.1";
            hostRecord.service = "54885";
            host.hosts.emplace(TrafficProxyDataType("internal"), std::move(hostRecord));
        }
        {
            TrafficProxyHost hostRecord;
            hostRecord.host    = "127.0.0.1";
            hostRecord.service = "54886";
            host.hosts.emplace(TrafficProxyDataType(TrafficProxyManager::kDefaultFieldName ), std::move(hostRecord));
        }
        manager.UpdateTrafficProxyHostInfo(TrafficProxyDataType("test_tcp"), std::move(host));
    }
}
