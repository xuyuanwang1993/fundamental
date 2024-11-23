#ifndef EPOLLTASKSCHEDULER_H
#define EPOLLTASKSCHEDULER_H
#include "task-scheduler.h"
namespace io {
class EpolltTaskScheduler final:public TaskScheduler{
public:
    static std::shared_ptr<TaskScheduler>Create(int _id);
    ~EpolltTaskScheduler();
protected:
    void UpdateChannel(const std::shared_ptr<IoChannel>&channel)override;
    void RemoveChannel(SOCKET fd)override;
    void HandleNetworkEvent(int64_t timeout)override;
    void PreInit()override;
    void ResetStatus()override;
    void ClearFd(SOCKET fd)override;
private:
    EpolltTaskScheduler(int _id);
    void update(int operation, SOCKET fd,int events);
private:
    std::atomic<SOCKET> epollFd;
};
}
#endif // EPOLLTASKSCHEDULER_H
