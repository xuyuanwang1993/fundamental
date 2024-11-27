#include "agent_connection.hpp"
#include "agent_storage.hpp"
#include "fundamental/basic/buffer.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
namespace network
{
namespace proxy
{

namespace details
{
static bool ProcessUpdate(AgentRequestFrame& request, AgentResponseFrame& response)
{
    using SizeType = decltype(request.payload)::SizeType;
    Fundamental::BufferReader<SizeType> reader;
    reader.SetBuffer(request.payload.GetData(), request.payload.GetSize());
    AgentUpdateRequest requestData;
    try
    {
        reader.ReadRawMemory(requestData.id);
        reader.ReadRawMemory(requestData.section);
        reader.ReadRawMemory(requestData.data);
        AgentStorage::Instance().UpdateAgentInfo(requestData.id, requestData.section, std::move(requestData.data));
        response.cmd = request.cmd;
    }
    catch (const std::exception&)
    {
        return false;
    }
    return true;
}

static bool ProcessQuery(AgentRequestFrame& request, AgentResponseFrame& response)
{
    using SizeType = decltype(request.payload)::SizeType;
    Fundamental::BufferReader<SizeType> reader;
    reader.SetBuffer(request.payload.GetData(), request.payload.GetSize());
    AgentQueryRequest requestData;
    AgentQueryResponse responseData;
    try
    {
        reader.ReadRawMemory(requestData.id);
        reader.ReadRawMemory(requestData.section);
        response.cmd = request.cmd;
        if (!AgentStorage::Instance().QueryAgentInfo(requestData.id, requestData.section, responseData))
        {
            response.code = AgentFailed;
        }
    }
    catch (const std::exception&)
    {
        return false;
    }
    SizeType payloadSize = sizeof(responseData.timestamp) +
                           requestData.section.GetSize() +
                           sizeof(requestData.section.GetSize());
    response.payload.Reallocate(payloadSize);
    Fundamental::BufferWriter<SizeType> writer;
    writer.SetBuffer(response.payload.GetData(),response.payload.GetSize());
    writer.WriteValue(&responseData.timestamp);
    writer.WriteRawMemory(responseData.data);
    return true;
}

static Fundamental::ScopeGuard s_register(nullptr, []() {
    AgentConnection::cmd_handlers.emplace(std::string(AgentUpdateRequest::kCmd), details::ProcessUpdate);
    AgentConnection::cmd_handlers.emplace(std::string(AgentQueryRequest::kCmd), details::ProcessQuery);
});
}; // namespace details

AgentConnection::AgentConnection(asio::ip::tcp::socket&& socket, ProxyFrame&& frame) :
ProxeServiceBase(std::move(socket), std::move(frame))
{
}

void AgentConnection::SetUp()
{
    ProcessCmd();
}

AgentConnection::~AgentConnection()
{
    FDEBUG("release AgentConnection");
}

void AgentConnection::ProcessCmd()
{
    do
    {
        AgentRequestFrame request;
        if (!PaserRequestFrame(request))
            break;
        auto iter = cmd_handlers.find(request.cmd);
        if (iter == cmd_handlers.end() || !iter->second)
        {
            FERR("unsupported cmd:{}", request.cmd.ToString());
            break;
        };
        AgentResponseFrame response;
        if (!iter->second(request, response))
            break;
        HandleResponse(response);
        return;
    } while (0);
    // the connection will be released
}
bool AgentConnection::PaserRequestFrame(AgentRequestFrame& request)
{
    using SizeType = decltype(frame.payload)::SizeType;
    Fundamental::BufferReader<SizeType> reader;
    reader.SetBuffer(frame.payload.GetData(), frame.payload.GetSize());
    try
    {
        reader.ReadRawMemory(request.cmd);
        reader.ReadValue(&request.version);
        reader.ReadRawMemory(request.payload);
        frame.payload.FreeBuffer();
    }
    catch (const std::exception&)
    {
        return false;
    }
    return true;
}

void AgentConnection::HandleResponse(AgentResponseFrame& response)
{
    ProxyFrame responseFrame;
    std::size_t payloadSize = 3 * sizeof(response.cmd.GetSize()) +
                              response.cmd.GetSize() +
                              response.msg.GetSize() +
                              response.payload.GetSize() + sizeof(response.code);
    responseFrame.op = frame.op;
    responseFrame.payload.Reallocate(payloadSize);
    using SizeType = decltype(ProxyFrame::payload)::SizeType;
    Fundamental::BufferWriter<SizeType> writer;
    writer.SetBuffer(responseFrame.payload.GetData(), responseFrame.payload.GetSize());
    writer.WriteRawMemory(response.cmd);
    writer.WriteValue(&response.code);
    writer.WriteRawMemory(response.msg);
    writer.WriteRawMemory(response.payload);
    ProxyRequestHandler::EncodeFrame(responseFrame);
    auto buffers = ProxyRequestHandler::FrameToBuffers(responseFrame);
    asio::async_write(socket_, std::move(buffers),
                      [this, self = shared_from_this(), refFrame = std::move(responseFrame)](std::error_code ec, std::size_t) {
                          if (!ec)
                          {
                              FWARN("disconnected for  write :{}", ec.message());
                          }
                      });
}

} // namespace proxy
} // namespace network
