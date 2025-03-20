#pragma once
#include "data_storage_interface.hpp"

#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"

#include <unordered_map>

namespace Fundamental
{

namespace storage
{
template <typename DataType, typename = std::void_t<>>
struct memory_storage_item {
    DataType data;
    Fundamental::DelayQueue::HandleType handle = Fundamental::DelayQueue::kInvalidHandle;
};

template <typename DataType>
struct memory_storage_item<DataType, std::void_t<std::enable_if_t<std::is_void_v<DataType>>>> {
    Fundamental::DelayQueue::HandleType handle = Fundamental::DelayQueue::kInvalidHandle;
};

template <typename ValueType>
class memory_storage_data : public data_storage_interface<memory_storage_data<ValueType>> {
protected:
    using storage_table_type  = std::unordered_map<std::string, storage::memory_storage_item<ValueType>>;
    using storage_tables_type = std::unordered_map<std::string, storage_table_type>;

public:
    using value_type = ValueType;

public:
    memory_storage_data(DelayQueue* delay_queue) : delay_queue(delay_queue) {
        FASSERT_ACTION(delay_queue != nullptr, throw std::runtime_error("need a valid delay queue ref"),
                       "need a valid delay queu ref");
    }
    ~memory_storage_data() {
    }
    bool init() {
        return true;
    }

    std::size_t table_size(std::string table_name) const {
        std::scoped_lock<std::mutex> locker(data_mutex);
        auto table_iter = storage.find(table_name);
        return table_iter == storage.end() ? 0 : table_iter->second.size();
    }

    bool has_key(std::string table_name, std::string key) const {
        std::scoped_lock<std::mutex> locker(data_mutex);
        auto table_iter = storage.find(table_name);
        return table_iter == storage.end() ? false : table_iter->second.count(key) > 0;
    }

    bool remove_data(std::string table_name, std::string key) {
        std::scoped_lock<std::mutex> locker(data_mutex);
        auto table_iter = storage.find(table_name);
        return table_iter == storage.end() ? false : table_iter->second.erase(key) > 0;
    }
    bool update_key_expired_time(std::string table_name, std::string key, std::int64_t update_expired_time_msec) {
        std::scoped_lock<std::mutex> locker(data_mutex);
        auto table_iter = storage.find(table_name);
        if (table_iter == storage.end()) return false;
        auto data_iter = table_iter->second.find(key);
        return data_iter == table_iter->second.end()
                   ? false
                   : delay_queue->ModifyTaskNextExpiredTimepoint(data_iter->second.handle, update_expired_time_msec);
    }

    ExpiredSignalType& expired_signal() {
        return expired_signal_;
    }

protected:
    void release_data() {
        std::scoped_lock<std::mutex> locker(data_mutex);
        for (auto& table : storage) {
            for (auto& item : table.second) {
                delay_queue->StopDelayTask(item.second.handle);
            }
        }
    }

protected:
    ExpiredSignalType expired_signal_;
    mutable std::mutex data_mutex;
    DelayQueue* const delay_queue;
    storage_tables_type storage;
};

} // namespace storage

template <typename ValueType = std::string>
class memory_storage : public storage::memory_storage_data<ValueType>,
                       public data_storage_accessor<ValueType, memory_storage<ValueType>> {
public:
    using value_type         = ValueType;
    using super              = storage::memory_storage_data<ValueType>;

public:
    memory_storage(DelayQueue* delay_queue) : super(delay_queue) {
    }
    ~memory_storage() {
        super::release_data();
    }
    bool persist_data(std::string table_name, std::string key, value_type data, const storage_config& config) {
        std::scoped_lock<std::mutex> locker(super::data_mutex);
        auto& table                                    = super::storage[table_name];
        auto data_iter                                 = table.find(key);
        storage::memory_storage_item<value_type>& item = data_iter == table.end() ? table[key] : data_iter->second;
        if (data_iter != table.end()) {
            if (!config.overwrite) {
                FWARN("table:{} key:{} has alread existed", table_name, key);
                return false;
            }
            // clear status
            super::delay_queue->StopDelayTask(item.handle);
            item.handle = Fundamental::DelayQueue::kInvalidHandle;
        }
        item.data = std::move(data);
        if (config.expired_time_msec > 0) {
            item.handle = super::delay_queue->AddDelayTask(
                config.expired_time_msec,
                [table_name, key, this]() {
                    super::remove_data(table_name, key);
                    super::expired_signal_.Emit(table_name, key);
                },
                true);
            super::delay_queue->StartDelayTask(item.handle);
        }
        return true;
    }

    std::tuple<bool, value_type> get_value(std::string table_name, std::string key) const {
        bool b_sucess = false;
        value_type ret;

        do {
            std::scoped_lock<std::mutex> locker(super::data_mutex);
            auto table_iter = super::storage.find(table_name);
            if (table_iter == super::storage.end()) break;
            auto data_iter = table_iter->second.find(key);
            if (data_iter == table_iter->second.end()) break;
            ret      = data_iter->second.data;
            b_sucess = true;
        } while (0);
        return { b_sucess, std::move(ret) };
    }
};

template <>
class memory_storage<void> : public storage::memory_storage_data<void>,
                             public data_storage_accessor<void, memory_storage<void>> {
public:
    using value_type         = void;
    using super              = storage::memory_storage_data<void>;

public:
    memory_storage(DelayQueue* delay_queue) : super(delay_queue) {
    }
    ~memory_storage() {
        super::release_data();
    }
    bool persist_data(std::string table_name, std::string key, const storage_config& config) {
        std::scoped_lock<std::mutex> locker(super::data_mutex);
        auto& table                                    = super::storage[table_name];
        auto data_iter                                 = table.find(key);
        storage::memory_storage_item<value_type>& item = data_iter == table.end() ? table[key] : data_iter->second;
        if (data_iter != table.end()) {
            if (!config.overwrite) {
                FWARN("table:{} key:{} has alread existed", table_name, key);
                return false;
            }
            // clear status
            super::delay_queue->StopDelayTask(item.handle);
            item.handle = Fundamental::DelayQueue::kInvalidHandle;
        }
        if (config.expired_time_msec > 0) {
            item.handle = super::delay_queue->AddDelayTask(
                config.expired_time_msec,
                [table_name, key, this]() {
                    super::remove_data(table_name, key);
                    expired_signal_.Emit(table_name, key);
                },
                true);
            super::delay_queue->StartDelayTask(item.handle);
        }
        return true;
    }
};
} // namespace Fundamental