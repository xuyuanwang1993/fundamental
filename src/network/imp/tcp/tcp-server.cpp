#include "tcp-server.h"
#include "fundamental/basic/log.h"
namespace io
{
TcpServer::TcpServer(TaskScheduler* pTaskSchedulerRef,const std::string &service):Acceptor(pTaskSchedulerRef,service)
{

}

TcpServer::~TcpServer()
{
    connetions.clear();
}

bool TcpServer::AddConnection(std::shared_ptr<TcpConnection> connection)
{
    bool ret = false;
    std::lock_guard<std::mutex> locker(dataMutex);
    do
    {
        auto fd = connection->GetFd();
        if (fd <= 0)
            break;
        auto iter = connetions.find(fd);
        if (iter != connetions.end())
            break;
        connetions.emplace(fd, connection);
    } while (0);
    return ret;
}

void TcpServer::RemoveConnection(SOCKET fd)
{
    std::lock_guard<std::mutex> locker(dataMutex);
    auto iter = connetions.find(fd);
    if (iter != connetions.end())
    {
        connetions.erase(iter);
    }
}

TcpServerConnection::TcpServerConnection(TaskScheduler* pTaskSchedulerRef, SOCKET _fd, TcpServer* pServerRef,PrivateConstructor) :
TcpConnection(pTaskSchedulerRef, _fd), pTcpServerRef(pServerRef)
{
    pTcpServerRef->AddConnection(shared_from_this());
}

TcpServerConnection::~TcpServerConnection()
{
    pTcpServerRef->RemoveConnection(GetFd());
}

std::shared_ptr<TcpConnection> TcpServer::NewConnection(SOCKET fd)
{
    return std::make_shared<TcpServerConnection>(pScheduler, fd, this,TcpServerConnection::PrivateConstructor{});
}

} // namespace io
