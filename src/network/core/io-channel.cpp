#include "io-channel.h"
#include "fundamental/basic/log.h"
#include "task-scheduler.h"
using namespace io;
std::shared_ptr<IoChannel> IoChannel::Create(TaskScheduler* pTaskSchedulerRef, SOCKET _fd)
{
    return std::shared_ptr<IoChannel>(new IoChannel(pTaskSchedulerRef, _fd));
}

void IoChannel::EnableReading()
{
    events |= EVENT_IN;
}

void IoChannel::EnablWriting()
{
    events |= EVENT_OUT;
}

void IoChannel::DisableReading()
{
    events &= ~EVENT_IN;
}

void IoChannel::DisableWriting()
{
    events &= ~EVENT_OUT;
}

bool IoChannel::IsReading() const
{
    return (events & EVENT_IN) != EVENT_NONE;
}

bool IoChannel::IsWriting() const
{
    return (events & EVENT_OUT) != EVENT_NONE;
}

bool IoChannel::IsNoneEvent() const
{
    return events == EVENT_NONE;
}

void IoChannel::ReuseFd()
{
    fd = INVALID_SOCKET;
}

SOCKET IoChannel::GetFd() const
{
    return fd;
}

int IoChannel::GetEvents() const
{
    return events.load();
}

void IoChannel::HandleIoEvent(int _events)
{
    do
    {
        if (_events & EVENT_IN || _events & EVENT_PRI)
        {
            bytesReady.Emit();
        }
        if (_events & EVENT_HUP)
        {
            closeEvent.Emit();
            break;
        }
        if (_events & EVENT_ERR)
        {
            errorEvent.Emit();
            break;
        }
        if (_events & EVENT_OUT)
        {
            writeReady.Emit();
        }
    } while (0);
}

IoChannel::~IoChannel()
{
    FDEBUG("release channel {}", fd);
    Stop();
    if (fd != INVALID_SOCKET)
        io::util::NetworkUtil::close_socket(fd);
}

void IoChannel::Sync()
{
    taskSchedulerRef->UpdateChannel(shared_from_this());
}

void IoChannel::Stop()
{
    taskSchedulerRef->RemoveChannel(fd);
}

IoChannel::IoChannel(TaskScheduler* pTaskSchedulerRef, SOCKET _fd) :
taskSchedulerRef(pTaskSchedulerRef), events(EVENT_NONE), fd(_fd)
{
    NETWORK_UTIL::make_noblocking(fd);
}
