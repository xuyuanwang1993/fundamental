
#pragma once
#include "event.h"
#include "eventpp/eventdispatcher.h"
#include "eventpp/eventqueue.h"
#include <functional>
#include <future>
#include <set>
namespace Fundamental
{
using EQ = eventpp::EventQueue<EventType, void(const EventPointerType&), EventPolicy>;
using EventHandleType =
    decltype(std::declval<EQ>().appendListener(std::declval<EventType>(), std::declval<EventCallbackType>()));
//---------------------------------------------------------------//
struct InternalNotifyQueuedEvent : Fundamental::Event {
    // an event must contain 'kEventType' field;
    constexpr inline static std::size_t kEventType =
        ComputeEventHash(0, "InternalNotifyQueuedEvent", "InternalNotifyQueuedEvent", "internal") | 1;

    InternalNotifyQueuedEvent() : Event(kEventType) {
    }
};

class EventSystem {
public:
    EventSystem()          = default;
    virtual ~EventSystem() = default;

    bool IsIdle() const;
    /*
     * maxProcessEventNums [int] maximum  proceed events' nums in one tick invocation
     * maxProcessTimeMsec [int] maximum proceed time msec int one tick invocation
     * this function must be called in this module inited thread
     * return the proceed events nums
     */
    std::size_t EventsTick(std::size_t maxProcessEventNums  = static_cast<std::size_t>(~0),
                           std::uint32_t maxProcessTimeMsec = 20);

    /*
     * throw std::invalid_argument when the eventHash is existed
     */
    void RegisterEvent(std::size_t eventHash);

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

protected:
    EQ m_syncer;
    std::mutex m_mutex;
    std::set<std::size_t> m_hashDic;
};

/*
 *
 * dispatch an event immediatelly
 * events can be  stopped by Event.StopImmediatePropagation();
 */

template <typename EventDataType, typename... Args>
inline decltype(auto) EventSystem::DispatcherImmediateEvent(Args&&... args) {
    return m_syncer.dispatch(std::make_shared<EventDataType>(std::forward<Args>(args)...));
}

/*
 *  cache event into syncer
 * events will be actually dispatched when EventsTick is called
 */

template <typename EventDataType, typename... Args>
inline decltype(auto) EventSystem::DispatcherEvent(Args&&... args) {
    EventSystem::template DispatcherImmediateEvent<InternalNotifyQueuedEvent>();
    return m_syncer.enqueue(std::make_shared<EventDataType>(std::forward<Args>(args)...));
}

template <typename Prototype, typename PoliciesType>
class SignalBase;

// callbacklist wapper class
template <typename PoliciesType, typename ReturnType, typename... Args>
class SignalBase<ReturnType(Args...), PoliciesType> {
public:
    using Callback_ =
        typename eventpp::internal_::SelectCallback<PoliciesType,
                                                    eventpp::internal_::HasTypeCallback<PoliciesType>::value,
                                                    std::function<ReturnType(Args...)>>::Type;
    using HandleType = typename eventpp::CallbackList<ReturnType(Args...), PoliciesType>::Handle;
    struct SignalFuture {
        HandleType handle;
        std::promise<void> p;
    };

public:
    /// @brief bind excutors for this signal
    /// @param callback  the excutor function
    /// @param append_mode true means the excutor will be append to the process deque tail,false means the excutor
    /// will be add to the process deque top
    /// @return the handle which can be used to cancel the excutor binding
    HandleType Connect(const Callback_& callback, bool append_mode = true);
    std::shared_ptr<SignalFuture> MakeSignalFuture() {
        auto ret    = std::make_shared<SignalFuture>();
        ret->handle = Connect([ret, this](Args...) -> void {
            DisConnect(ret->handle);
            ret->p.set_value();
        });
        return ret;
    }
    bool DisConnect(HandleType handle);
    void Emit(Args... args);
    void operator()(Args... args);
    bool operator!() const {
        return _connections.empty();
    }
    operator bool() const {
        return !_connections.empty();
    }

private:
    eventpp::CallbackList<ReturnType(Args...), PoliciesType> _connections;
};

template <typename PoliciesType, typename ReturnType, typename... Args>
inline typename SignalBase<ReturnType(Args...), PoliciesType>::HandleType SignalBase<
    ReturnType(Args...),
    PoliciesType>::Connect(const Callback_& callback, bool append_mode) {
    if (append_mode) {
        return _connections.append(callback);
    } else {
        return _connections.prepend(callback);
    }
}

template <typename PoliciesType, typename ReturnType, typename... Args>
inline bool SignalBase<ReturnType(Args...), PoliciesType>::DisConnect(HandleType handle) {
    return _connections.remove(handle);
}

template <typename PoliciesType, typename ReturnType, typename... Args>
inline void SignalBase<ReturnType(Args...), PoliciesType>::Emit(Args... args) {
    operator()(std::forward<Args>(args)...);
}

template <typename PoliciesType, typename ReturnType, typename... Args>
inline void SignalBase<ReturnType(Args...), PoliciesType>::operator()(Args... args) {
    _connections(std::forward<Args>(args)...);
}

template <typename Prototype_, typename Policies_ = eventpp::DefaultPolicies>
class Signal : public SignalBase<Prototype_, Policies_> {};

} // namespace Fundamental
