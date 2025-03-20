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

template <class derived_t>
class data_storage_interface {

public:
    bool init() {
        return derived_t::init();
    }

    std::size_t table_size(std::string table_name) const {
        return derived_t::table_size(std::move(table_name));
    }

    bool has_key(std::string table_name, std::string key) const {
        return derived_t::has_key(std::move(table_name), std::move(key));
    }

    bool remove_data(std::string table_name, std::string key) {
        return derived_t::remove_data(std::move(table_name), std::move(key));
    }

    bool update_key_expired_time(std::string table_name, std::string key, std::int64_t update_expired_time_msec) {
        return derived_t::update_key_expired_time(std::move(table_name), std::move(key), update_expired_time_msec);
    }

    ExpiredSignalType& expired_signal() {
        return derived_t::expired_signal();
    }
};

template <typename value_type, class derived_t>
class data_storage_acessor {
public:
    // return true if persist data success
    bool persist_data(std::string table_name, std::string key, value_type data, const storage_config& config) {
        return derived_t::persist_data(std::move(table_name), std::move(key), std::move(data), config);
    }

    std::tuple<bool, value_type> get_value(std::string table_name, std::string key) const {
        return derived_t::get_value(std::move(table_name), std::move(key));
    }
};

template <class derived_t>
class data_storage_acessor<void, derived_t> {
public:
    using value_type = void;

public:
    bool persist_data(std::string table_name, std::string key, const storage_config& config) {
        return derived_t::persist_data(std::move(table_name), std::move(key), config);
    }
};
} // namespace Fundamental