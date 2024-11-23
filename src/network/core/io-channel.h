#ifndef IOCHANNEL_H
#define IOCHANNEL_H
#include "network-util.h"
#include "fundamental/events/event_system.h"
#include<memory>

namespace io {
class TaskScheduler;
class EpolltTaskScheduler;
class SelectTaskScheduler;
class IoChannel:public std::enable_shared_from_this<IoChannel>
{
    friend class TaskScheduler;
    friend class EpolltTaskScheduler;
    friend class SelectTaskScheduler;
public:
    enum EventType
    {
        EVENT_NONE   = 0,
        EVENT_IN     = 1,
        EVENT_PRI    = 2,
        EVENT_OUT    = 4,
        EVENT_ERR    = 8,
        EVENT_HUP    = 16,
        EVENT_RDHUP  = 8192
    };
public://signals
    /**
     * @brief bytesReady emit when the channel become readable
     */
    Fundamental::Signal<void()>bytesReady;
    /**
     * @brief writeReady emit when the channel become writable
     */
    Fundamental::Signal<void()>writeReady;
    /**
     * @brief errorEvent emit when an error occurred
     */
    Fundamental::Signal<void()> errorEvent;
    /**
     * @brief closeEvent emit when the channel is closed
     */
    Fundamental::Signal<void()> closeEvent;
public:
    static std::shared_ptr<IoChannel>Create(TaskScheduler *pTaskSchedulerRef,SOCKET _fd);
    void EnableReading();
    void EnablWriting();
    void DisableReading();
    void DisableWriting();
    bool IsReading()const;
    bool IsWriting()const;
    bool IsNoneEvent()const;
    /**
     * @brief ReuseFd only call this when you want recycle fd
     */
    void ReuseFd();
    SOCKET GetFd()const;
     ~IoChannel();
    /**
     * @brief Sync 更新调度器中channel的状态
     */
    void Sync();
    /**
     * @brief Stop 从调度器中移除channel
     */
    void Stop();
private:
    int GetEvents()const;
    void HandleIoEvent(int _events);
    IoChannel(TaskScheduler *pTaskSchedulerRef,SOCKET _fd);
private:
    std::atomic<int>events;
    SOCKET fd;
    TaskScheduler * const taskSchedulerRef=nullptr;
};
}
#endif // IOCHANNEL_H
