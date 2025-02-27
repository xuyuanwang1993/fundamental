#pragma once
#include <cstdint>
#include <exception>
#include <memory>
#include <system_error>

#include "fundamental/basic/utils.hpp"

namespace network {
namespace rpc_service {
enum class result_code : std::int16_t {
    OK   = 0,
    FAIL = 1,
};

enum class request_type : uint8_t {
    rpc_req,
    rpc_subscribe,
    rpc_unsubscribe,
    rpc_publish,
    rpc_heartbeat,
    rpc_res,
    rpc_stream
};

enum class rpc_stream_data_status : std::uint8_t {
    rpc_stream_none,       // init status
    rpc_stream_data,       // size+data
    rpc_stream_write_done, // size=0
    rpc_stream_finish,
    rpc_stream_failed,
    rpc_stream_status_max,
};

struct message_type {
    std::uint64_t req_id;
    request_type req_type;
    std::string content;
};

struct rpc_stream_packet
{
    std::uint32_t size;
    std::uint8_t type;
    std::vector<std::uint8_t> data;
};

static const uint8_t RPC_MAGIC_NUM = 39;
#pragma pack(1)
struct rpc_header {
    uint8_t magic;
    request_type req_type;
    uint32_t body_len;
    uint64_t req_id;
    uint32_t func_id;
};
#pragma pack()

static constexpr std::size_t MAX_BUF_LEN     = 1024LLU * 1024 * 1024 * 4;
static constexpr std::size_t kRpcHeadLen        = sizeof(rpc_header);
static constexpr std::size_t INIT_BUF_SIZE   = 2 * 1024;
static constexpr std::size_t kSslPreReadSize = 3;

namespace error {
enum class rpc_errors : std::int32_t {
    rpc_success        = 0,
    rpc_failed         = 1,
    rpc_timeout        = 2,
    rpc_broken_pipe    = 3,
    rpc_pack_failed    = 4,
    rpc_unpack_failed  = 5,
    rpc_internal_error = 6,
    rpc_bad_request    = 7,
    rpc_memory_error   = 8
};

class rpc_category : public std::error_category, public Fundamental::Singleton<rpc_category> {
public:
    const char* name() const noexcept override {
        return "network.rpc";
    }
    std::string message(int value) const override {
        switch (static_cast<rpc_errors>(value)) {
        case rpc_errors::rpc_success: return "success";
        case rpc_errors::rpc_failed: return "failed";
        case rpc_errors::rpc_timeout: return "timeout";
        case rpc_errors::rpc_broken_pipe: return "broken pipe";
        case rpc_errors::rpc_pack_failed: return "pack failed";
        case rpc_errors::rpc_unpack_failed: return "unpack failed";
        case rpc_errors::rpc_bad_request: return "bad request";
        case rpc_errors::rpc_internal_error: return "internal error";
        case rpc_errors::rpc_memory_error: return "rpc memory error";
        default: return "network.rpc error";
        }
    }
};

inline std::error_code make_error_code(rpc_errors e) {
    return std::error_code(static_cast<int>(e), rpc_category::Instance());
}
} // namespace error
} // namespace rpc_service
} // namespace network