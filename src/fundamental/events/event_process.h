
#pragma once

#include "event_system.h"
#include <string>
namespace Fundamental
{

//---------------------------------------------------------------//
struct EngineProcessEvent : Fundamental::Event
{
    using TaskType = std::function<void()>;
    // an event must contain 'kEventType' field;
    constexpr inline static std::size_t kEventType = ComputeEventHash(0, "EngineProcessEvent", "EngineProcessEvent", "Interface");

    EngineProcessEvent(const TaskType& task) :
    Event(kEventType),
    task(task)
    {
    }
    TaskType task;
};

class DelayQueue;
class EventSystemWrapper;

// EventsHandler will add a listener for Events::EngineProcessEvent by default
struct EventsModel;
struct  EventsHandler
{
    friend struct EventsModel;
    struct EventsModelData;
    EventsHandler();
    virtual ~EventsHandler();
    virtual void Init();
    void PostProcessEvent(const std::function<void()>& event);
    void WakeUp();
    virtual void Tick();
    Fundamental::EventSystemWrapper* pEventSystem = nullptr;
    Fundamental::DelayQueue* pDelayQueue          = nullptr;
    EventsModelData* pEventsModelData                = nullptr;
};

struct  EventsModel
{
    EventsModel(EventsHandler* handleRef, const std::string& description);
    virtual ~EventsModel();
    virtual void Init();
    virtual bool IsIdle();
    virtual void Tick();
    EventsHandler* const pHandleRef = nullptr;
};

} // namespace Fundamental