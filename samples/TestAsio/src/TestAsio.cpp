

#include "fundamental/basic/log.h"
#include "network/services/echo/echo_connection.hpp"
#include "network/services/echo/echo_client.hpp"
#include "fundamental/delay_queue/delay_queue.h"
#include "network/server/io_context_pool.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include "fundamental/application/application.hpp"
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
            network::io_context_pool::s_excutorNums=std::stoi(argv[3]);
            network::io_context_pool::Instance().start();
            // Initialise the server.
            network::echo::EchoServer s(argv[1], argv[2]);
            s.Start();
            Fundamental::Application::Instance().Loop();
        }
        catch (std::exception& e)
        {
            FERR("exception: {}",e.what());
        }

        return 0;
    }
    else
    {
        using asio::ip::tcp;
        try
        {
            if (argc != 3)
            {
                std::cerr << "Usage: echo_client <host> <port>\n";
                return 1;
            }

            asio::io_context io_context;

            tcp::resolver resolver(io_context);
            auto endpoints = resolver.resolve(argv[1], argv[2]);
            network::echo::echo_client c(io_context, endpoints);

            std::thread t([&io_context]() { io_context.run(); });
            auto maxSize=64;
            char line[maxSize- 1];
            bool isRunning=true;
            c.wait_connect();
            while (c.connected()&&isRunning&&std::cin.getline(line, maxSize + 1))
            {
                network::echo::EchoMsg msg;
                auto len=std::strlen(line);
                if(0==len)
                    continue;
                FINFO("read size:{}",len);
                if(strncmp(line,"#",1)==0)
                {
                    FWARN("disconnected request");
                    len=0;
                    isRunning=false;
                }
                auto bufLen=len+network::echo::EchoMsg::kTimeStampSize;
                msg.msg.resize(bufLen);
                msg.header.v=htobe32(bufLen);
                ;
                msg.TimeStamp()=htobe64((Fundamental::Timer::GetTimeNow<std::chrono::seconds,std::chrono::system_clock>()));
                std::memcpy(msg.Data(),line,len);
                c.write(msg);
            }
            t.join();
            c.close();
        }
        catch (std::exception& e)
        {
            FERR("exception: {}",e.what());
        }

        return 0;
    }
}
