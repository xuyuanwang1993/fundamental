#pragma once
#include "network/core/task-scheduler.h"
#include <memory>
namespace io {
class ProtocalBase;
class IcacheBufferBase;

class TcpConnection :public std::enable_shared_from_this<TcpConnection>
{
public:
    //this signal will emit when the connection disconnected
    Fundamental::Signal<void()>disconnected;
    Fundamental::Signal<void(const void * /* data ptr ref */,uint32_t /*frame_len*/)> notifyFrame;
public:
    TcpConnection(TaskScheduler *pTaskSchedulerRef,SOCKET _fd);
    virtual ~TcpConnection();
    SOCKET GetFd()const { return  fd;}
    std::string GetPeerHostName() const { return peerHostName;}
    uint16_t GetPeerPort() const { return  peerPort;}
    // you should call this interface only once
    virtual void SetProtocal(std::shared_ptr<ProtocalBase>_protocal);
    // send failed means network is  very poor
    virtual bool SendFrame(const void *frame, uint32_t frame_len);
protected:
    virtual void OnRecv();
    virtual void OnWrite();
    virtual void OnClose();
    virtual void OnError();
private:
    //signal handler
    void HandRecv(){ OnRecv();}
    void HandWrite(){ OnWrite();}
    void HandClose(){ OnClose();}
    void HandError(){ OnError();}
protected:
    TaskScheduler *const scheduler;
    const SOCKET fd;
    const std::string peerHostName;
    const uint16_t peerPort;
    std::shared_ptr<IoChannel> channel;
    std::shared_ptr<IcacheBufferBase> readCache;
    std::shared_ptr<IcacheBufferBase> writeCache;
    std::mutex writeMutex;
};

}