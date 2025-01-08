#pragma once
#include <memory>
#include <memory_resource>
#ifdef AllocatorTracker
    #include <iostream>
#endif
namespace Fundamental {
template <typename _Tp>
using AllocatorType = std::pmr::polymorphic_allocator<_Tp>;

template <typename MemorySourceType, typename... Args>
decltype(auto) MakeSharedMemorySource(Args&&... args) {
    return std::make_shared<MemorySourceType>(std::forward<Args>(args)...);
}

template <typename... Args>
decltype(auto) MakePoolMemorySource(Args&&... args) {
    return MakeSharedMemorySource<std::pmr::unsynchronized_pool_resource>(std::forward<Args>(args)...);
}

template <typename... Args>
decltype(auto) MakeThreadSafePoolMemorySource(Args&&... args) {
    return MakeSharedMemorySource<std::pmr::synchronized_pool_resource>(std::forward<Args>(args)...);
}

template <typename... Args>
decltype(auto) MakeMonoBufferMemorySource(Args&&... args) {
    return MakeSharedMemorySource<std::pmr::monotonic_buffer_resource>(std::forward<Args>(args)...);
}
namespace internal {
template <size_t ObjectSize>
inline constexpr decltype(auto) GetPoolOption() {
    return std::pmr::pool_options { 4, ObjectSize * 4 };
}
} // namespace internal

// fixed-size object allocator
template <typename ObjectType, bool ThreadSafe,
          typename PoolSourceType = std::conditional_t<ThreadSafe, std::pmr::synchronized_pool_resource,
                                                       std::pmr::unsynchronized_pool_resource>>
struct ObjectPoolAllocator : std::pmr::polymorphic_allocator<ObjectType> {
    using PoolSourceType_ = PoolSourceType;
    std::shared_ptr<std::pmr::memory_resource> source;
    ObjectPoolAllocator() :
    std::pmr::polymorphic_allocator<ObjectType>(new PoolSourceType(internal::GetPoolOption<sizeof(ObjectType)>())),
    source(std::shared_ptr<std::pmr::memory_resource>(std::pmr::polymorphic_allocator<ObjectType>::resource())) {
    }
#ifdef AllocatorTracker
    [[nodiscard]]
    ObjectType* allocate(size_t __n) __attribute__((__returns_nonnull__)) {
        if ((__gnu_cxx::__int_traits<size_t>::__max / sizeof(ObjectType)) < __n) std::__throw_bad_array_new_length();
        std::cout << "allocate align_size:" << alignof(ObjectType) << " total:" << __n * sizeof(ObjectType)
                  << " count:" << __n << std::endl;
        return static_cast<ObjectType*>(source->allocate(__n * sizeof(ObjectType), alignof(ObjectType)));
    }

    void deallocate(ObjectType* __p, size_t __n) noexcept __attribute__((__nonnull__)) {
        std::cout << "deallocate align_size:" << alignof(ObjectType) << " total:" << __n * sizeof(ObjectType)
                  << " count:" << __n << std::endl;
        source->deallocate(__p, __n * sizeof(ObjectType), alignof(ObjectType));
    }
#endif

    template <typename... _CtorArgs>
    [[nodiscard]] ObjectType* NewObject(_CtorArgs&&... __ctor_args) {
        auto* __p = std::pmr::polymorphic_allocator<ObjectType>::allocate(1);
        __try {
            std::pmr::polymorphic_allocator<ObjectType>::construct(__p, std::forward<_CtorArgs>(__ctor_args)...);
        }
        __catch(...) {
            std::pmr::polymorphic_allocator<ObjectType>::deallocate(__p, 1);
            __throw_exception_again;
        }
        return __p;
    }

    void DeleteObject(ObjectType* __p) {
        std::pmr::polymorphic_allocator<ObjectType>::destroy(__p);
        std::pmr::polymorphic_allocator<ObjectType>::deallocate(__p, 1);
    }
};

template <typename ObjectType>
struct ThreadSafeObjectPoolAllocator : ObjectPoolAllocator<ObjectType, true> {};

template <typename ObjectType>
struct ThreadUnSafeObjectPoolAllocator : ObjectPoolAllocator<ObjectType, false> {};
} // namespace Fundamental