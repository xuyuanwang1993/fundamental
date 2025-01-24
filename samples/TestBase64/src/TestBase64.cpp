#include "fundamental/basic/base64_utils.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/random_generator.hpp"
#include "fundamental/basic/utils.hpp"

int main(int argc, char* argv[]) {
    using ValueType = std::uint8_t;
    ValueType kMin  = 1;
    ValueType kMax  = 127;
    auto generator  = Fundamental::DefaultNumberGenerator(kMin, kMax);
    using InputType = std::vector<std::uint8_t>;
    InputType input;

    input.resize(100);
    generator.gen(reinterpret_cast<std::uint8_t*>(&input[0]), input.size());
    FDEBUG("input:{}", Fundamental::Utils::BufferToHex(input.data(), input.size()));
    {
        constexpr auto coderType = Fundamental::Base64CoderType::kFSBase64;
        std::string output       = Fundamental::Base64Encode<coderType>(input.data(), input.size());
        FDEBUG("output:{}", output);
        InputType decodeOutput;
        Fundamental::Base64Decode<coderType>(output, decodeOutput);
        FASSERT(input.size() == decodeOutput.size() &&
                std::memcmp(input.data(), decodeOutput.data(), input.size()) == 0);
    }
    {
        constexpr auto coderType = Fundamental::Base64CoderType::kNormalBase64;
        std::string output       = Fundamental::Base64Encode<coderType>(input.data(), input.size());
        FDEBUG("output:{}", output);
        InputType decodeOutput;
        Fundamental::Base64Decode<coderType>(output, decodeOutput);
        FASSERT(input.size() == decodeOutput.size() &&
                std::memcmp(input.data(), decodeOutput.data(), input.size()) == 0);
    }
    return 0;
}
