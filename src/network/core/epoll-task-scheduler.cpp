#include "epoll-task-scheduler.h"
#include "fundamental/basic/log.h"
#include "io-channel.h"
#include "platform_internal.h"
/*epoll*/
#if TARGET_PLATFORM_LINUX
    #include <sys/epoll.h>
#endif
using namespace io;
std::shared_ptr<TaskScheduler> EpolltTaskScheduler::Create(int _id)
{
    return std::shared_ptr<TaskScheduler>(new EpolltTaskScheduler(_id));
}

EpolltTaskScheduler::~EpolltTaskScheduler()
{
}

void EpolltTaskScheduler::UpdateChannel(const std::shared_ptr<IoChannel>& channel)
{
#if TARGET_PLATFORM_LINUX
    if (!channel || channel->GetFd() == INVALID_SOCKET)
        return;
    {
        std::scoped_lock<std::mutex> locker(dataMutex);
        auto fd = channel->GetFd();
        {
            auto iter = channelsMap.find(fd);
            if (iter == std::end(channelsMap))
            {
                if (!channel->IsNoneEvent())
                {
                    channelsMap.emplace(fd, std::weak_ptr<IoChannel>(channel));
                    update(EPOLL_CTL_ADD, channel->GetFd(), channel->GetEvents());
                }
            }
            else
            {
                if (channel->IsNoneEvent())
                {
                    update(EPOLL_CTL_DEL, channel->GetFd(), channel->GetEvents());
                    channelsMap.erase(iter);
                }
                else
                {
                    update(EPOLL_CTL_MOD, channel->GetFd(), channel->GetEvents());
                }
            }
        }
    }
    WakeUp();
#else
    (void)channel;
#endif
}

void EpolltTaskScheduler::RemoveChannel(int fd)
{
#if TARGET_PLATFORM_LINUX
    {
        auto iter = channelsMap.find(fd);
        if (iter != std::end(channelsMap))
        {
            update(EPOLL_CTL_DEL, fd, 0);
            channelsMap.erase(iter);
        }
    }
    WakeUp();
#else
    (void)channel;
#endif
}

void EpolltTaskScheduler::HandleNetworkEvent(int64_t timeout)
{
#if TARGET_PLATFORM_LINUX
    struct epoll_event events[512];
    bzero(&events, sizeof(events));
    int numEvents = -1;
    numEvents     = epoll_wait(epollFd.load(), events, 512, static_cast<int>(timeout));
    if (numEvents < 0) //
    {
        if (io::platform::getErrno() != EINTR)
        {
            FERR("epoll error{}", strerror(io::platform::getErrno()));
        }
        else
        {
            FWARN("epoll error{}", strerror(io::platform::getErrno()));
        }
        return;
    }
    else if (numEvents > 0)
    {
        for (int n = 0; n < numEvents; n++)
        {
            auto channel = CheckAndGetChannelByFd(events[n].data.fd);
            if (channel)
                channel->HandleIoEvent(events[n].events);
        }
    }
#endif
}

void EpolltTaskScheduler::PreInit()
{
#if TARGET_PLATFORM_LINUX
    epollFd.exchange(epoll_create1(0));
#endif
}

void EpolltTaskScheduler::ResetStatus()
{
#if TARGET_PLATFORM_LINUX
    ::close(epollFd.load());
    epollFd.exchange(INVALID_SOCKET);
#endif
}

void EpolltTaskScheduler::ClearFd(SOCKET fd)
{
#if TARGET_PLATFORM_LINUX
    update(EPOLL_CTL_DEL, fd, 0);
#else
    (void)fd;
#endif
}

void EpolltTaskScheduler::update(int operation, SOCKET fd, int events)
{
#if TARGET_PLATFORM_LINUX
    struct epoll_event event;
    bzero(&event, sizeof(event));
    if (operation != EPOLL_CTL_DEL)
    {
        event.data.fd = fd;
        event.events  = static_cast<uint32_t>(events);
    }

    if (::epoll_ctl(epollFd.load(), operation, fd, &event) < 0)
    {
        FERR("epoll ctl error{}! epollfd[{}] operation[{}:{}:{}]", strerror(platform::getErrno()), epollFd.load(), operation, fd, events);
    }
#else
    (void)operation;
    (void)fd;
    (void)events;
#endif
}

EpolltTaskScheduler::EpolltTaskScheduler(int _id) :
TaskScheduler(std::string("epoll_") + std::to_string(_id))
{
}
