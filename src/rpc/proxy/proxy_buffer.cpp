#include "proxy_buffer.hpp"
#include "fundamental/basic/log.h"

namespace network
{
namespace proxy
{
bool EndponitCacheStatus::PrepareWriteCache() {
    auto& front = cache_.front();
    // old buffer can be removed
    if (front.readOffset == front.writeOffset && cache_.size() > 1) cache_.pop_front();
    if (cache_.size() == 1) {
        auto& back = cache_.back();
        // last buffer has been written finished
        if (back.readOffset == back.writeOffset) {
            return false;
        }
    }
    // normally we should return true,but we can't call async_write_some twice
    // so we check the flag 'is_writing'
    if (is_writing) return false;
    is_writing = true;
    return true;
}

void EndponitCacheStatus::PrepareReadCache() {
    do {
        if (cache_.empty()) { // add a new buffer
            cache_.emplace_back();
            break;
        }
        auto& back = cache_.back();
        if (back.writeOffset + kMinPerReadSize > kCacheBufferSize) { // add a new buffer for
            cache_.emplace_back();
            break;
        }
    } while (0);
}
asio::mutable_buffer EndponitCacheStatus::GetReadBuffer() {
    auto& back = cache_.back();
#ifdef RPC_VERBOSE
    FDEBUG("proxy {} try read {:p} size current_offset:{}", tag_, (void*)&back, back.readOffset);
#endif
    return asio::buffer(back.data.data() + back.readOffset, kCacheBufferSize - back.readOffset);
}
asio::const_buffer EndponitCacheStatus::GetWriteBuffer() {
    auto& front = cache_.front();
#ifdef RPC_VERBOSE
    FDEBUG("proxy {} try write {:p} size {}-{}={}", tag_, (void*)&front, front.readOffset, front.writeOffset,
           front.readOffset - front.writeOffset);
#endif
    return asio::const_buffer(front.data.data() + front.writeOffset, front.readOffset - front.writeOffset);
}
void EndponitCacheStatus::UpdateReadBuffer(std::size_t readBytes) {
    if (readBytes == 0) return;
    auto& back = cache_.back();

#ifdef RPC_VERBOSE
    FDEBUG("proxy {} read {:p} {} {} --> {}", tag_, (void*)&back, readBytes, back.readOffset,
           Fundamental::Utils::BufferToHex(back.data.data() + back.readOffset, readBytes, 140));
#endif
    FASSERT(back.readOffset + readBytes <= kCacheBufferSize, " {}+{}<={}", back.readOffset, readBytes,
            kCacheBufferSize);
    back.readOffset += readBytes;
}

void EndponitCacheStatus::UpdateWriteBuffer(std::size_t writeBytes) {
    is_writing = false;
    if (writeBytes == 0) return;
    auto& front = cache_.front();
#ifdef RPC_VERBOSE
    FDEBUG("proxy {} write {:p} {} {} --> {}", (void*)&front, tag_, writeBytes, front.writeOffset,
           Fundamental::Utils::BufferToHex(front.data.data() + front.writeOffset, writeBytes, 140));
#endif
    FASSERT(front.writeOffset + writeBytes <= front.readOffset, "{}+{} <={}", front.writeOffset, writeBytes,
            front.readOffset);
    front.writeOffset += writeBytes;
}
} // namespace proxy
} // namespace network