#include "fundamental/basic/commandline_utils.hpp"
#include "fundamental/basic/log.h"
int main(int argc, char* argv[]) {
    Fundamental::commandline_utils commandline_utils { argc, argv, "8.8.8" };
    commandline_utils.AddOption("test", "test short option no param", 't');
    commandline_utils.AddOption("test2", "test short option with required param", 'd',
                                Fundamental::commandline_utils::param_type::required_param, "testparam");
    commandline_utils.AddOption("test3", "test short option with optional param", 'a',
                                Fundamental::commandline_utils::param_type::optional_param, "testoparam");
    FASSERT(!commandline_utils.AddOption("test3", "test short option with optional param", 'i',
                                         Fundamental::commandline_utils::param_type::optional_param, "testoparam"))
    FASSERT(!commandline_utils.AddOption("test4", "test short option with optional param", 'a',
                                         Fundamental::commandline_utils::param_type::optional_param, "testoparam"))
    FASSERT(!commandline_utils.AddOption("test4", "test short option with optional param", '?',
                                         Fundamental::commandline_utils::param_type::optional_param, "testoparam"))
    FASSERT(!commandline_utils.AddOption("test4", "test short option with optional param", ':',
                                         Fundamental::commandline_utils::param_type::optional_param, "testoparam"))
    FASSERT(!commandline_utils.AddOption("test4", "test short option with optional param", 'h',
                                         Fundamental::commandline_utils::param_type::optional_param, "testoparam"))
    FASSERT(!commandline_utils.AddOption("test4", "test short option with optional param", 'v',
                                         Fundamental::commandline_utils::param_type::optional_param, "testoparam"))
    if (argc == 1) {
        commandline_utils.ShowHelp();
        return 1;
    }
    if (commandline_utils.ParseCommandLine()) {
        FINFO("paser success");
    } else {
        FERR("paser failed");
    }
    if (commandline_utils.HasParam()) {
        commandline_utils.ShowHelp();
        return 1;
    }
    if (commandline_utils.HasParam(Fundamental::commandline_utils::kVersionOptionName)) {
        commandline_utils.ShowVersion();
    }
    // check values
    {
        std::string option = "test";
        auto v             = commandline_utils.GetOptionValue(option);
        if (v.has_value()) {
            FINFO("set {}", option);
        } else {
            FINFO("not set {}", option);
        }
    }
    {
        std::string option = "test2";
        auto v             = commandline_utils.GetOptionValue(option);
        if (v.has_value()) {
            FINFO("set {} value:{}", option, v.value());
        } else {
            FINFO("not set {}", option);
        }
    }
    {
        std::string option = "test3";
        auto v             = commandline_utils.GetOptionValues(option);
        if (v.has_value()) {
            FINFO("set {}", option);
            auto value = v.value();
            for (std::size_t i = 0; i < value.size(); ++i)
                FDEBUG("{}[{}]={}", option, i, value[i]);
        } else {
            FINFO("not set {}", option);
        }
    }
    auto none_option_params = commandline_utils.GetNonOptionValues();
    for (auto& item : none_option_params)
        FDEBUG("non option: {}", item);
    return 0;
}
