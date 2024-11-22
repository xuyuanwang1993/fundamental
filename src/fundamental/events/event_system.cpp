#include "event_system.h"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"

namespace Fundamental
{
bool EventSystemWrapper::IsIdle() const
{
    return m_syncer.emptyQueue();
}

std::size_t EventSystemWrapper::EventsTick(std::size_t maxProcessEventNums, std::uint32_t maxProcessTimeMsec)
{
    auto startTimeMsec      = Fundamental::Timer::GetTimeNow();
    std::size_t proceedNums = 0;
    while (maxProcessTimeMsec != 0)
    {
        if (!m_syncer.processOne())
            break;
        --maxProcessTimeMsec;
        ++proceedNums;
        if (maxProcessTimeMsec > 0 &&
            (Fundamental::Timer::GetTimeNow() - startTimeMsec > static_cast<decltype(startTimeMsec)>(maxProcessTimeMsec)))
        {
            break;
        }
    }
    return proceedNums;
}

/*
 * return false when the hash is existed
 */
bool EventSystemWrapper::RegisterEvent(std::size_t eventHash)
{
    return m_hashDic.insert(eventHash).second;
}

EventHandleType EventSystemWrapper::AddEventListener(std::size_t eventHash, const EventCallbackType& listener)
{
    return m_syncer.appendListener(eventHash, listener);
}
void EventSystemWrapper::Release()
{
    m_hashDic.clear();
    EQ clearEq;
    std::swap(clearEq, m_syncer);
}
void EventSystemWrapper::Init()
{ // do nothing
}
} // namespace Fundamental
