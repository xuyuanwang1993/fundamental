
#include "fundamental/basic/integer_codec.hpp"
#include "fundamental/basic/log.h"
#include <chrono>
#include <iostream>
#include <memory>

#include <gtest/gtest.h>
using namespace Fundamental;
template <typename T>
class IntegerCodecTest : public ::testing::Test {
public:
    constexpr static bool Signed = std::is_signed_v<T>;
    constexpr static T kMin      = std::numeric_limits<T>::min();
    constexpr static T kMax      = std::numeric_limits<T>::max();
};

using AllTestTypes = ::testing::Types<std::uint8_t,
                                      std::uint16_t,
                                      std::uint32_t,
                                      std::uint64_t,
                                      std::int8_t,
                                      std::int16_t,
                                      std::int32_t,
                                      std::int64_t>;
// declare
TYPED_TEST_SUITE(IntegerCodecTest, AllTestTypes);

TYPED_TEST(IntegerCodecTest, TestAllNumberType) {
    auto max_extra_bytes                  = sizeof(this->kMin);
    std::uint8_t src[max_extra_bytes + 1] = {};
    using ValueType                       = std::decay_t<decltype(this->kMin)>;
    if constexpr (this->Signed)
    { // test len
        ValueType encode_v = 0;
        for (std::size_t shift = 0; shift < sizeof(ValueType) * 8 - 1; ++shift)
        {
            encode_v += 1 << shift;
            auto len           = VarintEncode(encode_v, src);
            ValueType decode_v = 0;
            auto len2          = VarintDecode(decode_v, src);
            EXPECT_EQ(len, len2);
            EXPECT_EQ(decode_v, encode_v);
        }
        encode_v = 0;
        for (std::size_t shift = 0; shift < sizeof(ValueType) * 8 - 1; ++shift)
        {
            encode_v += -1 << shift;
            auto len           = VarintEncode(encode_v, src);
            ValueType decode_v = 0;

            auto len2 = VarintDecode(decode_v, src);
            EXPECT_EQ(len, len2);
            EXPECT_EQ(decode_v, encode_v);
        }
    }
    else
    {
        ValueType encode_v = 0;
        for (std::size_t shift = 0; shift < sizeof(ValueType) * 8; ++shift)
        {
            encode_v += 1 << shift;
            auto len = VarintEncode(encode_v, src);

            ValueType decode_v = 0;
            auto len2          = VarintDecode(decode_v, src);
            EXPECT_EQ(len, len2);
            EXPECT_EQ(decode_v, encode_v);
        }
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}