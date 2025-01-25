#pragma once

#include <getopt.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Fundamental {
class commandline_utils {
public:
    enum param_type : std::int32_t {
        with_none_param,
        required_param,
        optional_param
    };
    struct paser_option {
        std::string name;
        std::int32_t option_value;
        param_type type;
        std::string opt_usage;
        std::string param_description;
        std::optional<std::vector<std::string>> paser_values;
    };
    static constexpr std::int8_t kReservedShortOption[] = { '?' /*for invalid option*/, ':' /*missing param*/ };
    static constexpr std::int32_t kNoShortOption        = -1;
    static constexpr std::int32_t kHealperShortOption   = 'h';
    static constexpr std::int32_t kVersionShortOption   = 'v';
    static constexpr const char* kHelperOptionName      = "help";
    static constexpr const char* kVersionOptionName     = "version";

public:
    commandline_utils(int argc, char* argv[], const std::string& version = "1.0.0");
    ~commandline_utils();
    bool AddOption(const std::string& name, const std::string& opt_usage,
                   std::int32_t short_option_value = kNoShortOption, param_type type = with_none_param,
                   const std::string& param_description = "param");
    // return false only means some fatal error occurs,you can see the error message in stderr
    bool ParseCommandLine();
    void ShowHelp();
    void ShowVersion();
    std::optional<std::vector<std::string>> GetOptionValues(const std::string& name) const;
    std::optional<std::string> GetOptionValue(const std::string& name) const;
    const std::vector<std::string>& GetNonOptionValues() const;
    bool HasParam(const std::string& name = kHelperOptionName) const;

private:
    const int argc;
    char** const argv;
    const std::string version;
    std::int32_t current_option_value = 128;
    std::string_view program_path;
    std::vector<std::string> no_option_params;
    std::unordered_map<std::string, paser_option> options;
    std::map<std::int32_t, std::string> short_options_dict;
};

commandline_utils::commandline_utils(int argc, char* argv[], const std::string& version) :
argc(argc), argv(argv), version(version), program_path(argv[0]) {
    AddOption(kHelperOptionName, "show this help page", kHealperShortOption, with_none_param);
    AddOption(kVersionOptionName, "show version information", kVersionShortOption, with_none_param);
    auto size = sizeof(kReservedShortOption);
    for (size_t i = 0; i < size; i++) {
        short_options_dict.emplace(kReservedShortOption[i], "__reserved__");
    }
}

commandline_utils::~commandline_utils() {
}

inline bool commandline_utils::AddOption(const std::string& name, const std::string& opt_usage,
                                         std::int32_t option_value, param_type type,
                                         const std::string& param_description) {
    auto iter = options.find(name);
    if (iter != options.end()) {
        std::cerr << "Option " << name << " already exists." << std::endl;
        return false;
    }
    if (option_value != kNoShortOption && !std::isprint(option_value)) {
        std::cerr << "Invalid short option value " << option_value << std::endl;
        return false;
    }
    auto iter_short = short_options_dict.find(option_value);
    if (iter_short != short_options_dict.end()) {
        std::cerr << "short option value:" << (char)option_value << " already exists for option:" << iter_short->second
                  << std::endl;
        return false;
    }
    if (option_value == kNoShortOption) {
        option_value = current_option_value++;
    }
    short_options_dict.emplace(option_value, name);
    options.emplace(name, paser_option { name, option_value, type, opt_usage, param_description, std::nullopt });
    return true;
}

