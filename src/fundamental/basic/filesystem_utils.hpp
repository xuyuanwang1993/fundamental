#pragma once
#include <chrono>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace Fundamental::fs {

inline void RemoveExpiredFiles(std::string_view dir_path, std::string_view pattern, std::int64_t expiredSec,
                        bool recursive = false) {
    std::regex filePattern(pattern.data(), pattern.length());
    auto now = std::chrono::system_clock::now();
    std::filesystem::path directory(dir_path);
    std::vector<std::string> subdirPaths;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                const auto& path     = entry.path();
                const auto& filename = path.filename().string();
                if (std::regex_match(filename, filePattern)) {
                    auto ftime    = std::filesystem::last_write_time(path);
                    auto fileTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                    auto fileAge = std::chrono::duration_cast<std::chrono::seconds>(now - fileTime).count();
                    if (fileAge > expiredSec) {
                        std::filesystem::remove(path);
                    }
                }
            }
            if (recursive && entry.is_directory()) {
                subdirPaths.push_back(entry.path().string());
            }
        }
    } catch (const std::exception& e) {
#ifdef DEBUG
        std::cerr << "RemoveExpiredFiles " << dir_path << " catch error:" << e.what() << std::endl;
#endif
    }

    for (auto& item : subdirPaths)
        RemoveExpiredFiles(item, pattern, expiredSec, true);
}

} // namespace Fundamental::fs