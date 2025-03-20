#pragma once
#include "fundamental/basic/utils.hpp"
#include "fundamental/events/event_system.h"
#include "object_definitions.h"
#include <string_view>
#include <tuple>

namespace Fundamental
{
using ExpiredSignalType =
    Fundamental::Signal<Fundamental::SignalBrokenType(std::string_view /*table_name*/, std::string_view /*key*/)>;
template <class derived>
class data_storage_interface{
public:
    using super = derived;

public:
    bool init() {
        return imp().init();
    }

    // return true if persist data success
    bool persist_data(std::string table_name, std::string key, std::string data, const storage_config& config) {
        return imp().persist_data(std::move(table_name), std::move(key), std::move(data), config);
    }

    std::size_t table_size(std::string table_name) const {
        return imp().table_size(std::move(table_name));
    }

    bool has_key(std::string table_name, std::string key) const {
        return imp().has_key(std::move(table_name), std::move(key));
    }

    bool remove_data(std::string table_name, std::string key) {
        return imp().remove_data(std::move(table_name), std::move(key));
    }

    bool update_key_expired_time(std::string table_name, std::string key, std::int64_t update_expired_time_msec) {
        return imp().update_key_expired_time(std::move(table_name), std::move(key), update_expired_time_msec);
    }

    std::tuple<bool, std::string> get_value(std::string table_name, std::string key) const {
        return imp().get_value(std::move(table_name), std::move(key));
    }

    ExpiredSignalType& expired_signal() {
        return imp().expired_signal();
    }

protected:
    derived& imp() {
        return *(static_cast<derived*>(this));
    };
    const derived& imp() const {
        return *(static_cast<const derived*>(this));
    };
};
} // namespace Fundamental