#include "deserializer.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <rttr/registration>
#include <string>
#include <vector>
using namespace rttr;
namespace {
RTTR_REGISTRATION {
    using namespace rttr;

    { RTTR_REGISTRATION_STANDARD_TYPE_VARIANTS(nlohmann::json); }
}

/////////////////////////////////////////////////////////////////////////////////////////

void fromjson_recursively(instance obj, const Fundamental::json& json_object,
                          const Fundamental::RttrDeserializeOption& option);

/////////////////////////////////////////////////////////////////////////////////////////

variant extract_basic_types(const Fundamental::json& json_value) {
    switch (json_value.type()) {
    case Fundamental::json::value_t::string: {
        return json_value.get<std::string>();
        break;
    }
    case Fundamental::json::value_t::null: {
        break;
    }
    case Fundamental::json::value_t::boolean: {
        return json_value.get<bool>();
        break;
    }
    case Fundamental::json::value_t::number_integer: {
        return json_value.get<int64_t>();
        break;
    }
    case Fundamental::json::value_t::number_float: {
        return json_value.get<double>();
        break;
    }
    case Fundamental::json::value_t::number_unsigned: {
        return json_value.get<uint64_t>();
        break;
    }
    // we handle only the basic types here
    case Fundamental::json::value_t::object:
    case Fundamental::json::value_t::array: {
        return variant();
        break;
    }
    default: break;
    }

    return variant();
}

/////////////////////////////////////////////////////////////////////////////////////////

static void write_array_recursively(variant_sequential_view& view, const Fundamental::json& json_array_value,
                                    const Fundamental::RttrDeserializeOption& option) {
    view.set_size(json_array_value.size());
    const type array_value_type = view.get_rank_type(1);

    for (size_t i = 0; i < json_array_value.size(); ++i) {
        auto& json_index_value = json_array_value[i];
        if (json_index_value.is_array()) {
            auto sub_array_view = view.get_value(i).create_sequential_view();
            write_array_recursively(sub_array_view, json_index_value, option);
        } else if (json_index_value.is_object()) {
            variant var_tmp     = view.get_value(i);
            variant wrapped_var = var_tmp.extract_wrapped_value();
            fromjson_recursively(wrapped_var, json_index_value, option);
            view.set_value(i, wrapped_var);
        } else {
            variant extracted_value = extract_basic_types(json_index_value);
            if (extracted_value.convert(array_value_type)) view.set_value(i, extracted_value);
        }
    }
}

variant extract_value(const Fundamental::json& json_value, const type& t,
                      const Fundamental::RttrDeserializeOption& option) {
    variant extracted_value  = extract_basic_types(json_value);
    const bool could_convert = extracted_value.convert(t);
    if (!could_convert) {
        if (json_value.is_object()) {
            constructor ctor = t.get_constructor();
            for (auto& item : t.get_constructors()) {
                if (item.get_instantiated_type() == t) ctor = item;
            }
            extracted_value = ctor.invoke();
            fromjson_recursively(extracted_value, json_value, option);
        } else if (json_value.is_array()) {
            constructor ctor = t.get_constructor();
            for (auto& item : t.get_constructors()) {
                if (item.get_instantiated_type() == t) ctor = item;
            }
            extracted_value = ctor.invoke();
            auto view       = extracted_value.create_sequential_view();
            write_array_recursively(view, json_value, option);
        }
    }

    return extracted_value;
}

static void write_associative_view_recursively(variant_associative_view& view,
                                               const Fundamental::json& json_array_value,
                                               const Fundamental::RttrDeserializeOption& option) {
    for (size_t i = 0; i < json_array_value.size(); ++i) {
        auto& json_index_value = json_array_value[i];
        if (json_index_value.is_object()) // a key-value associative view
        {
            auto key_itr   = json_index_value.find("key");
            auto value_itr = json_index_value.find("value");

            if (key_itr != json_index_value.end() && value_itr != json_index_value.end()) {
                auto key_var   = extract_value(key_itr.value(), view.get_key_type(), option);
                auto value_var = extract_value(value_itr.value(), view.get_value_type(), option);
                if (key_var && value_var) {
                    view.insert(key_var, value_var);
                }
            }
        } else // a key-only associative view
        {
            variant extracted_value = extract_basic_types(json_index_value);
            if (extracted_value && extracted_value.convert(view.get_key_type())) view.insert(extracted_value);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

void fromjson_recursively(instance obj2, const Fundamental::json& json_object,
                          const Fundamental::RttrDeserializeOption& option) {
    instance obj         = obj2.get_type().get_raw_type().is_wrapper() ? obj2.get_wrapped_instance() : obj2;
    const auto prop_list = obj.get_derived_type().get_properties();

    for (auto& prop : prop_list) {
        // ignore noserialized data
        if (!option.ValidateSerialize(prop)) continue;
        auto iter = json_object.find(prop.get_name().data());
        if (iter == json_object.end()) {
            continue;
        }
        const Fundamental::json& json_value = iter.value();
        const type value_t                  = prop.get_type();

        switch (json_value.type()) {
        case Fundamental::json::value_t::array: {
            variant var;
            if (value_t.is_sequential_container()) {
                var       = prop.get_value(obj);
                auto view = var.create_sequential_view();
                write_array_recursively(view, json_value, option);
            } else if (value_t.is_associative_container()) {
                var                   = prop.get_value(obj);
                auto associative_view = var.create_associative_view();
                write_associative_view_recursively(associative_view, json_value, option);
            }

            prop.set_value(obj, var);
            break;
        }
        case Fundamental::json::value_t::object: {
            variant var = prop.get_value(obj);
            if (var.is_type<nlohmann::json>()) {
                prop.set_value(obj, json_value);
            } else {
                fromjson_recursively(var, json_value, option);
                prop.set_value(obj, var);
            }

            break;
        }
        default: {
            variant extracted_value = extract_basic_types(json_value);
            if (extracted_value.convert(
                    value_t)) // REMARK: CONVERSION WORKS ONLY WITH "const type", check whether this is correct or not!
                prop.set_value(obj, extracted_value);
            else {
                variant var = prop.get_value(obj);
                if (var.is_type<nlohmann::json>()) {
                    prop.set_value(obj, json_value);
                }
            }
        }
        }
    }
}

} // namespace

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
namespace Fundamental {
namespace io {

bool from_json_obj(const Fundamental::json& json, rttr::instance obj, const RttrDeserializeOption& option) {
    fromjson_recursively(obj, json, option);
    return true;
}

rttr::variant from_json_obj(const Fundamental::json& json, const rttr::type& t, const RttrDeserializeOption& option) {
    return extract_value(json, t, option);
}

static bool DfsSetProperties(rttr::variant& var, std::list<std::string>& properties, const std::string& value,
                             const RttrDeserializeOption& option) {
    type t = var.get_type();
    if (properties.empty()) {
        var = from_json(value, t, option);
        return var.is_valid();
    }
    if (t.is_arithmetic() || t.is_enumeration() || t == type::get<std::string>()) {
        return false;
    }
    if (var.is_sequential_container()) {
        decltype(auto) view = var.create_sequential_view();
        try {
            std::size_t index = std::stoul(properties.front());
            if (index >= view.get_size()) return false;
            rttr::variant nextVal = view.get_value(index).extract_wrapped_value();
            properties.pop_front();
            bool ret = DfsSetProperties(nextVal, properties, value, option);
            if (ret) {
                view.set_value(index, nextVal);
            }
            return ret;
        } catch (const std::exception&) {
            return false;
        }
    } else if (var.is_associative_container()) {
        decltype(auto) view = var.create_associative_view();
        if (view.is_key_only_type()) return false;
        std::string key  = properties.front();
        auto nextValIter = view.find(key);
        if (nextValIter == view.end()) return false;
        properties.pop_front();
        rttr::variant nextVal = nextValIter.get_value().extract_wrapped_value();
        bool ret              = DfsSetProperties(nextVal, properties, value, option);
        if (ret) {
            view.erase(key);
            view.insert(key, nextVal);
        }
        return ret;
    }
    rttr::instance obj = var;
    instance objActual = obj.get_type().get_raw_type().is_wrapper() ? obj.get_wrapped_instance() : obj;
    auto prop          = type::get(objActual).get_property(properties.front());
    properties.pop_front();
    if (!prop.is_valid()) return false;
    variant prop_value = prop.get_value(objActual);
    if (!prop_value) return false;
    bool ret = DfsSetProperties(prop_value, properties, value, option);
    if (ret) prop.set_value(objActual, prop_value);
    return ret;
};

bool set_property_by_json(rttr::instance obj, const std::string& propertyPath, const std::string& value,
                          const RttrDeserializeOption& option) {
    if (propertyPath.empty()) return from_json(value, obj, option);
    auto splitFunction = [](const std::string& input, const std::string& sep) -> std::list<std::string> {
        std::list<std::string> chunks;

        size_t offset(0);
        while (true) {
            size_t pos = input.find(sep, offset);

            if (pos == std::string::npos) {
                chunks.push_back(input.substr(offset));
                break;
            }

            chunks.push_back(input.substr(offset, pos - offset));
            offset = pos + sep.length();
        }
        return chunks;
    };
    auto targetProperties = splitFunction(propertyPath, ".");
    if (targetProperties.empty()) return false;
    // limit for max stack size
    if (targetProperties.size() > 256) return false;

    instance objActual = obj.get_type().get_raw_type().is_wrapper() ? obj.get_wrapped_instance() : obj;
    auto prop          = type::get(objActual).get_property(targetProperties.front());
    targetProperties.pop_front();
    if (!prop.is_valid()) return false;
    variant prop_value = prop.get_value(objActual);
    if (!prop_value) return false;

    bool ret = DfsSetProperties(prop_value, targetProperties, value, option);
    if (ret) prop.set_value(objActual, prop_value);
    return ret;
}

bool from_json(const std::string& json, rttr::instance obj, const RttrDeserializeOption& option) {
    Fundamental::json json_obj;
    try {
        json_obj = Fundamental::json::parse(json);
    } catch (...) {
        return false;
    }

    from_json_obj(json_obj, obj, option);

    return true;
}

bool from_json(const void* data, std::size_t dataLen, rttr::instance obj, const RttrDeserializeOption& option) {
    if (!data) return false;
    Fundamental::json json_obj;
    try {
        const char* pData = reinterpret_cast<const char*>(data);
        json_obj          = Fundamental::json::parse(pData, pData + dataLen);
    } catch (...) {
        return false;
    }

    from_json_obj(json_obj, obj, option);

    return true;
}

rttr::variant from_json(const std::string& json, const rttr::type& t, const RttrDeserializeOption& option) {
    Fundamental::json json_obj;
    try {
        json_obj = Fundamental::json::parse(json);
    } catch (...) {
        return false;
    }
    return extract_value(json_obj, t, option);
}

} // namespace io

void TestRttrInstance() {
    std::cout << "rttr instance :" << &rttr::detail::get_registration_manager() << std::endl;
}
// end namespace io
} // namespace Fundamental