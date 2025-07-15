#pragma once
#include <cassert>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "proxy_encoder.h"
namespace network
{
namespace proxy
{
struct ProxyRequest {
    constexpr static std::size_t kMaxPayloadLen      = 32 * 1024; // 32k
    constexpr static std::uint8_t kMagicNum          = PROXY_MAGIC_NUM;
    constexpr static const char* kVerifyStr          = PROXY_RESPONSE_DATA;
    constexpr static std::size_t kVerifyStrLen       = PROXY_RESPONSE_DATA_LEN;
    static constexpr std::size_t kMinSize            = 1 + sizeof(std::uint32_t) * 6;
    static constexpr std::size_t kHeaderLen          = 1 + sizeof(std::uint32_t);
    inline static const std::uint32_t kMaskInitValue = PROXY_MASK_INIT_VALUE;
    std::string service_name_;
    std::string token_;
    std::string field_;
    ProxyRequest() = default;
    ProxyRequest(std::string_view service_name, std::string token, std::string field) :
    service_name_(service_name), token_(token), field_(field) {
    }

    bool FromBuf(void* buf, std::size_t len) {
        // magic(1)  payload_len(4) check_sum(4)mask(4)  serviceLen(4) fieldLen(4) tokenLen(4) service  field   token
        if (len < kMinSize) return false;
        auto ptr           = (std::uint8_t*)buf;
        std::size_t offset = 0;
        if (ptr[offset] != kMagicNum) return false;
        offset += 1;
        auto payload_len = PeekSize(ptr + offset);
        offset += 4;
        auto p_check_sum = ptr + offset;
        offset += 4;
        auto p_mask = ptr + offset;
        offset += 4;

        if (len < (kHeaderLen + payload_len)) return false;
        auto data_len = payload_len - 8;

        auto p_data = ptr + offset;
        for (size_t i = 0; i < data_len; ++i) {
            // unmask   data
            p_data[i] ^= p_mask[i % 4];
            // update check sum
            p_check_sum[i % 4] ^= p_data[i];
        }
        if (std::memcmp(&kMaskInitValue, p_check_sum, 4) != 0) return false;
        auto serviceLen = PeekSize(ptr + offset);
        offset += 4;
        auto fieldLen = PeekSize(ptr + offset);
        offset += 4;
        auto tokenLen = PeekSize(ptr + offset);
        offset += 4;
        if ((serviceLen + fieldLen + tokenLen + sizeof(std::uint32_t) * 3) != data_len) return false;
        auto current_ptr = ptr + offset;
        service_name_.clear();
        field_.clear();
        token_.clear();
        if (serviceLen > 0) { // service name
            service_name_ = std::string(current_ptr, current_ptr + serviceLen);
            current_ptr += serviceLen;
        }
        if (fieldLen > 0) { // service name
            field_ = std::string(current_ptr, current_ptr + fieldLen);
            current_ptr += fieldLen;
        }
        if (tokenLen > 0) { // service name
            token_ = std::string(current_ptr, current_ptr + tokenLen);
            current_ptr += tokenLen;
        }
        return true;
    }
    std::vector<std::uint8_t> Encode() const {
        std::vector<std::uint8_t> ret;
        proxy_encode_input input;
        input.field      = field_.data();
        input.fieldLen   = field_.size();
        input.service    = service_name_.data();
        input.serviceLen = service_name_.size();
        input.token      = token_.data();
        input.tokenLen   = token_.size();
        proxy_encode_output output;
        proxy_encode_request(input, output);
        ret.resize(output.bufLen);
        std::memcpy(ret.data(), output.buf, ret.size());
        proxy_free_output(&output);
        return ret;
    }
    static std::uint32_t PeekSize(const void* ptr) {
        std::uint32_t ret = *((const std::uint32_t*)ptr);
        return le32toh(ret);
    }
};

struct ProxyRawTcpRequest {
    constexpr static std::uint8_t kMagicNum = PROXY_RAW_TCP_MAGIC_NUM;
    static constexpr std::size_t kMaxSize   = 61;
    static constexpr std::size_t kMinSize   = 7;
    static constexpr std::size_t kSizeLen   = 2;
    static constexpr char kSeperateChar     = ':';
    static constexpr char kEndChar          = '$';
    std::string host_;
    std::string service_;
    ProxyRawTcpRequest() = default;
    ProxyRawTcpRequest(std::string host, std::string service) : host_(host), service_(service) {
    }

    bool FromBuf(void* buf, std::size_t len) {
        // magic(1)  host:service\n
        if (len < kMinSize || len > kMaxSize + 1 + kSizeLen) return false;

        auto ptr = (std::uint8_t*)buf;
        if (ptr[0] != kMagicNum) return false;
        std::string len_data = std::string(ptr + 1, ptr + 1 + kSizeLen);

        try {
            auto following_len = std::stoul(len_data);
            if (following_len + 1 + kSizeLen > len) return false;
        } catch (const std::exception& e) {
            return false;
        }

        std::string temp_data = std::string(ptr + 1 + kSizeLen, ptr + len);
        auto pos              = temp_data.find_first_of(kEndChar);
        if (pos == std::string::npos) return false;
        temp_data = temp_data.substr(0, pos);
        pos       = temp_data.find_last_of(kSeperateChar);
        if (pos == std::string::npos) return false;
        host_    = temp_data.substr(0, pos);
        service_ = temp_data.substr(pos + 1);
        return !host_.empty() && !service_.empty();
    }
    std::vector<std::uint8_t> Encode() const {
        std::vector<std::uint8_t> ret;
        std::size_t data_size = host_.size() + 1 + service_.size() + 1;
        ret.resize(data_size + 1 + kSizeLen);
        std::size_t offset = 0;
        auto p_data        = ret.data();
        p_data[offset]     = kMagicNum;
        ++offset;
        auto len_data = std::to_string(data_size);
        //filled with zero
        if(len_data.size()<kSizeLen)
        {
            len_data.insert(0,kSizeLen-len_data.size(),'0');
        }
        std::memcpy(p_data + offset, len_data.data(), kSizeLen);
        offset += kSizeLen;
        std::memcpy(p_data + offset, host_.data(), host_.size());
        offset += host_.size();
        p_data[offset] = kSeperateChar;
        ++offset;
        std::memcpy(p_data + offset, service_.data(), service_.size());
        offset += service_.size();
        p_data[offset] += kEndChar;
        return ret;
    }

    static std::size_t PeekSize(const void* ptr) {
        auto ret = (const std::uint8_t*)ptr;
        try {
            return std::stoul(std::string(ret, ret + kSizeLen));
        } catch (const std::exception& e) {
            return 0;
        }
    }
};
} // namespace proxy
} // namespace network
