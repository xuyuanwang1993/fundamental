#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif
inline unsigned int generate_32bit_random_number() {
    unsigned int seed = (unsigned int)time(NULL);
    srand(seed);
    unsigned int random_number = ((unsigned int)rand() << 15) | (unsigned int)rand();

    return random_number;
}
//'('
#define PROXY_MAGIC_NUM         40
//')'
#define PROXY_RAW_TCP_MAGIC_NUM 41
#define PROXY_RESPONSE_DATA     "ok"
#define PROXY_RESPONSE_DATA_LEN 2
#define PROXY_MASK_INIT_VALUE   0
struct proxy_encode_input {
    const void* field;
    uint32_t fieldLen;
    const void* service;
    uint32_t serviceLen;
    const void* token;
    uint32_t tokenLen;
};
// magic(1) payload_len(4) check_sum(4)mask(4)   serviceLen(4) fieldLen(4) tokenLen(4) service  field   token
struct proxy_encode_output {
    /// @brief you should call free to release this buf
    unsigned char* buf;
    uint32_t bufLen;
};

inline void proxy_free_output(proxy_encode_output* output) {
    if (output && output->buf) {
        free(output->buf);
        output->buf    = NULL;
        output->bufLen = 0;
    }
}

inline uint16_t proxy__encode_32__(void* buf, uint32_t value) {
    uint32_t& t = *((uint32_t*)buf);
    return t    = htole32(value);
}

inline void proxy_encode_request(proxy_encode_input& input, proxy_encode_output& output) {
    uint32_t payload_len = sizeof(uint32_t) * 5 + input.fieldLen + input.serviceLen + input.tokenLen;
    output.bufLen        = 1 + sizeof(uint32_t) + payload_len;
    output.buf           = (unsigned char*)malloc(output.bufLen);
    unsigned char* ptr   = output.buf;
    // encode
    uint32_t offset = 0;
    ptr[offset]     = PROXY_MAGIC_NUM;
    ++offset;
    proxy__encode_32__(ptr + offset, payload_len); // payload size
    offset += 4;
    unsigned char* check_sum_ptr = ptr + offset;
    uint32_t init_value          = PROXY_MASK_INIT_VALUE;
    memcpy(ptr + offset, &init_value, 4);
    offset += 4;
    uint32_t mask = generate_32bit_random_number(); // mask
    memcpy(ptr + offset, &mask, 4);
    unsigned char* mask_ptr = ptr + offset;
    offset += 4;
    unsigned char* data_ptr = ptr + offset;
    
    proxy__encode_32__(ptr + offset, input.serviceLen); // service size
    offset += 4;
    proxy__encode_32__(ptr + offset, input.fieldLen); // field size
    offset += 4;
    proxy__encode_32__(ptr + offset, input.tokenLen); // tokenlen
    offset += 4;
    memcpy(ptr + offset, input.service, input.serviceLen); // service
    offset += input.serviceLen;
    memcpy(ptr + offset, input.field, input.fieldLen); // field
    offset += input.fieldLen;
    memcpy(ptr + offset, input.token, input.tokenLen); // token
    payload_len -= 8;
    for (size_t i = 0; i < payload_len; ++i) {
        // update check sum
        check_sum_ptr[i % 4] ^= data_ptr[i];
        // mask origin data
        data_ptr[i] ^= mask_ptr[i % 4];
    }
}

#ifdef __cplusplus
}
#endif