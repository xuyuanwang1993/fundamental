#pragma once
#include <memory>
#include <memory_resource>
namespace Fundamental
{
template<typename _Tp>
using AllocatorType =std::pmr::polymorphic_allocator<_Tp>;

template <typename MemorySourceType, typename... Args>
decltype(auto) MakeSharedMemorySource(Args&&... args)
{
    return std::make_shared<MemorySourceType>(std::forward<Args>(args)...);
}

template <typename... Args>
decltype(auto) MakePoolMemorySource(Args&&... args)
{
    return MakeSharedMemorySource<std::pmr::unsynchronized_pool_resource>(std::forward<Args>(args)...);
}

template <typename... Args>
decltype(auto) MakeThreadSafePoolMemorySource(Args&&... args)
{
    return MakeSharedMemorySource<std::pmr::synchronized_pool_resource>(std::forward<Args>(args)...);
}

template <typename... Args>
decltype(auto) MakeMonoBufferMemorySource(Args&&... args)
{
    return MakeSharedMemorySource<std::pmr::monotonic_buffer_resource>(std::forward<Args>(args)...);
}

} // namespace Fundamental