inline bool commandline_utils::ParseCommandLine() {

    std::vector<option> long_options;
    std::string short_options;
    for (const auto& [name, option] : options) {
        auto& new_long_option = long_options.emplace_back();
        new_long_option.name  = option.name.c_str();
        std::string new_short_option;
        if (std::isprint(option.option_value)) {
            new_short_option.push_back(static_cast<char>(option.option_value));
        }
        if (option.type == with_none_param) {
            new_long_option.has_arg = no_argument;
        } else if (option.type == required_param) {
            new_long_option.has_arg = required_argument;
            if (!new_short_option.empty()) new_short_option += ":";
        } else {
            new_long_option.has_arg = optional_argument;
            if (!new_short_option.empty()) new_short_option += "::";
        }
        new_long_option.flag = nullptr;
        new_long_option.val  = option.option_value;
        short_options += new_short_option;
    }
    long_options.push_back({ nullptr, 0, nullptr, 0 });
    short_options.push_back('\0');
    while (true) {
        int option_index = 0;
        int res          = getopt_long(argc, argv, short_options.data(), long_options.data(), &option_index);

        if (res == -1) {
            break;
        }

        switch (res) {
        case 0: {
            std::cerr << "internal error for flag long opt" << std::endl;
            return false;
        }
        case '?': std::cerr << "invalid option: -" << static_cast<char>(optopt) << std::endl; break;
        case ':': std::cerr << "option -" << optopt << " requires an argument" << std::endl; return false;
        default: {
            auto iter = short_options_dict.find(res);
            if (iter == short_options_dict.end()) {
                std::cerr << "internal error can't find:" << res << std::endl;
                return false;
            }
            auto option_iter = options.find(iter->second);
            if (option_iter == options.end()) {
                std::cerr << "internal error can't find option:" << iter->second << std::endl;
                return false;
            }

            auto& current_option = option_iter->second;
            std::cout << current_option.name << " " << (optarg ? optarg : "none arg") << " " << current_option.type
                      << std::endl;
            if (!current_option.paser_values.has_value()) {
                current_option.paser_values = std::vector<std::string>();
            }
            switch (current_option.type) {
            case optional_param: {
                if (optarg) current_option.paser_values.value().push_back(optarg);
            } break;
            case required_param: {
                if (!optarg) {
                    std::cerr << "paser " << iter->second << " internal error" << std::endl;
                    return false;
                }

                current_option.paser_values.value().push_back(optarg);
            } break;
            default: break;
            }
        }
        }
    }
    while (optind < argc) {
        no_option_params.push_back(argv[optind]);
        ++optind;
    }
    return true;
}

inline void commandline_utils::ShowHelp() {
    std::cout << "Usage for program:" << program_path << " version:" << version << std::endl;
    std::cout << "Notice:For options with optional arguments,the argument must either immediately follow the short "
                 "option or be connected with =."
              << std::endl;
    std::cout << "For long options, the argument can only be connected with =." << std::endl;
    std::cout << "For example: -a[arg] or -a[=arg] or --long-option[=arg]" << std::endl;
    for (const auto& [name, option] : options) {
        if (option.option_value < 128) {
            std::cout << "\t-" << (char)(option.option_value) << ",";
        } else {
            std::cout << "\t";
        }
        std::cout << "--" << name;
        if (option.type == required_param) {
            std::cout << " <" << option.param_description << ">";
        } else if (option.type == optional_param) {
            std::cout << " [" << option.param_description << "]";
        }
        std::cout << "\t" << option.opt_usage << std::endl;
    }
}

inline std::optional<std::vector<std::string>> commandline_utils::GetOptionValues(const std::string& name) const {
    auto iter = options.find(name);
    return iter == options.end() ? std::nullopt : iter->second.paser_values;
}

inline std::optional<std::string> commandline_utils::GetOptionValue(const std::string& name) const {
    auto iter = options.find(name);
    if (iter == options.end() || !iter->second.paser_values.has_value()) return std::nullopt;
    auto& value = iter->second.paser_values.value();
    return value.empty() ? "" : value[0];
}

inline const std::vector<std::string>& commandline_utils::GetNonOptionValues() const {
    return no_option_params;
}

inline bool commandline_utils::HasParam(const std::string& name) const {
    return GetOptionValue(name).has_value();
}

inline void commandline_utils::ShowVersion() {
    std::cout << "Version: " << version << std::endl;
}

} // namespace Fundamental