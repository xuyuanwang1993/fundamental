#pragma once
#include <future>
#include <memory>

namespace Fundamental
{

template <typename T>
inline decltype(auto) MakeSharedPromise()
{
    return std::make_shared<std::promise<T>>();
}
} // namespace Fundamental