#pragma once
#include <cassert>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "proxy_encoder.h"
namespace network {
namespace proxy {
struct ProxyRequest {
    constexpr static std::size_t kMaxPayloadLen      = 32*1024;//32k
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
        auto payload_len = PeekSize(ptr+offset);
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
        auto serviceLen = PeekSize(ptr+offset);
        offset += 4;
        auto fieldLen = PeekSize(ptr+offset);
        offset += 4;
        auto tokenLen = PeekSize(ptr+offset);
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
} // namespace proxy
} // namespace network
