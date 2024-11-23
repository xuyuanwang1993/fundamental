#pragma once
#include "acceptor.h"
#include "tcp-connection.h"
namespace io
{
class TcpServerConnection;

class TcpServer:public Acceptor
{
    friend class TcpServerConnection;
public:
    TcpServer(TaskScheduler *pTaskSchedulerRef,const std::string &service);
    virtual ~TcpServer();
    virtual std::shared_ptr<TcpConnection> NewConnection(SOCKET fd);
protected:
    bool AddConnection(std::shared_ptr<TcpConnection>connection);
    void RemoveConnection(SOCKET fd);
protected:
    std::mutex dataMutex;
    std::unordered_map<SOCKET,std::weak_ptr<TcpConnection>> connetions;
};

class TcpServerConnection :public TcpConnection
{
    friend class TcpServer;
protected:
    struct PrivateConstructor
    {

    };
public:
    ~TcpServerConnection() override;
    explicit TcpServerConnection(TaskScheduler *pTaskSchedulerRef,SOCKET _fd,TcpServer * pServerRef,PrivateConstructor);
protected:

    TcpServer * const pTcpServerRef=nullptr;
};
}