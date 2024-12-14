#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif
    unsigned int generate_32bit_random_number()
    {
        unsigned int seed = (unsigned int)time(NULL);
        srand(seed);
        unsigned int random_number = ((unsigned int)rand() << 15) | (unsigned int)rand();

        return random_number;
    }

    struct proxy_encode_input
    {
        unsigned char opCode;
        const char* field;
        size_t fieldLen;
        const char* service;
        size_t serviceLen;
        const char* token;
        size_t tokenLen;
    };

    struct proxy_encode_output
    {
        /// @brief you should call free to release this buf
        unsigned char* buf;
        size_t bufLen;
    };

    inline void proxy_free_output(proxy_encode_output* output)
    {
        if (output && output->buf)
        {
            free(output->buf);
            output->buf    = NULL;
            output->bufLen = 0;
        }
    }

    inline uint64_t proxy__encode_64__(void* buf, uint64_t value)
    {
        uint64_t& t = *((uint64_t*)buf);
        return t    = htole64(value);
    }

    inline uint16_t proxy__encode_16__(void* buf, uint16_t value)
    {
        uint16_t& t = *((uint16_t*)buf);
        return t    = htole16(value);
    }

    inline void proxy_encode_request(proxy_encode_input& input, proxy_encode_output& output)
    {
        uint64_t payload_len = sizeof(uint64_t) * 3 +
                               input.fieldLen + input.serviceLen + input.tokenLen;
        output.bufLen = sizeof(uint16_t) + 1 + 1 + 1 +
                            sizeof(uint32_t) + sizeof(uint64_t) + payload_len;
        output.buf         = (unsigned char*)malloc(output.bufLen);
        unsigned char* ptr = output.buf;
        // encode
        size_t offset = 0;
        proxy__encode_16__(ptr + offset, 0x6668);
        offset += 2; // fixed
        unsigned char& checkSum = ptr[offset];
        ++offset;
        ptr[offset] = 0x01; // version
        ++offset;
        ptr[offset] = input.opCode; // opcode
        ++offset;
        uint32_t& mask = *((uint32_t*)(ptr + offset));
        mask           = generate_32bit_random_number(); // mask
        offset += 4;
        proxy__encode_64__(ptr + offset, payload_len); // payload size
        offset += 8;
        unsigned char* payload_ptr = ptr + offset;
        proxy__encode_64__(ptr + offset, input.serviceLen); // service size
        offset += 8;
        memcpy(ptr + offset, input.service, input.serviceLen); // service
        offset += input.serviceLen;
        proxy__encode_64__(ptr + offset, input.fieldLen); // field size
        offset += 8;
        memcpy(ptr + offset, input.field, input.fieldLen); // field
        offset += input.fieldLen;
        proxy__encode_64__(ptr + offset, input.tokenLen); // tokenlen
        offset += 8;
        memcpy(ptr + offset, input.token, input.tokenLen); // token

        // mask operation
        size_t leftSize           = payload_len % 4;
        size_t alignBufferSize    = payload_len - leftSize;
        uint32_t opeationCheckSum = 0;
        // mask the origin data
        for (size_t i = 0; i < alignBufferSize; i += 4)
        {
            uint32_t& operationNum = *((uint32_t*)(payload_ptr + i));
            opeationCheckSum ^= operationNum;
            operationNum ^= mask;
        }
        // update mask
        unsigned char* p_mask_result = (unsigned char*)&opeationCheckSum;
        checkSum ^= p_mask_result[0];
        checkSum ^= p_mask_result[1];
        checkSum ^= p_mask_result[2];
        checkSum ^= p_mask_result[3];
        unsigned char* p_mask = (unsigned char*)&mask;
        // fix left bytes
        for (size_t i = 0; i < leftSize; ++i)
        {
            checkSum ^= payload_ptr[i + alignBufferSize];
            payload_ptr[i + alignBufferSize] ^= p_mask[i % 4];
        }
    }

#ifdef __cplusplus
}
#endif