#include "fundamental/basic/allocator.hpp"

#include <array>
#include <iostream>
#include <memory_resource>
#include <vector>
#include <deque>


class tracer_memory_resource : public std::pmr::memory_resource
{
public:
    explicit tracer_memory_resource(std::pmr::memory_resource* refsource) :
    ref(refsource)
    {
    }

protected:
    void*
    do_allocate(size_t __bytes, size_t __alignment) override
    {
        std::cout << "allocate " << __bytes << std::endl;
        return ref->allocate(__bytes, __alignment);
    }

    void
    do_deallocate(void* __p, size_t __bytes, size_t __alignment) override
    {
        std::cout << "deallocate " << __bytes << std::endl;
        ref->deallocate(__p, __bytes, __alignment);
    }

    bool
    do_is_equal(const memory_resource& __other) const noexcept override
    {
        return this == &__other;
    }

private:
    std::pmr::memory_resource* const ref;
};

int main()
{
    std::pmr::memory_resource* upstream = std::pmr::new_delete_resource();
    tracer_memory_resource source(upstream);
    std::pmr::pool_options options;
    constexpr std::size_t kLargeBlockSize = 4098;
    options.largest_required_pool_block   = kLargeBlockSize;
    options.max_blocks_per_chunk          = 4;
    
    auto poolResource= Fundamental::MakePoolMemorySource(options, &source);

    std::pmr::memory_resource* activeSource = poolResource.get();
    {
        std::cout << "test int vec " << std::endl;
        std::pmr::vector<int> myVector { activeSource };

        for (int i = 0; i < 32; ++i)
        {
            std::cout<<"emplace"<<std::endl;
            myVector.push_back(i);
        }
        std::cout << "clear" << std::endl;
        myVector.clear();
        std::cout << "shrink_to_fit" << std::endl;
        myVector.shrink_to_fit();
    }

    struct LargeBlock
    {
        char data[kLargeBlockSize * 2+1];
    };
    {
        std::cout << "test large block vec " << sizeof(LargeBlock) << std::endl;
        std::pmr::vector<LargeBlock> testVec { activeSource };
        for (int i = 0; i < 10; ++i)
        {
            std::cout<<"emplace"<<std::endl;
            testVec.emplace_back();
        }
            
        std::cout << "clear" << std::endl;
        testVec.clear();
        std::cout << "shrink_to_fit" << std::endl;
        testVec.shrink_to_fit();
    }
    {
        std::deque<int,std::pmr::polymorphic_allocator<int>> dq;
    }
    std::cout << "finished" << std::endl;
    return 0;
}