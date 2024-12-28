// base64_utils.hpp
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>

namespace Fundamental
{
#define BASE64TABLE_DEFAULT_CODE(V) V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V
static constexpr std::array<std::uint8_t, 256> kBase64DecodeTable = {
    BASE64TABLE_DEFAULT_CODE(0x80),
    BASE64TABLE_DEFAULT_CODE(0x80),
    0x80,
    0x80,
    0x80,
    62,
    0x80,
    0x80,
    0x80,
    63,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    0x80,
    0x80,
    0x80,
    0,
    0x80,
    0x80,
    0x80,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
};

static constexpr char kBase64Char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode inputBuff to a base64 string.
inline std::string Base64Encode(const void* inputBuff, std::size_t buffSize)
{
    std::string ret("");
    if (buffSize == 0)
        return ret;
    auto buf                             = reinterpret_cast<const uint8_t*>(inputBuff);
    const std::size_t numOrig24BitValues = buffSize / 3;
    bool havePadding                     = buffSize > numOrig24BitValues * 3;      // if has remainder
    bool havePadding2                    = buffSize == numOrig24BitValues * 3 + 2; // if remainder = 2
    const std::size_t numResultBytes     = 4 * (numOrig24BitValues + havePadding); // final string's size
    ret.resize(numResultBytes);
    std::size_t i;
    for (i = 0; i < numOrig24BitValues; ++i)
    {
        ret[4 * i + 0] = kBase64Char[(buf[3 * i] >> 2) & 0x3F];
        ret[4 * i + 1] = kBase64Char[(((buf[3 * i] & 0x3) << 4) | (buf[3 * i + 1] >> 4)) & 0x3F];
        ret[4 * i + 2] = kBase64Char[((buf[3 * i + 1] << 2) | (buf[3 * i + 2] >> 6)) & 0x3F];
        ret[4 * i + 3] = kBase64Char[buf[3 * i + 2] & 0x3F];
    }

    // remainder is 1 need append two '='
    // remainder is 2 need append one '='
    if (havePadding)
    {
        ret[4 * i + 0] = kBase64Char[(buf[3 * i] >> 2) & 0x3F];
        if (havePadding2)
        {
            ret[4 * i + 1] = kBase64Char[(((buf[3 * i] & 0x3) << 4) | (buf[3 * i + 1] >> 4)) & 0x3F];
            ret[4 * i + 2] = kBase64Char[(buf[3 * i + 1] << 2) & 0x3F];
        }
        else
        {
            ret[4 * i + 1] = kBase64Char[((buf[3 * i] & 0x3) << 4) & 0x3F];
            ret[4 * i + 2] = '=';
        }
        ret[4 * i + 3] = '=';
    }
    return ret;
}

// Decode a base64 string to binary buff
template <typename T, typename = typename std::enable_if_t<std::disjunction_v<
                          std::is_same<T, std::vector<std::uint8_t>>,
                          std::is_same<T, std::string>>>>
inline bool Base64Decode(const std::string& originString, T& outContainer)
{
    if (originString.empty())
        return false;


    int k            = 0;
    int paddingCount = 0;
    auto buffSize    = originString.length();
    std::size_t retSize;
    const std::size_t jMax = buffSize - 3;
    retSize                = 3 * buffSize / 4;
    // std::string buff;
    outContainer.resize(retSize);
    // std::shared_ptr<std::uint8_t> retBuf(new std::uint8_t[retSize], std::default_delete<std::uint8_t[]>());
    for (std::size_t j = 0; j < jMax; j += 4)
    {
        char inTmp[4], outTmp[4];
        for (int i = 0; i < 4; ++i)
        {
            inTmp[i] = originString[i + j];
            if (inTmp[i] == '=')
                ++paddingCount;
            outTmp[i] = kBase64DecodeTable[(unsigned char)inTmp[i]];
            if ((outTmp[i] & 0x80) != 0)
                outTmp[i] = 0; // this happens only if there was an invalid character; pretend that it was 'A'
        }
        outContainer[k++] = (outTmp[0] << 2) | (outTmp[1] >> 4);
        outContainer[k++] = (outTmp[1] << 4) | (outTmp[2] >> 2);
        outContainer[k++] = (outTmp[2] << 6) | outTmp[3];
    }
    outContainer.resize(retSize - paddingCount);
    return true;
}

} // namespace Fundamental