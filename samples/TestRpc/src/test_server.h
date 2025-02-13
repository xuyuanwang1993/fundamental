#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

struct person {
    std::int32_t id;
    std::string name;
    std::int32_t age;
};
struct dummy1 {
    std::size_t id;
    std::string str;
};

void run_server();
void exit_server();