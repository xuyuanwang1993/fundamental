
#include "event_process.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "fundamental/basic/log.h"
#include "event_system.h"
#include <unordered_map>
namespace Fundamental
{
struct EventsHandler::EventsModelData
{
    ~EventsModelData();
    // ref models
    std::unordered_map<EventsModel*, std::string> eventsModels;
    std::mutex notifyMutex;
    std::condition_variable cv;
    void Wait(std::int64_t timeMsec);
    void WakeUp();
    bool NeedProcessing();
    void ProcessEvents();
};

EventsHandler::EventsHandler() :
pEventSystem(new Fundamental::EventSystemWrapper),
pDelayQueue(new Fundamental::DelayQueue),
pEventsModelData(new EventsModelData())
{
    {
        using RegisterEventType = Fundamental::EngineProcessEvent;
        auto func               = [](const Fundamental::EventPointerType& event) {
            auto* ptr = Fundamental::ConstCastEvent<RegisterEventType>(event);
            if (ptr->task)
                ptr->task();
        };
        pEventSystem->AddEventListener(RegisterEventType::kEventType, func);
    }
    {
        using RegisterEventType = Fundamental::InternalNotifyQueuedEvent;
        auto func               = [=](const Fundamental::EventPointerType& event) {
            pEventsModelData->WakeUp();
        };
        pEventSystem->AddEventListener(RegisterEventType::kEventType, func);
    }
    pDelayQueue->SetStateChangedCallback([=]() {
        pEventsModelData->WakeUp();
    });
}

EventsHandler::~EventsHandler()
{
    if (pEventsModelData)
        delete pEventsModelData;
    if (pDelayQueue)
        delete pDelayQueue;
    if (pEventSystem)
        delete pEventSystem;
}

void EventsHandler::Init()
{
    // do nothing for default implementation
    for (auto& item : pEventsModelData->eventsModels)
    {
        item.first->Init();
    }
}

void EventsHandler::Tick()
{
    // handle queued event
    pEventSystem->EventsTick();
    // handle delay task
    pDelayQueue->HandleEvent();
    pEventsModelData->ProcessEvents();
    if (pEventSystem->IsIdle()&&!pEventsModelData->NeedProcessing())
    {
        auto waitTimeMsec = pDelayQueue->GetNextTimeoutMsec();
        if (waitTimeMsec > 10 || waitTimeMsec <= 0)
            waitTimeMsec = 10;
        pEventsModelData->Wait(waitTimeMsec);
    }
}

void EventsHandler::PostProcessEvent(const std::function<void()>& event)
{
    pEventSystem->DispatcherEvent<Fundamental::EngineProcessEvent>(event);
}

void EventsHandler::WakeUp()
{
    pEventsModelData->WakeUp();
}

EventsHandler::EventsModelData::~EventsModelData()
{
    eventsModels.clear();
}

void EventsHandler::EventsModelData::Wait(std::int64_t timeMsec)
{

    std::unique_lock<std::mutex> locker(notifyMutex);
    if (timeMsec > 0)
    {
        cv.wait_for(locker, std::chrono::milliseconds(timeMsec));
    }
    else
    {
        cv.wait(locker);
    }
}

void EventsHandler::EventsModelData::WakeUp()
{
    std::scoped_lock<std::mutex> locker(notifyMutex);
    cv.notify_one();
}

bool EventsHandler::EventsModelData::NeedProcessing()
{
    for (auto& item : eventsModels)
    {
        if (!item.first->IsIdle())
            return true;
    }
    return false;
}

void EventsHandler::EventsModelData::ProcessEvents()
{
    for (auto& item : eventsModels)
    {
        item.first->Tick();
    }
}

EventsModel::EventsModel(EventsHandler* handleRef, const std::string& description) :
pHandleRef(handleRef)
{
    FINFO("events model [{}] :{} registered", description, std::intptr_t(this));
    pHandleRef->pEventsModelData->eventsModels.emplace(this, description);
}

EventsModel::~EventsModel()
{
}

void EventsModel::Init()
{
}

bool EventsModel::IsIdle()
{
    return true;
}
void EventsModel::Tick()
{
}
} // namespace Fundamental