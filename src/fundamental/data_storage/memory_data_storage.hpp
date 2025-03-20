#pragma once
#include "data_storage_interface.hpp"

namespace Fundamental
{
class DelayQueue;
class memory_storage : public data_storage_interface<memory_storage>{
    struct PrivateData;
public:
    memory_storage(DelayQueue * delay_queue);
    ~memory_storage();
    bool init();

    // return true if persist data success
    bool persist_data(std::string table_name,
                      std::string key,
                      std::string data,
                      const storage_config& config);

    std::size_t table_size(std::string table_name) const;

    bool has_key(std::string table_name, std::string key) const;

    bool remove_data(std::string table_name, std::string key);
    bool update_key_expired_time(std::string table_name,
                                 std::string key,
                                 std::int64_t update_expired_time_msec);

    std::tuple<bool, std::string> get_value(std::string table_name, std::string key) const;

    ExpiredSignalType& expired_signal();

private:
    PrivateData* p_data;
};
} // namespace Fundamental