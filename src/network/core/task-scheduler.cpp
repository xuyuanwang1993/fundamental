#include "task-scheduler.h"
#include "io-channel.h"
#include "fundamental/basic/log.h"
#if TARGET_PLATFORM_LINUX
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#endif
#include <future>
using namespace io;

std::shared_ptr<IoChannel> TaskScheduler::AddChannel(SOCKET fd)
{
    return IoChannel::Create(this,fd);
}

TaskScheduler::~TaskScheduler()
{
    Stop();
}

void io::TaskScheduler::Loop()
{
    FINFO("start TaskScheduler");
    isProcessing.exchange(true);
    exitingStatus.test_and_set();
    std::scoped_lock<std::mutex> locker(_mutex);
    workThread=std::make_unique<std::thread>([=](){
        this->Exec();
    });
}

void io::TaskScheduler::Stop()
{
    FINFO("stop TaskScheduler");
    bool expected=true;
    if(!isProcessing.compare_exchange_strong(expected,false))
    {
        FWARN("TaskScheduler is not running");
        return;
    }
    //wait status clear
    while (exitingStatus.test_and_set())
    {
        std::this_thread::yield();
    }
    std::scoped_lock<std::mutex> locker(_mutex);
    if(workThread&&workThread->joinable())
        workThread->join();
}

void TaskScheduler::Exec()
{
#if TARGET_PLATFORM_LINUX
    prctl(PR_SET_NAME,_name.empty()?_name.c_str():"TaskScheduler", 0, 0, 0);
#endif
    PreInit();
    wakeupChannel->Sync();
    while(isProcessing)
    {
        Tick();
    }
    wakeupChannel->Stop();
    ResetStatus();
    exitingStatus.clear();
}



void TaskScheduler::OnWakeupChannelRecv()
{
    char recv_buf;
    while(wakeupPipe->read(&recv_buf,1)>0);
}

std::shared_ptr<IoChannel> TaskScheduler::CheckAndGetChannelByFd(SOCKET fd)
{
    std::scoped_lock<std::mutex> locker(_mutex);
    auto iter=channelsMap.find(fd);
    if(iter==channelsMap.end())return nullptr;
    auto ret=iter->second.lock();
    if(!ret)
    {
        channelsMap.erase(iter);
        ClearFd(fd);
    }
    return ret;
}

void io::TaskScheduler::WakeUpImp()
{
    static const char data='1';
    wakeupPipe->write(&data,1);
}

void io::TaskScheduler::Wait(std::int64_t timeMsec)
{
    HandleNetworkEvent(timeMsec);
}

TaskScheduler::TaskScheduler(const std::string &name):_name(name),wakeupPipe(new Pipe())
{

#if TARGET_PLATFORM_LINUX
    signal(SIGPIPE, SIG_IGN);
#endif
    FASSERT(wakeupPipe->open()," pipe open failed");
    wakeupChannel=IoChannel::Create(this,wakeupPipe.get()->operator()());
    wakeupChannel->bytesReady.Connect(std::bind(&TaskScheduler::OnWakeupChannelRecv,this));
    wakeupChannel->EnableReading();
    wakeupChannel->Sync();
}
