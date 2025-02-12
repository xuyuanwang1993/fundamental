#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "serializer.h"

#include <rttr/registration>
using namespace rttr;
RTTR_REGISTRATION {
    using namespace rttr;

    { RTTR_REGISTRATION_STANDARD_TYPE_VARIANTS(nlohmann::json); }
}

namespace {

/////////////////////////////////////////////////////////////////////////////////////////
Fundamental::json to_json_recursively(const instance& obj, const Fundamental::RttrSerializeOption& option);

/////////////////////////////////////////////////////////////////////////////////////////

Fundamental::json write_variant(const variant& var, bool& flag, const Fundamental::RttrSerializeOption& option);
Fundamental::json write_variant(const variant& var, const Fundamental::RttrSerializeOption& option);

bool write_atomic_types_to_json(const type& t, const variant& var, Fundamental::json& json_obj) {
    if (t.is_arithmetic()) {
        if (t == type::get<bool>())
            json_obj = var.to_bool();
        else if (t == type::get<char>())
            json_obj = var.to_int8();
        else if (t == type::get<int8_t>())
            json_obj = var.to_int8();
        else if (t == type::get<int16_t>())
            json_obj = var.to_int16();
        else if (t == type::get<int32_t>())
            json_obj = var.to_int32();
        else if (t == type::get<int64_t>())
            json_obj = var.to_int64();
        else if (t == type::get<uint8_t>())
            json_obj = var.to_uint8();
        else if (t == type::get<uint16_t>())
            json_obj = var.to_uint16();
        else if (t == type::get<uint32_t>())
            json_obj = var.to_uint32();
        else if (t == type::get<uint64_t>())
            json_obj = var.to_uint64();
        else if (t == type::get<float>())
            json_obj = var.to_double();
        else if (t == type::get<double>())
            json_obj = var.to_double();

        return true;
    } else if (t.is_enumeration()) {
        bool ok     = false;
        auto result = var.to_string(&ok);
        if (ok) {
            json_obj = var.to_string();
        } else {
            ok         = false;
            auto value = var.to_uint64(&ok);
            if (ok)
                json_obj = value;
            else
                json_obj = Fundamental::json(); // Null
        }

        return true;
    } else if (t == type::get<std::string>()) {
        json_obj = var.to_string();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////

static Fundamental::json write_array(const variant_sequential_view& view,
                                     const Fundamental::RttrSerializeOption& option) {
    Fundamental::json json_array = Fundamental::json::array();
    for (const auto& item : view) {
        if (item.is_sequential_container()) {
            Fundamental::json child = write_array(item.create_sequential_view(), option);
            json_array.push_back(child);
        } else {
            variant wrapped_var = item.extract_wrapped_value();
            type value_type     = wrapped_var.get_type();
            if (value_type.is_arithmetic() || value_type == type::get<std::string>() || value_type.is_enumeration()) {
                Fundamental::json child;
                write_atomic_types_to_json(value_type, wrapped_var, child);
                json_array.push_back(child);
            } else // object
            {
                Fundamental::json child = to_json_recursively(wrapped_var, option);
                json_array.push_back(child);
            }
        }
    }
    return json_array;
}

/////////////////////////////////////////////////////////////////////////////////////////

static Fundamental::json write_associative_container(const variant_associative_view& view,
                                                     const Fundamental::RttrSerializeOption& option) {
    static const std::string key_name("key");
    static const std::string value_name("value");

    Fundamental::json json_array = Fundamental::json::array();

    if (view.is_key_only_type()) {
        for (auto& item : view) {
            Fundamental::json child = write_variant(item.first, option);
            json_array.push_back(child);
        }
    } else {
        for (auto& item : view) {
            Fundamental::json child = Fundamental::json::object();
            child[key_name]         = write_variant(item.first, option);
            child[value_name]       = write_variant(item.second, option);

            json_array.push_back(child);
        }
    }

    return json_array;
}

/////////////////////////////////////////////////////////////////////////////////////////
Fundamental::json write_variant(const variant& var, bool& flag, const Fundamental::RttrSerializeOption& option) {
    flag = true;

    Fundamental::json json_obj;
    auto value_type   = var.get_type();
    auto wrapped_type = value_type.is_wrapper() ? value_type.get_wrapped_type() : value_type;
    bool is_wrapper   = wrapped_type != value_type;

    if (write_atomic_types_to_json(is_wrapper ? wrapped_type : value_type,
                                   is_wrapper ? var.extract_wrapped_value() : var, json_obj)) {
    } else if (var.is_sequential_container()) {
        json_obj = write_array(var.create_sequential_view(), option);
    } else if (var.is_associative_container()) {
        json_obj = write_associative_container(var.create_associative_view(), option);
    } else if (var.is_type<nlohmann::json>()) {
        return var.get_value<nlohmann::json>();
    } else {

        decltype(wrapped_type.get_properties()) child_props =
            is_wrapper ? wrapped_type.get_properties() : value_type.get_properties();
        if (!child_props.empty()) {
            json_obj = to_json_recursively(var, option);
        } else {
            flag     = false;
            json_obj = var.to_string(&flag);
            if (!flag) {
                json_obj = nullptr;
            }
        }
    };

    return json_obj;
}

Fundamental::json write_variant(const variant& var, const Fundamental::RttrSerializeOption& option) {
    bool flag;
    return write_variant(var, flag, option);
}

/////////////////////////////////////////////////////////////////////////////////////////

Fundamental::json to_json_recursively(const instance& obj2, const Fundamental::RttrSerializeOption& option) {
    Fundamental::json json_obj = Fundamental::json::object();

    instance obj = obj2.get_type().get_raw_type().is_wrapper() ? obj2.get_wrapped_instance() : obj2;

    auto prop_list = obj.get_derived_type().get_properties();
    for (auto& prop : prop_list) {
        if (!option.ValidateSerialize(prop)) continue;

        variant prop_value = prop.get_value(obj);
        if (!prop_value) continue; // cannot serialize, because we cannot retrieve the value

        const auto name = prop.get_name();
        bool flag;
        Fundamental::json child = write_variant(prop_value, flag, option);
        if (!flag) {
            std::cerr << "cannot serialize property: " << name << std::endl;
        }
        json_obj[name.to_string()] = child;
    }

    return json_obj;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

namespace Fundamental {
namespace io {

std::string to_json(const rttr::variant& var, const RttrSerializeOption& option) {
    if (var.is_type<nlohmann::json>()) {
        return var.get_value<nlohmann::json>().dump(4);
    }
    Fundamental::json json_obj = to_json_obj(var, option);

    return json_obj.dump(4);
}

Fundamental::json to_json_obj(const rttr::variant& var, const RttrSerializeOption& option) {
    // optimisation for void data
    if (var.is_type<void>()) {
        return Fundamental::json(nullptr);
    }

    return write_variant(var, option);
}

} // end namespace io
} // namespace Fundamental