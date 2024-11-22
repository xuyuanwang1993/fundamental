
#include "fundamental/events/event_system.h"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include <condition_variable>
#include <fstream>
#include <future>
#include <memory>
#if 1
static Fundamental::EventSystemWrapper* g_nativeEventSystem = nullptr;
static std::unique_ptr<std::thread> s_nativeLoopThread;
static bool g_exitFlag = false;

namespace Events
{
struct NativeNotifyEvents : Fundamental::Event
{
    NativeNotifyEvents(const char* _msg, std::size_t _index) :
    Event(0), msg(_msg), index(_index)
    {
    }
    std::string msg;
    std::size_t index;
};

struct NativeCallEvents : Fundamental::Event
{
    // an event must contain 'kEventType' field;
    constexpr inline static std::size_t kEventType = 1;
    NativeCallEvents(const char* _msg, std::size_t _index) :
    Event(1), msg(_msg), index(_index)
    {
    }
    std::string msg;
    std::size_t index;
};
} // namespace Events

static void NativeLoop();
static void NativeBindEventListener();
static void StressTesting(std::size_t eventNum);
int main(int argc, char* argv[])
{
    using EventType = Events::NativeNotifyEvents;
    // init native
    g_nativeEventSystem = new Fundamental::EventSystemWrapper();
    NativeBindEventListener();
    { // test disaptcher directly
        g_nativeEventSystem->DispatcherImmediateEvent<Events::NativeCallEvents>("test", 0);
        g_nativeEventSystem->DispatcherImmediateEvent<Events::NativeNotifyEvents>("test", 0);
    }

    s_nativeLoopThread = std::make_unique<std::thread>(NativeLoop);

    std::size_t index = 0;
    while (index < 50)
    {
        g_nativeEventSystem->DispatcherImmediateEvent<Events::NativeCallEvents>("test", index);
        g_nativeEventSystem->DispatcherEvent<Events::NativeCallEvents>("test", index);
        ++index;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    g_exitFlag = true;
    s_nativeLoopThread->join();
    delete g_nativeEventSystem;
    StressTesting(1000000);
    return 0;
}

void NativeLoop()
{
    g_nativeEventSystem->Init();

    while (!g_exitFlag)
    {
        g_nativeEventSystem->EventsTick();
        // do some logic handler
        std::this_thread::yield();
    }

    g_nativeEventSystem->Release();
}

void NativeBindEventListener()
{
    {
        using BindEventType  = Events::NativeNotifyEvents;
        auto eventListenFunc = [](const Fundamental::EventPointerType& ptr) {
            auto raw = static_cast<const BindEventType*>(ptr.get());
            // we need to check event type and event size
            FINFO("thread:{} recv(NativeNotifyEvents):{} index:{}", syscall(SYS_gettid), raw->msg, raw->index);
        };
        g_nativeEventSystem->AddEventListener(0, eventListenFunc);
    }
    {
        using BindEventType  = Events::NativeCallEvents;
        auto eventListenFunc = [](const Fundamental::EventPointerType& ptr) {
            auto raw = static_cast<const BindEventType*>(ptr.get());
            // we need to check event type and event size
            FINFO("thread:{} recv(NativeCallEvents):{} index:{}", syscall(SYS_gettid), raw->msg, raw->index);
        };
        g_nativeEventSystem->AddEventListener(1, eventListenFunc);
    }
}
void StressTesting(std::size_t eventNum)
{
    using EventType = Events::NativeNotifyEvents;
    // init native
    g_nativeEventSystem = new Fundamental::EventSystemWrapper();
    std::size_t count   = 0;
    std::condition_variable exitCV;
    std::mutex syncMutex;
    {
        using BindEventType  = Events::NativeNotifyEvents;
        auto eventListenFunc = [&](const Fundamental::EventPointerType& ptr) {
            ++count;
            if (count == eventNum)
            {
                std::scoped_lock<std::mutex> locker(syncMutex);
                exitCV.notify_one();
            }
        };
        g_nativeEventSystem->AddEventListener(0, eventListenFunc);
    }
    g_exitFlag      = false;
    s_nativeLoopThread = std::make_unique<std::thread>(NativeLoop);
    std::size_t index  = 0;
    FINFO("start stress testing eventNum={}", eventNum);
    auto startTimeMsec = Fundamental::Timer::GetTimeNow();
    while (index < eventNum)
    {
        g_nativeEventSystem->DispatcherEvent<Events::NativeNotifyEvents>("test", index);
        ++index;
    }
    {
        std::unique_lock<std::mutex> locker(syncMutex);
        exitCV.wait(locker);
    }
    FINFO("start stress testing eventNum={} cots [{}ms]", eventNum, Fundamental::Timer::GetTimeNow() - startTimeMsec);
    g_exitFlag = true;
    s_nativeLoopThread->join();
    delete g_nativeEventSystem;
}
#else
int main()
{
    return 0;
}
#endif
