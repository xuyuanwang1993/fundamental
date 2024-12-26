
#pragma once
#include "meta_control.h"
#include "nlohmann/json.hpp"
#include <rttr/type>
#include <string>
namespace Fundamental {
using json = nlohmann::json;
namespace io {
/*
 *1. all struct need register a default constructor
 *2. property path is split by '.'
 *3. stl container must be wrappered by an extra struct
 */
bool from_json(const std::string& jsonStr, rttr::instance obj, const RttrDeserializeOption& option = {});
bool from_json(const void* data, std::size_t dataLen, rttr::instance obj, const RttrDeserializeOption& option = {});
rttr::variant from_json(const std::string& jsonStr, const rttr::type& t, const RttrDeserializeOption& option = {});

bool from_json_obj(const json& jsonObj, rttr::instance obj, const RttrDeserializeOption& option = {});
rttr::variant from_json_obj(const json& jsonObj, const rttr::type& t, const RttrDeserializeOption& option = {});

bool set_property_by_json(rttr::instance obj, const std::string& propertyPath, const std::string& value,
                          const RttrDeserializeOption& option = {});

template <typename DataType>
inline bool EnumTypeFromString(const std::string& str, DataType& type) {
    auto e   = rttr::type::get<DataType>().get_enumeration();
    auto ret = e.name_to_value(str);
    if (!ret.is_valid()) return false;
    type = ret.template get_value<DataType>();
    return true;
}
template <typename EnumType, typename DataType>
inline bool EnumTypeFromString(const std::string& str, DataType& type) {
    auto e   = rttr::type::get<EnumType>().get_enumeration();
    auto ret = e.name_to_value(str);
    if (!ret.is_valid()) return false;
    type = static_cast<DataType>(ret.template get_value<EnumType>());
    return true;
}
} // namespace io
void TestRttrInstance();
} // namespace Fundamental