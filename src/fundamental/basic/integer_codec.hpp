#pragma once
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace Fundamental
{


template <typename T, typename = std::enable_if_t<std::is_signed_v<T>>>
struct PairSignedWithUnsignedType {
    using SignedType                   = T;
    using UnsignedType                 = std::make_unsigned_t<T>;
    static constexpr std::size_t shift = sizeof(T) * 8 - 1;
};

template <typename T, typename = std::enable_if_t<std::is_signed_v<T>>>
[[nodiscard]] inline decltype(auto) ZigZagEncode(T value) noexcept {
    return static_cast<typename PairSignedWithUnsignedType<T>::UnsignedType>(
        (value << 1) ^ (value >> PairSignedWithUnsignedType<T>::shift));
}
template <typename T, typename = std::enable_if_t<std::is_signed_v<T>>>
[[nodiscard]] inline T ZigZagDecode(typename PairSignedWithUnsignedType<T>::UnsignedType value) noexcept {
    return static_cast<T>((value >> 1) ^ -(value & 1));
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
[[nodiscard]] inline std::size_t VarintEncode(T inValue, std::uint8_t* dst) noexcept {
    if constexpr (sizeof(T) == 1)
    {
        dst[0] = static_cast<std::uint8_t>(inValue);
        return 1;
    }
    if constexpr (std::is_signed_v<T>)
    {
        return VarintEncode(ZigZagEncode(inValue), dst);
    }
    std::size_t extra_bytes = 0;
    dst[extra_bytes]        = static_cast<std::uint8_t>(inValue & 0xf);
    inValue                 = inValue >> 4;
    while (inValue > 0)
    {
        ++extra_bytes;
        dst[extra_bytes] = static_cast<std::uint8_t>(inValue & 0xff);
        inValue          = inValue >> 8;
    }
    dst[0] |= (static_cast<std::uint8_t>(extra_bytes)) << 4;
    return extra_bytes + 1;
}
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
[[nodiscard]] inline std::size_t VarintDecode(T& outValue, const std::uint8_t* src) noexcept {
    outValue = 0;
    if constexpr (sizeof(T) == 1)
    {
        outValue = static_cast<T>(src[0]);
        return 1;
    }
    if constexpr (std::is_signed_v<T>)
    {
        typename PairSignedWithUnsignedType<T>::UnsignedType tmp;
        auto ret = VarintDecode(tmp, src);
        outValue = ZigZagDecode<T>(tmp);
        return ret;
    }
    std::size_t extra_bytes = src[0] >> 4;
    assert(extra_bytes <= sizeof(T) && "invalid decode stc data");
    outValue          = static_cast<T>(src[0] & 0xf);
    std::size_t shift = 4;
    src += 1;
    for (std::size_t i = 0; i < extra_bytes; ++i)
    {
        outValue |= static_cast<T>(src[i]) << shift;
        shift += 8;
    }
    return extra_bytes + 1;
}

} // namespace Fundamental