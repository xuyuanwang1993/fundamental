#pragma once
#include "network/core/network-util.h"
#include "network/core/task-scheduler.h"
#include <atomic>
namespace io {
class Acceptor
{
public:
    /**
     * @brief newConnectionComing send this signal while a passive connection is established
     */
    Fundamental::Signal<void(SOCKET)> NewConnectionComing;
public:
    Acceptor(TaskScheduler *pTaskSchedulerRef, const std::string &service);
    bool StartListen(uint32_t max_pending_size=20);
    void StopListen();
    std::string ServiceName()const;
    virtual ~Acceptor();
protected:
    void OnAccept(int fd);
protected:
    TaskScheduler * const pScheduler;
    const std::string service_name;
    std::list<std::shared_ptr<IoChannel>> server_channel_list;
    std::atomic<bool> listenning=false;
};
}