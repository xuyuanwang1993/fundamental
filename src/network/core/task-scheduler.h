#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H
#include "fundamental/events/event_process.h"
#include "task-pipe.h"
#include <thread>
#include <memory>
#include <mutex>
#include <functional>
namespace io
{
class IoChannel;
class TaskScheduler : public Fundamental::EventsHandler
{
public:
    /**
     * @brief AddChannel 添加一个socketchannel
     */
    std::shared_ptr<IoChannel> AddChannel(SOCKET fd);
    virtual ~TaskScheduler();
    void Loop();
    void Stop();

protected:
    /**
     * @brief UpdateChannel 更新channel监听的事件
     * 内部存放弱引用
     */
    virtual void UpdateChannel(const std::shared_ptr<IoChannel>& channel) = 0;
    /**
     * @brief RemoveChannel 移除channel
     */
    virtual void RemoveChannel(SOCKET fd)            = 0;
    virtual void HandleNetworkEvent(int64_t timeout) = 0;
    virtual void PreInit()                           = 0;
    virtual void ResetStatus()                       = 0;
    virtual void ClearFd(SOCKET fd)                  = 0;

protected:
    explicit TaskScheduler(const std::string& name);
    virtual void Exec();
    void OnWakeupChannelRecv();
    std::shared_ptr<IoChannel> CheckAndGetChannelByFd(SOCKET fd);

protected:
    void WakeUpImp() override;
    void Wait(std::int64_t timeMsec) override;

protected:
    const std::string _name;
    std::mutex _mutex;
    std::mutex dataMutex;
    std::unique_ptr<std::thread> workThread;
    std::atomic<bool> isProcessing = false;
    std::atomic_flag exitingStatus=ATOMIC_FLAG_INIT;
    std::shared_ptr<Pipe> wakeupPipe;
    std::shared_ptr<IoChannel> wakeupChannel;
    std::unordered_map<SOCKET, std::weak_ptr<IoChannel>> channelsMap;
    
    //
    friend class IoChannel;
};
} // namespace io
#endif // TASKSCHEDULER_H
