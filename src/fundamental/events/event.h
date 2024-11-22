#pragma once
#include <functional>
#include <memory>
#include <set>
#include <cassert>
namespace Fundamental
{
using EventType = std::size_t;
struct Event
{
    explicit Event(EventType type) :
    eventType(type)
    {
    }

    virtual ~Event()
    {
    }
    const EventType eventType;
};

using EventPointerType  = std::shared_ptr<Event>;
using EventCallbackType = std::function<void(const EventPointerType&)>;

struct EventPolicy
{
    static EventType getEvent(const EventPointerType& event)
    {
        return event->eventType;
    }
};


inline constexpr std::size_t ComputeStrHash(const char* str, std::size_t seed)
{
    if (!str)
        return seed;
    std::size_t i = 0;
    while (std::size_t ch = static_cast<std::size_t>(str[i]))
    {
        seed = seed * 65599 + ch;
        ++i;
    }

    return seed;
}

/*
 * compute str hash helper
 */
template <typename First,
          typename... Rest>
inline constexpr std::size_t ComputeEventHash(std::size_t seed, First first, Rest... rest)
{
    seed = ComputeStrHash(first, seed);
    // Recusively iterate all levels
    if constexpr (sizeof...(Rest) > 0)
    {
        seed = ComputeEventHash<Rest...>(seed, std::forward<Rest>(rest)...);
    }
    return seed;
}


template <typename TargetType>
auto ConstCastEvent(const Fundamental::EventPointerType& event)
{
    using RawType = std::decay_t<TargetType>;
    assert(event.get() && RawType::kEventType == event->eventType);
    return reinterpret_cast<const RawType*>(event.get());
}

template <typename TargetType>
auto CastEvent(const Fundamental::EventPointerType& event)
{
    using RawType = std::decay_t<TargetType>;
    return const_cast<RawType*>(ConstCastEvent<RawType>(event));
}
} // namespace Fundamental