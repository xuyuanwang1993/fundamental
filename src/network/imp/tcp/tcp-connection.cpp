#include "tcp-connection.h"
#include "fundamental/basic/log.h"
#include "network/core/io-channel.h"
#include "network/imp/tcp/icache_buffer_stream.h"
using namespace io;
TcpConnection::TcpConnection(TaskScheduler* pTaskSchedulerRef, SOCKET _fd) :
scheduler(pTaskSchedulerRef), fd(_fd), peerHostName(NETWORK_UTIL::get_peer_ip(fd)), peerPort(NETWORK_UTIL::get_peer_port(fd)), channel(nullptr), readCache(nullptr), writeCache(nullptr)
{
    channel = scheduler->AddChannel(fd);
    channel->bytesReady.Connect(std::bind(&TcpConnection::HandRecv, this));
    channel->writeReady.Connect(std::bind(&TcpConnection::HandWrite, this));
    channel->closeEvent.Connect(std::bind(&TcpConnection::HandClose, this));
    channel->errorEvent.Connect(std::bind(&TcpConnection::HandError, this));
}

TcpConnection::~TcpConnection()
{
    if (channel)
    {
        channel->Stop();
    }
    channel.reset();
    FWARN("release tcp connection {}", fd);
}

void TcpConnection::SetProtocal(std::shared_ptr<ProtocalBase> _protocal)
{
    FASSERT(_protocal, "need a valid procoal");
    channel->Stop();
    readCache.reset(new IcacheBufferStream(_protocal, fd));
    writeCache.reset(new IcacheBufferStream(_protocal, fd));
    channel->EnableReading();
    channel->Sync();
}

bool TcpConnection::SendFrame(const void* frame, uint32_t frame_len)
{

    if (frame_len == 0)
        return true;
    std::unique_lock<std::mutex> locker(writeMutex);
    auto ret = writeCache->appendFrame(frame, frame_len);
    locker.unlock();
    if (writeCache->frame_count() == 1)
    {
        channel->EnablWriting();
        channel->Sync();
    }
    return ret > 0 && ret == frame_len;
}

void TcpConnection::OnRecv()
{
    while (1)
    {
        auto ret = readCache->readFromFd();
        if (ret < 0)
            break;
        else if (ret == 0)
        {
            FDEBUG("recv remote maybe disconnected,close it!");
            OnClose();
            break;
        }
        //
        while (1)
        {
            auto frame = readCache->popFrame();
            if (frame.second > 0)
            {
                notifyFrame(frame.first, frame.second);
            }
            else
            {
                break;
            }
        }
    }
}

void TcpConnection::OnWrite()
{
    {//copy data from user space to kernel space
        std::lock_guard<std::mutex> locker(writeMutex);
        while (writeCache->frame_count() > 0)
        {
            auto ret = writeCache->sendCacheByFd();
            if (ret < 0)
                break;
            if (ret == 0)
            {
                FDEBUG("write remote maybe disconnected,close it!");
                OnClose();
                return;
            }
        }
    }

    if (writeCache->frame_count() == 0)
    {
        channel->DisableWriting();
        channel->Sync();
    }
}

void TcpConnection::OnClose()
{
    FERR("tcp connection closed,disconnect it! {}", strerror(NETWORK_UTIL::get_socket_error(fd)));
    channel->Stop();
    disconnected();
}

void TcpConnection::OnError()
{
    FERR("tcp connection error,disconnect it! {}", strerror(NETWORK_UTIL::get_socket_error(fd)));
    channel->Stop();
    disconnected();
}