#include "memory_data_storage.hpp"
#include <mutex>
#include <unordered_map>

#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
namespace Fundamental
{
namespace storage
{
struct memory_storage_item {
    std::string data;
    Fundamental::DelayQueue::HandleType handle = Fundamental::DelayQueue::kInvalidHandle;
};
}; // namespace storage

using storage_table_type  = std::unordered_map<std::string, storage::memory_storage_item>;
using storage_tables_type = std::unordered_map<std::string, storage_table_type>;
struct memory_storage::PrivateData {
    PrivateData(DelayQueue* delay_queue) : delay_queue(delay_queue) {
        FASSERT_ACTION(delay_queue != nullptr, throw std::runtime_error("need a valid delay queue ref"),
                       "need a valid delay queu ref");
    }
    ~PrivateData() {
        {
            std::scoped_lock<std::mutex> locker(data_mutex);
            release_all();
        }
    }
    void release_all() {
        for (auto& table : storage) {
            for (auto& item : table.second) {
                delay_queue->StopDelayTask(item.second.handle);
            }
        }
    }
    ExpiredSignalType expired_signal;
    std::mutex data_mutex;
    DelayQueue* const delay_queue;
    storage_tables_type storage;
};

memory_storage::memory_storage(DelayQueue* delay_queue) : p_data(new PrivateData(delay_queue)) {
}
memory_storage::~memory_storage() {
    delete p_data;
}
bool memory_storage::init() {
    // just do nothing
    return true;
}
bool memory_storage::persist_data(std::string table_name,
                                  std::string key,
                                  std::string data,
                                  const storage_config& config) {
    std::scoped_lock<std::mutex> locker(p_data->data_mutex);
    auto& table                        = p_data->storage[table_name];
    auto data_iter                     = table.find(key);
    storage::memory_storage_item& item = data_iter == table.end() ? table[key] : data_iter->second;
    if (data_iter != table.end()) {
        if (!config.overwrite) {
            FWARN("table:{} key:{} has alread existed", table_name, key);
            return false;
        }
        // clear status
        p_data->delay_queue->StopDelayTask(item.handle);
        item.handle = Fundamental::DelayQueue::kInvalidHandle;
    }
    item.data = std::move(data);
    if (config.expired_time_msec > 0) {
        item.handle = p_data->delay_queue->AddDelayTask(
            config.expired_time_msec,
            [table_name, key, this]() {
                remove_data(table_name, key);
                p_data->expired_signal.Emit(table_name, key);
            },
            true);
        p_data->delay_queue->StartDelayTask(item.handle);
    }
    return true;
}
std::size_t memory_storage::table_size(std::string table_name) const {
    std::scoped_lock<std::mutex> locker(p_data->data_mutex);
    auto table_iter = p_data->storage.find(table_name);
    return table_iter == p_data->storage.end() ? 0 : table_iter->second.size();
}
bool memory_storage::has_key(std::string table_name, std::string key) const {
    std::scoped_lock<std::mutex> locker(p_data->data_mutex);
    auto table_iter = p_data->storage.find(table_name);
    return table_iter == p_data->storage.end() ? false : table_iter->second.count(key) > 0;
}
bool memory_storage::remove_data(std::string table_name, std::string key) {
    std::scoped_lock<std::mutex> locker(p_data->data_mutex);
    auto table_iter = p_data->storage.find(table_name);
    return table_iter == p_data->storage.end() ? false : table_iter->second.erase(key) > 0;
}
bool memory_storage::update_key_expired_time(std::string table_name,
                                             std::string key,
                                             std::int64_t update_expired_time_msec) {

    std::scoped_lock<std::mutex> locker(p_data->data_mutex);
    auto table_iter = p_data->storage.find(table_name);
    if (table_iter == p_data->storage.end()) return false;
    auto data_iter = table_iter->second.find(key);
    return data_iter == table_iter->second.end() ? false
                                                 : p_data->delay_queue->ModifyTaskNextExpiredTimepoint(
                                                       data_iter->second.handle, update_expired_time_msec);
}
std::tuple<bool, std::string> memory_storage::get_value(std::string table_name, std::string key) const {
    bool b_sucess = false;
    std::string ret;

    do {
        std::scoped_lock<std::mutex> locker(p_data->data_mutex);
        auto table_iter = p_data->storage.find(table_name);
        if (table_iter == p_data->storage.end()) break;
        auto data_iter = table_iter->second.find(key);
        if (data_iter == table_iter->second.end()) break;
        ret      = data_iter->second.data;
        b_sucess = true;
    } while (0);
    return { b_sucess, std::move(ret) };
}
ExpiredSignalType& memory_storage::expired_signal() {
    return p_data->expired_signal;
}
} // namespace Fundamental