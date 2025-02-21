#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
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
template <typename T, typename = typename std::enable_if_t<
                          std::disjunction_v<std::is_same<T, std::vector<std::uint8_t>>, std::is_same<T, std::string>>>>
inline bool ReadFile(std::string_view path, T& output) {
    // Always read as binary.
    std::ifstream file(std::string(path), std::ios::binary);
    if (file) {
        // Get the lengthInBytes in bytes
        file.seekg(0, file.end);
        auto lengthInBytes = file.tellg();
        file.seekg(0, file.beg);

        output.resize(lengthInBytes);
        file.read(reinterpret_cast<char*>(output.data()), lengthInBytes);
        file.close();
        return true;
    } else {
        std::cerr << "can't read from " << path << std::endl;
    }

    return false;
}

inline bool WriteFile(std::string_view path, const void* data, std::size_t len, bool override = true) {
    if (path.empty()) return false;
    // App mode means append mode, trunc mode means overwrite
    std::ios_base::openmode mode = override ? std::ios::trunc : std::ios::app;
    mode |= (std::ios::binary | std::ios::out);

    std::ofstream file(std::string(path), mode);
    if (file) {
        file.write(reinterpret_cast<const char*>(data), len);
        file.flush();
        file.close();
        return true;
    } else {
#ifdef DEBUG
        std::cerr << "can't write to " << path << std::endl;
#endif
    }

    return false;
}

inline bool SwitchToProgramDir(const std::string& argv0) {
    try {
        std::filesystem::current_path(std::filesystem::path(argv0).parent_path());
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}
} // namespace Fundamental::fs