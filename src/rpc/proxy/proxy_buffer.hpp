#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <deque>
#include <asio.hpp>

#include "fundamental/basic/allocator.hpp"
namespace network
{
namespace proxy
{
inline static constexpr std::size_t kCacheBufferSize = 32 * 1024; // 32k
inline static constexpr std::size_t kMinPerReadSize  = 1200;
using DataCacheType                                  = std::array<std::uint8_t, kCacheBufferSize>;
struct DataCahceItem {
    DataCacheType data;
    std::size_t readOffset  = 0;
    std::size_t writeOffset = 0;
};

struct EndponitCacheStatus {
    explicit EndponitCacheStatus(decltype(Fundamental::MakePoolMemorySource()) dataSource) : cache_(dataSource.get()) {
    }
    bool is_writing = false;
    std::deque<DataCahceItem, Fundamental::AllocatorType<DataCahceItem>> cache_;
    std::string tag_;
    bool PrepareWriteCache();
    void PrepareReadCache();
    asio::mutable_buffer GetReadBuffer();
    asio::const_buffer GetWriteBuffer();
    void UpdateReadBuffer(std::size_t readBytes);
    void UpdateWriteBuffer(std::size_t writeBytes);
};
} // namespace proxy
} // namespace rpc