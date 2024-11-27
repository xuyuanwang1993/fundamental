#include "utils.hpp"
#include <pthread.h>
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
} // namespace Utils
} // namespace Fundamental