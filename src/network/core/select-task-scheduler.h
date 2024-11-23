#ifndef SELECTTASKSCHEDULER_H
#define SELECTTASKSCHEDULER_H
#include "task-scheduler.h"
#include <set>
namespace io {
class SelectTaskScheduler final:public TaskScheduler{
public:
    static std::shared_ptr<TaskScheduler>Create(int _id,const std::string &name="");
    ~SelectTaskScheduler();
private:
    void UpdateChannel(const std::shared_ptr<IoChannel>&channel)override;
    void RemoveChannel(SOCKET fd)override;
    void HandleNetworkEvent(int64_t timeout)override;
    void PreInit()override;
    void ResetStatus()override;
    void ClearFd(SOCKET fd)override;
private:
    SelectTaskScheduler(int _id,const std::string &name);
private:
    /**
     * @brief m_read_sets 读fd集合
     */
    fd_set read_sets;
    /**
     * @brief m_write_sets 写fd集合
     */
    fd_set write_sets;
    /**
     * @brief m_exception_sets 异常fd集合
     */
    fd_set exception_sets;
    /**
     * @brief allfdSet 所有fd的集合
     */
    std::set<SOCKET> allfdSet;
};
}
#endif // SELECTTASKSCHEDULER_H
