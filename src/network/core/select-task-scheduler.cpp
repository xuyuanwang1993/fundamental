#include "select-task-scheduler.h"
#include "io-channel.h"
#include "platform_internal.h"
#include "fundamental/basic/log.h"
using namespace io;
std::shared_ptr<TaskScheduler> SelectTaskScheduler::Create(int _id,const std::string &name)
{
    return std::shared_ptr<TaskScheduler>(new SelectTaskScheduler(_id,name));
}

SelectTaskScheduler::~SelectTaskScheduler()
{

}

void SelectTaskScheduler::UpdateChannel(const std::shared_ptr<IoChannel>&channel)
{
    if(!channel||channel->GetFd()==INVALID_SOCKET)return;
    {
        std::scoped_lock<std::mutex> locker(dataMutex);
       auto fd=channel->GetFd();
       auto iter=channelsMap.find(channel->GetFd());
       if(iter!=channelsMap.end())
       {
           ClearFd(iter->first);
           allfdSet.erase(iter->first);
           channelsMap.erase(iter);
       }
       if(!channel->IsNoneEvent())
       {
           FD_SET(fd,&exception_sets);
           if(channel->IsReading())FD_SET(fd,&read_sets);
           if(channel->IsWriting())FD_SET(fd,&write_sets);
           allfdSet.insert(fd);
           channelsMap.emplace(fd,std::weak_ptr<IoChannel>(channel));
       }

    }
    WakeUp();
}

void SelectTaskScheduler::RemoveChannel(int fd)
{
    {
        std::scoped_lock<std::mutex> locker(dataMutex);
        auto iter=channelsMap.find(fd);
        if(iter!=channelsMap.end())
        {
            ClearFd(iter->first);
            allfdSet.erase(iter->first);
            channelsMap.erase(iter);
        }
    }
    
    WakeUp();
}

void SelectTaskScheduler::HandleNetworkEvent(int64_t timeout)
{
    fd_set read_sets_copy;
    fd_set write_sets_copy;
    fd_set exception_sets_copy;
    FD_ZERO(&read_sets_copy);
    FD_ZERO(&write_sets_copy);
    FD_ZERO(&exception_sets_copy);
    SOCKET max_fd;
    {
        read_sets_copy=read_sets;
        write_sets_copy=write_sets;
        exception_sets_copy=exception_sets;
        max_fd=allfdSet.empty()?1:*allfdSet.rbegin();
    }
    struct timeval tv = { static_cast<__time_t>(timeout) / 1000, static_cast<__suseconds_t>(timeout) % 1000*1000 };
    int ret = select(max_fd+1, &read_sets_copy, &write_sets_copy, &exception_sets_copy, &tv);
    if(ret<0){
#if TARGET_PLATFORM_LINUX
        if(io::platform::getErrno()!=EINTR&&io::platform::getErrno()!=EAGAIN)
        {
            FERR("select error{}",strerror(io::platform::getErrno()));
        }
        else {
            FWARN("select error{}",strerror(io::platform::getErrno()));
        }
#elif TARGET_PLATFORM_WINDOW
        int err = io::platform::getErrno();
        if (err == WSAEINVAL && read_sets.fd_count == 0) {err=EINTR;}
        if(err!=EINTR){
            FERR("select error[{}]",err);
        }
        else {
            FWARN("select error[{}]",err);
        }
#endif
    }
    else if (ret>0) {
        for(SOCKET fd=1;fd<=max_fd;fd++){
            auto channel=CheckAndGetChannelByFd(fd);
            if(!channel)continue;
            int events=0;
            if(FD_ISSET(fd,&read_sets_copy))events|=IoChannel::EVENT_IN;
            if(FD_ISSET(fd,&write_sets_copy))events|=IoChannel::EVENT_OUT;
            if(FD_ISSET(fd,&exception_sets_copy))events|=IoChannel::EVENT_HUP;
            channel->HandleIoEvent(events);
        }
    }
}

void SelectTaskScheduler::PreInit()
{

}

void SelectTaskScheduler::ResetStatus()
{
    allfdSet.clear();
    FD_ZERO(&read_sets);
    FD_ZERO(&write_sets);
    FD_ZERO(&exception_sets);
}

void SelectTaskScheduler::ClearFd(SOCKET fd)
{
    FD_CLR(fd,&read_sets);
    FD_CLR(fd,&write_sets);
    FD_CLR(fd,&exception_sets);
}

SelectTaskScheduler::SelectTaskScheduler(int _id,const std::string &name):TaskScheduler(name.empty()?(std::string("select_")+std::to_string(_id)):name)
{
    ResetStatus();
}
