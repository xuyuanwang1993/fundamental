
#pragma once
#include "event.h"
#include "eventpp/eventdispatcher.h"
#include "eventpp/eventqueue.h"
#include <functional>
#include <set>
namespace Fundamental
{
using EQ              = eventpp::EventQueue<EventType, void(const EventPointerType&), EventPolicy>;
using EventHandleType = decltype(std::declval<EQ>().appendListener(std::declval<EventType>(), std::declval<EventCallbackType>()));
//---------------------------------------------------------------//
struct InternalNotifyQueuedEvent : Fundamental::Event
{
    // an event must contain 'kEventType' field;
    constexpr inline static std::size_t kEventType = ComputeEventHash(0, "InternalNotifyQueuedEvent", "InternalNotifyQueuedEvent", "internal");

    InternalNotifyQueuedEvent() :
    Event(kEventType)
    {
    }
};
class EventSystemWrapper
{
public:
    EventSystemWrapper()          = default;
    virtual ~EventSystemWrapper() = default;

    bool IsIdle() const;
    /*
     * maxProcessEventNums [int] maximum  proceed events' nums in one tick invocation
     * maxProcessTimeMsec [int] maximum proceed time msec int one tick invocation
     * this function must be called in this module inited thread
     * return the proceed events nums
     */
    std::size_t EventsTick(std::size_t maxProcessEventNums = static_cast<std::size_t>(~0), std::uint32_t maxProcessTimeMsec = 20);

    /*
     * return false when the hash is existed
     */
    bool RegisterEvent(std::size_t eventHash);

    /*
     * append an event listener
     */
    EventHandleType AddEventListener(std::size_t eventHash, const EventCallbackType& listener);
    /*
     *  cache event into syncer
     * events will be actually dispatched when EventsTick is called
     */
    template <typename EventDataType, typename... Args>
    decltype(auto) DispatcherEvent(Args&&... args);

    /*
     *
     * dispatch an event immediatelly
     * events can be  stopped by Event.StopImmediatePropagation();
     */
    template <typename EventDataType, typename... Args>
    decltype(auto) DispatcherImmediateEvent(Args&&... args);

    void Release();

    void Init();

protected:
    EQ m_syncer;
    std::set<std::size_t> m_hashDic;
};


/*
 *
 * dispatch an event immediatelly
 * events can be  stopped by Event.StopImmediatePropagation();
 */

template <typename EventDataType, typename... Args>
inline decltype(auto) EventSystemWrapper::DispatcherImmediateEvent(Args&&... args)
{
    return m_syncer.dispatch(std::make_shared<EventDataType>(std::forward<Args>(args)...));
}

/*
 *  cache event into syncer
 * events will be actually dispatched when EventsTick is called
 */

template <typename EventDataType, typename... Args>
inline decltype(auto) EventSystemWrapper::DispatcherEvent(Args&&... args)
{
    EventSystemWrapper::template DispatcherImmediateEvent<InternalNotifyQueuedEvent>();
    return m_syncer.enqueue(std::make_shared<EventDataType>(std::forward<Args>(args)...));
}

} // namespace Fundamental
