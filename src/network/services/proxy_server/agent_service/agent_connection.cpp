#include "agent_connection.hpp"
#include "agent_storage.hpp"
#include "fundamental/basic/buffer.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
namespace network
{
namespace proxy
{

struct AgentConnection::details
{
    static void ProcessUpdate(AgentRequestFrame& request, std::shared_ptr<AgentConnection> connection)
    {
        AgentResponseFrame response;
        using SizeType = decltype(request.payload)::SizeType;
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(request.payload.GetData(), request.payload.GetSize());
        AgentUpdateRequest requestData;
        response.cmd = request.cmd;
        try
        {
            reader.ReadRawMemory(requestData.id);
            reader.ReadRawMemory(requestData.section);
            reader.ReadRawMemory(requestData.data);
            reader.ReadValue(&requestData.op);
            AgentStorage::Instance().UpdateAgentInfo(requestData.id, requestData.section, std::move(requestData.data),requestData.op);
        }
        catch (const std::exception&)
        { // close connection
            return;
        }
        connection->HandleResponse(response);
    }
    static void ProcessSniff(AgentRequestFrame& request, std::shared_ptr<AgentConnection> connection)
    {
        AgentResponseFrame response;
        using SizeType = decltype(request.payload)::SizeType;
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(request.payload.GetData(), request.payload.GetSize());
        AgentSniffRequest requestData;
        response.cmd = request.cmd;

        auto ipInfo = connection->socket_.remote_endpoint().address().to_string();
        if (ipInfo == AgentSniffRequest::kLoopbackIp)
        {
            auto networkInfo = Fundamental::Utils::GetLocalNetInformation();
            auto iter        = networkInfo.find(AgentSniffRequest::kPreferEth);
            if (iter != networkInfo.end())
            {
                ipInfo = iter->second.ipv4;
            }
            else
            {
                for (auto& item : networkInfo)
                {
                    if (!item.second.isLoopback)
                        ipInfo = item.second.ipv4;
                }
            }
        }
        AgentSniffResponse responseData;
        responseData.host    = ipInfo;
        SizeType payloadSize = responseData.host.GetSize() +
                               sizeof(responseData.host.GetSize());
        response.payload.Reallocate(payloadSize);
        Fundamental::BufferWriter<SizeType> writer;
        writer.SetBuffer(response.payload.GetData(), response.payload.GetSize());
        writer.WriteRawMemory(responseData.host);
        connection->HandleResponse(response);
    }
    static void ProcessQuery(AgentRequestFrame& request, std::shared_ptr<AgentConnection> connection)
    {
        using SizeType = decltype(request.payload)::SizeType;
        Fundamental::BufferReader<SizeType> reader;
        reader.SetBuffer(request.payload.GetData(), request.payload.GetSize());
        std::shared_ptr<AgentQueryRequest> requestData   = std::make_shared<AgentQueryRequest>();
        std::shared_ptr<AgentResponseFrame> response     = std::make_shared<AgentResponseFrame>();
        std::shared_ptr<AgentQueryResponse> responseData = std::make_shared<AgentQueryResponse>();
        try
        {
            reader.ReadRawMemory(requestData->id);
            reader.ReadRawMemory(requestData->section);
            reader.ReadValue(&requestData->max_query_wait_time_sec);
        }
        catch (const std::exception&)
        {
            return;
        }
        auto delay_process_func = [=]() -> bool {
            response->cmd = request.cmd;
            if (!AgentStorage::Instance().QueryAgentInfo(requestData->id, requestData->section, *responseData))
            {
                if (requestData->max_query_wait_time_sec > 0)
                {
                    --requestData->max_query_wait_time_sec;
                    return false;
                }
                response->msg  = "not existed";
                response->code = AgentFailed;
            }
            SizeType payloadSize = sizeof(responseData->timestamp) +
                                   responseData->data.GetSize() +
                                   sizeof(responseData->data.GetSize());
            response->payload.Reallocate(payloadSize);
            Fundamental::BufferWriter<SizeType> writer;
            writer.SetBuffer(response->payload.GetData(), response->payload.GetSize());
            writer.WriteValue(&responseData->timestamp);
            writer.WriteRawMemory(responseData->data);
            connection->HandleResponse(*response);
            return true;
        };
        if (!delay_process_func())
        {
            connection->HandleDelayRequest(delay_process_func);
        }
    }
}; //  details
static Fundamental::ScopeGuard s_register(nullptr, []() {
    AgentConnection::cmd_handlers.emplace(std::string(AgentSniffRequest::kCmd), AgentConnection::details::ProcessSniff);
    AgentConnection::cmd_handlers.emplace(std::string(AgentUpdateRequest::kCmd), AgentConnection::details::ProcessUpdate);
    AgentConnection::cmd_handlers.emplace(std::string(AgentQueryRequest::kCmd), AgentConnection::details::ProcessQuery);
});

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
    if (delay_timer)
        delay_timer->cancel();
}

void AgentConnection::ProcessCmd()
{
    do
    {
        AgentRequestFrame request;
        if (!PaserRequestFrame(request))
        {
            FERR("invalid agent request");
            break;
        }

        auto iter = cmd_handlers.find(request.cmd);
        if (iter == cmd_handlers.end() || !iter->second)
        {
            FERR("unsupported cmd:{}", request.cmd.ToString());
            break;
        };
        iter->second(request, shared_from_this());
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
    catch (const std::exception& e)
    {
        FDEBUG("e:{}", e.what());
        return false;
    }
    return true;
}

void AgentConnection::HandleResponse(AgentResponseFrame& response)
{
    std::shared_ptr<ProxyFrame> responseFrame = std::make_shared<ProxyFrame>();
    std::size_t payloadSize                   = 3 * sizeof(response.cmd.GetSize()) +
                              response.cmd.GetSize() +
                              response.msg.GetSize() +
                              response.payload.GetSize() + sizeof(response.code);
    responseFrame->op = frame.op;
    responseFrame->payload.Reallocate(payloadSize);
    using SizeType = decltype(ProxyFrame::payload)::SizeType;
    Fundamental::BufferWriter<SizeType> writer;
    writer.SetBuffer(responseFrame->payload.GetData(), responseFrame->payload.GetSize());
    writer.WriteRawMemory(response.cmd);
    writer.WriteValue(&response.code);
    writer.WriteRawMemory(response.msg);
    writer.WriteRawMemory(response.payload);
    ProxyRequestHandler::EncodeFrame(*responseFrame);
    asio::async_write(socket_, responseFrame->ToAsioBuffers(),
                      [this, self = shared_from_this(), responseFrame](std::error_code ec, std::size_t transfer_bytes) {
                          FDEBUG("agent reponse transfer bytes:{} ec:{}", transfer_bytes, ec.message());
                          socket_.cancel();
                          if (delay_timer)
                              delay_timer->cancel();
                      });
}

void AgentConnection::HandleDelayRequest(const std::function<bool()>& check_func)
{
    delay_timer = std::make_shared<asio::steady_timer>(socket_.get_executor(), asio::chrono::seconds(1));
    TimeoutProcess(check_func);
    asio::async_read(socket_, asio::buffer(&delay_buf, 1),
                     [this, self = shared_from_this()](std::error_code ec, std::size_t) {
                         if (ec)
                         {
                             FDEBUG("disconnected for delay request:[{}-{}] :{}", ec.category().name(), ec.value(), ec.message());
                         }
                         delay_timer->cancel();
                     });
}

void AgentConnection::TimeoutProcess(const std::function<bool()>& check_func)
{
    delay_timer->expires_after(asio::chrono::seconds(1));
    delay_timer->async_wait([this, self = shared_from_this(), check_func](const std::error_code& ec) {
        if (!ec)
        {
            if (!check_func())
            {
                TimeoutProcess(check_func);
            }
        }
        else
        {
            FDEBUG("timer stop :[{}-{}] :{}", ec.category().name(), ec.value(), ec.message());
        }
    });
}

} // namespace proxy
} // namespace network
