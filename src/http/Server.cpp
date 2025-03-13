
#include "Server.hpp"
#include <functional>
#include <thread>
#include <vector>
#include "network/network.hpp"
namespace network::http
{

Server::Server( std::uint16_t port,
               const std::string& docRoot, std::size_t threadPoolSize) :
m_threadPoolSize(threadPoolSize),
m_signals(m_ioService),
m_acceptor(m_ioService),
m_newConnection(),
m_requestHandler(docRoot)
{
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    m_signals.add(SIGINT);
    m_signals.add(SIGTERM);
#if defined(SIGQUIT)
    m_signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
    m_signals.async_wait(std::bind(&Server::HandleStop, this));

    network::protocal_helper::init_acceptor(m_acceptor,port);

    StartAccept();
}

void Server::Run()
{
    // Create a pool of threads to run all of the io_services.
    std::vector<std::shared_ptr<std::thread>> threads;
    for (std::size_t i = 0; i < m_threadPoolSize; ++i)
    {
        std::shared_ptr<std::thread> thread(new std::thread(
            std::bind(static_cast<std::size_t (::asio::io_context::*)()>(&::asio::io_context::run), &m_ioService)));
        threads.push_back(thread);
    }

    // Wait for all threads in the pool to exit.
    for (std::size_t i = 0; i < threads.size(); ++i)
        threads[i]->join();
}

void Server::Stop()
{
    HandleStop();
}

void Server::StartAccept()
{
    m_newConnection.reset(new Connection(m_ioService, m_requestHandler));
    m_newConnection->SetHttpHandler(m_httpHandler);
    m_acceptor.async_accept(m_newConnection->Socket(),
                            std::bind(&Server::HandleAccept, this,
                                      std::placeholders::_1));
}

void Server::HandleAccept(const ::asio::error_code& e)
{
    if (!e)
    {
        m_newConnection->Start();
    }

    StartAccept();
}

void Server::HandleStop()
{
    m_ioService.stop();
}

} // namespace network::http