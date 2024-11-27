#pragma once
#include "fundamental/basic/buffer.hpp"
#include "fundamental/basic/utils.hpp"
#include <string>
#include <unordered_map>
namespace network
{
namespace proxy
{
enum AgentCode : std::int32_t
{
    AgentSuccess = 0,
    AgentFailed  = 1,
};

using AgentSizeType = std::uint32_t;
using AgentDataType = Fundamental::Buffer<AgentSizeType>;
struct AgentEntryInfo
{
    std::int64_t timestamp = 0;
    AgentDataType data;
};
using AgentItemMap = std::unordered_map<AgentDataType /*section*/, AgentEntryInfo, Fundamental::BufferHash<AgentSizeType>>;
using AgentMap     = std::unordered_map<AgentDataType /*id*/, AgentItemMap, Fundamental::BufferHash<AgentSizeType>>;

// network
struct AgentRequestFrame
{
    AgentDataType cmd;
    std::int32_t version = 0;
    AgentDataType payload;
};

struct AgentResponseFrame
{
    AgentDataType cmd;
    std::int32_t code = AgentSuccess;
    AgentDataType msg;
    AgentDataType payload;
};

// api payload
struct AgentDummyRequest
{
};

struct AgentDummyResponse
{
};

struct AgentUpdateRequest
{
    static constexpr const char * kCmd="update";
    AgentDataType id;
    AgentDataType section;
    AgentDataType data;
};

using AgentUpdateResponse = AgentDummyResponse;

struct AgentQueryRequest
{
    static constexpr const char * kCmd="query";
    AgentDataType id;
    AgentDataType section;
};

struct AgentQueryResponse : AgentEntryInfo
{
};
} // namespace proxy
} // namespace network