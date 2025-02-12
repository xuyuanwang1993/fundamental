#pragma once
#include <string>
#include <string_view>
#include <tuple>

using string_view = std::string_view;

#include "rpc/basic/codec.h"

namespace network {
inline bool has_error(string_view result) {
    if (result.empty()) {
        return true;
    }

    rpc_service::msgpack_codec codec;
    auto tp = codec.unpack<std::tuple<int>>(result.data(), result.size());

    return std::get<0>(tp) != 0;
}

template <typename T>
inline T get_result(string_view result) {
    rpc_service::msgpack_codec codec;
    auto tp = codec.unpack<std::tuple<int, T>>(result.data(), result.size());
    return std::get<1>(tp);
}

inline std::string get_error_msg(string_view result) {
    rpc_service::msgpack_codec codec;
    auto tp = codec.unpack<std::tuple<int, std::string>>(result.data(), result.size());
    return std::get<1>(tp);
}

template <typename T>
inline T as(string_view result) {
    if (has_error(result)) {
        throw std::logic_error(get_error_msg(result));
    }

    return get_result<T>(result);
}
} // namespace network