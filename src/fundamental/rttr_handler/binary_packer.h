#pragma once

#include <cstdlib>
#include <cstring>

#include "fundamental/basic/buffer.hpp"
#include "fundamental/basic/log.h"
#include <rttr/type>
namespace Fundamental::io {

enum PackerDataType : std::uint8_t {
    unknown_pack_data,
    bool_pack_data,
    char_pack_data,
    int8_pack_data,
    int16_pack_data,
    int32_pack_data,
    int64_pack_data,
    uint8_pack_data,
    uint16_pack_data,
    uint32_pack_data,
    uint64_pack_data,
    float_pack_data,
    double_pack_data,
    string_pack_data,
    enum_pack_data,
    array_pack_data,
    set_pack_data,
    map_pack_data,
    object_pack_data,
    custom_object__pack_data,
};

namespace internal {
bool do_binary_unpack(const std::uint8_t*& data, std::size_t& len, rttr::variant& var, rttr::instance dst_obj,
                      PackerDataType type);

template <typename T>
inline bool unpack_basic_value(const std::uint8_t*& data, std::size_t& len, T& value) {
    constexpr std::size_t value_size = sizeof(value);
    if (len < value_size) {
        FERR("read basic value failed");
        return false;
    }

    if constexpr (value_size > 1 && Fundamental::NeedConvertEndian<Fundamental::Endian::LittleEndian>()) {
        std::uint8_t* p_dst = (std::uint8_t*)(&value);
        for (std::size_t i = 0; i < value_size; ++i) {
            p_dst[value_size - 1 - i] = data[i];
        }
    } else {
        std::memcpy(&value, data, value_size);
    }
    data = data + value_size;
    len -= value_size;
    return true;
}
} // namespace internal
// throw when pack failed
[[nodiscard]] std::vector<std::uint8_t> binary_pack(const rttr::variant& var);

// return false when unpack failed
template <typename T>
inline bool binary_unpack(const void* data, std::size_t len, T& out) {
    auto type = rttr::type::get<T>();
    if (!type.is_valid()) return false;
    rttr::variant var(out);
    auto* p_data             = static_cast<const std::uint8_t*>(data);
    PackerDataType data_type = unknown_pack_data;
    if (!internal::unpack_basic_value(p_data, len, data_type)) return false;
    if (!internal::do_binary_unpack(p_data, len, var, out, data_type)) return false;
    if (data_type != object_pack_data) out = var.get_value<T>();
    return true;
}

} // namespace Fundamental::io