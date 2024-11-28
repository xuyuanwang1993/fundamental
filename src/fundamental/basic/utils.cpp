#include "utils.hpp"
#include <iomanip>
#include <pthread.h>
#include <sstream>
namespace Fundamental
{
namespace Utils
{
void SetThreadName(const std::string& name)
{
#if TARGET_PLATFORM_LINUX
    ::pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#endif
}

std::string BufferToHex(const void* buffer, std::size_t size)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    const std::uint8_t* ptr = static_cast<const std::uint8_t*>(buffer);
    for (size_t i = 0; i < size; ++i)
    {
        oss << std::setw(2) << static_cast<int>(ptr[i]);
    }
    return oss.str();
}
std::string BufferDumpAscii(const void* buffer, std::size_t size)
{
    std::stringstream ss;
    const std::uint8_t* ptr = static_cast<const std::uint8_t*>(buffer);
    for (std::size_t i = 0; i < size; ++i)
    {
        char c = ptr[i];
        if (c >= 32 || c <= 126)
            ss << c;
    }
    return ss.str();
}
} // namespace Utils
} // namespace Fundamental