#include "agent_client.hpp"
#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "network/server/io_context_pool.hpp"
#include <array>
#include <atomic>
#include <memory>
namespace network
{
namespace proxy
{
namespace rpc
{
struct AgentClient::AgentSession : public std::enable_shared_from_this<AgentSession>
{
    enum SessionStatus : std::uint8_t
    {
        AgentNotStarted,
        AgentDnsResolving,
        AgentConnecting,
        AgentSending,
        AgentRecving,
        AgentFinishedSuccess,
        AgentFinishedFailed,
        AgentCancelled
    };
    AgentSession(const std::string& host, const std::string& service, const AgentFinishedCallback& cb,
                 ProxyFrame&& frame);
    ~AgentSession();
    void Start();
    void Cancel();
    void StartDnsResolve();
    void StartConnect(asio::ip::tcp::resolver::results_type&& result);
    void SendRequest();
    void RecvHeader();
    void RecvBody();
    void HandleResponse();
    void HandleFailed(asio::error_code e);
    Fundamental::Signal<void()> finish;
    std::string host;
    std::string service;
    ProxyFrame frame;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver;
    std::atomic<SessionStatus> status_   = AgentNotStarted;
    const AgentFinishedCallback finishCb = nullptr;
    // response
    std::array<std::uint8_t, ProxyFrame::kHeaderSize> headerBuffer;
    ProxyFrame responseFrame;
};

struct AgentClient::AgentHandler
{
    template <typename ContextType>
    static void PackageProxyFrame(AgentRequestFrame& frame, ContextType& context);

    template <typename ContextType>
    static AgentClientToken HandleAgentRequest(AgentClient* client, ContextType& context);
};

AgentClient::AgentClient()
{
}

AgentClient::~AgentClient()
{
}

bool AgentClient::CancelRequest(AgentClientToken token)
{
    std::scoped_lock<std::mutex> locker(mutex_);
    auto iter = sessions_.find(token);
    do
    {
        if (iter == sessions_.end())
            break;
        auto ptr = iter->second.lock();
        if (!ptr)
        { // maybe some bugs existed if the program reach this case
            sessions_.erase(iter);
            FASSERT(false, "maybe some bugs existed if the program reach this case");
            break;
        }
        ptr->Cancel();
    } while (0);
    return false;
}

AgentClientToken AgentClient::Update(const AgentUpdateContext& context)
{
    return AgentHandler::HandleAgentRequest(this, context);
}

AgentClientToken AgentClient::Query(const AgentQueryContext& context)
{
    return AgentHandler::HandleAgentRequest(this, context);
}

template <>
void AgentClient::AgentHandler::PackageProxyFrame<AgentUpdateContext>(AgentRequestFrame& frame, AgentUpdateContext& context)
{
    using SizeType       = decltype(frame.payload)::SizeType;
    SizeType payloadSize = sizeof(SizeType) * 3 +
                           context.request.data.GetSize() + context.request.id.GetSize() +
                           context.request.section.GetSize();
    frame.payload.Reallocate(payloadSize);
    Fundamental::BufferWriter<SizeType> writer;
    writer.SetBuffer(frame.payload.GetData(), frame.payload.GetSize());
    writer.WriteRawMemory(context.request.id);
    writer.WriteRawMemory(context.request.section);
    writer.WriteRawMemory(context.request.data);
}

template <>
void AgentClient::AgentHandler::PackageProxyFrame<AgentQueryContext>(AgentRequestFrame& frame, AgentQueryContext& context)
{
    using SizeType       = decltype(frame.payload)::SizeType;
    SizeType payloadSize = sizeof(SizeType) * 2 +
                           context.request.id.GetSize() +
                           context.request.section.GetSize();
    frame.payload.Reallocate(payloadSize);
    Fundamental::BufferWriter<SizeType> writer;
    writer.SetBuffer(frame.payload.GetData(), frame.payload.GetSize());
    writer.WriteRawMemory(context.request.id);
    writer.WriteRawMemory(context.request.section);
}

template <typename ContextType>
void AgentClient::AgentHandler::PackageProxyFrame(AgentRequestFrame& frame, ContextType& context)
{
    //just do noting
}

template <typename ContextType>
AgentClientToken AgentClient::AgentHandler::HandleAgentRequest(AgentClient* client, ContextType& context)
{
    ProxyFrame frame;
    frame.op = ProxyOpCode::AgentServiceOp;
    AgentRequestFrame requestFrame;
    PackageProxyFrame(requestFrame, context);
    {
        using SizeType       = typename decltype(frame.payload)::SizeType;
        SizeType payloadSize = sizeof(SizeType) * 2 + requestFrame.cmd.GetSize() +
                               requestFrame.payload.GetSize() +
                               sizeof(requestFrame.version);
        frame.payload.Reallocate(payloadSize);
        Fundamental::BufferWriter<SizeType> writer;
        writer.SetBuffer(frame.payload.GetData(), frame.payload.GetSize());
        writer.WriteRawMemory(requestFrame.cmd);
        writer.WriteValue(&requestFrame.version);
        writer.WriteRawMemory(requestFrame.payload);
    }
    ProxyRequestHandler::EncodeFrame(frame);
    auto newSession        = std::make_shared<AgentSession>(context.host, context.service, context.cb, std::move(frame));
    AgentClientToken token = reinterpret_cast<AgentClientToken>(newSession.get());
    {
        std::scoped_lock<std::mutex> locker(client->mutex_);
        client->sessions_.emplace(token, newSession);
    }
    newSession->finish.Connect([client, token]() {
        std::scoped_lock<std::mutex> locker(client->mutex_);
        client->sessions_.erase(token);
    });
    newSession->Start();
    return token;
}

AgentClient::AgentSession::AgentSession(const std::string& host,
                                        const std::string& service,
                                        const AgentFinishedCallback& cb,
                                        ProxyFrame&& frame) :
host(host),
service(service),
frame(std::move(frame)),
socket_(io_context_pool::Instance().get_io_context()),
resolver(socket_.get_executor()),
finishCb(cb)
{
}

AgentClient::AgentSession::~AgentSession()
{
    finish.Emit();
}

void AgentClient::AgentSession::Start()
{
    StartDnsResolve();
}

void AgentClient::AgentSession::Cancel()
{
    auto ref                = shared_from_this();
    SessionStatus old_Value = status_.load();
    for (;;)
    {
        switch (old_Value)
        {
        case AgentDnsResolving:
        {
            if (status_.compare_exchange_strong(old_Value, AgentCancelled))
            {
                asio::post(resolver.get_executor(),
                           [ref, this]() {
                               resolver.cancel();
                           });
                return;
            } // else means another thread change the value ,we should determine the new value
        }
        break;
        case AgentConnecting:
        case AgentSending:
        case AgentRecving:
        {

            if (status_.compare_exchange_strong(old_Value, AgentCancelled))
            {
                asio::post(socket_.get_executor(),
                           [ref, this]() {
                               socket_.close();
                           });
                return;
            }
        }
        break;
        default:
        { // just do nothing
            return;
        }
        }
    }
}

void AgentClient::AgentSession::StartDnsResolve()
{
    status_.exchange(SessionStatus::AgentDnsResolving);
    resolver.async_resolve(asio::ip::tcp::v4(),
                           host, service,
                           [ref = shared_from_this(), this](asio::error_code ec,
                                                            decltype(resolver)::results_type result) {
                               if (ec || status_ == AgentCancelled)
                               {
                                   HandleFailed(ec);
                                   return;
                               }
                               StartConnect(std::move(result));
                           });
}

void AgentClient::AgentSession::StartConnect(asio::ip::tcp::resolver::results_type&& result)
{
    status_.exchange(SessionStatus::AgentConnecting);
    asio::async_connect(socket_, result,
                        [this, self = shared_from_this()](std::error_code ec, asio::ip::tcp::endpoint) {
                            if (ec || status_ == AgentCancelled)
                            {
                                HandleFailed(ec);
                                return;
                            }
                            SendRequest();
                        });
}

void AgentClient::AgentSession::SendRequest()
{
    status_.exchange(SessionStatus::AgentSending);
    auto buffers = ProxyRequestHandler::FrameToBuffers(frame);
    asio::async_write(socket_, std::move(buffers),
                      [this, self = shared_from_this()](std::error_code ec, std::size_t) {
                          if (ec || status_ == AgentCancelled)
                          {
                              HandleFailed(ec);
                              return;
                          }
                          RecvHeader();
                      });
}

void AgentClient::AgentSession::RecvHeader()
{
    asio::async_read(socket_, asio::buffer(headerBuffer.data(), headerBuffer.size()),
                     [this, self = shared_from_this()](std::error_code ec, std::size_t bytes_transferred) {
                         do
                         {
                             if (ec || status_ == AgentCancelled)
                             {
                                 break;
                             }
                             if (!ProxyRequestHandler::DecodeHeader(headerBuffer.data(),
                                                                    headerBuffer.size(), responseFrame))
                             {
                                 break;
                             }
                             RecvBody();
                             return;
                         } while (0);
                         HandleFailed(ec);
                     });
}

void AgentClient::AgentSession::RecvBody()
{
    asio::async_read(socket_, asio::buffer(responseFrame.payload.GetData(), responseFrame.payload.GetSize()),
                     [this, self = shared_from_this()](std::error_code ec, std::size_t bytes_transferred) {
                         do
                         {
                             if (ec || status_ == AgentCancelled)
                             {
                                 break;
                             }
                             if (!ProxyRequestHandler::DecodePayload(responseFrame))
                             {
                                 break;
                             }
                             HandleResponse();
                             return;
                         } while (0);
                         HandleFailed(ec);
                     });
}

void AgentClient::AgentSession::HandleResponse()
{
    using SizeType = decltype(responseFrame.payload)::SizeType;
    AgentResponseFrame res;
    Fundamental::BufferReader<SizeType> reader;
    reader.SetBuffer(responseFrame.payload.GetData(), responseFrame.payload.GetSize());
    try
    {
        reader.ReadRawMemory(res.cmd);
        reader.ReadValue(&res.code);
        reader.ReadRawMemory(res.msg);
        reader.ReadRawMemory(res.payload);
        status_.exchange(AgentFinishedSuccess);
        FDEBUG("recv {}  code:{} response size {}", res.cmd.ToString(), res.code, res.payload.GetSize());
        AgentResponse tmp;
        tmp.code = res.code;
        tmp.msg  = res.msg.ToString();
        tmp.data = res.payload.ToVec();
        if (finishCb)
            finishCb(true, reinterpret_cast<AgentClientToken>(this), std::move(tmp));
    }
    catch (const std::exception&)
    {
        HandleFailed({});
    }
}

void AgentClient::AgentSession::HandleFailed(asio::error_code ec)
{
    FDEBUG("request failed for {} {} final status{}", host, service, static_cast<std::int32_t>(status_.load()));
    if (status_ != AgentCancelled)
        status_.exchange(AgentFinishedFailed);
    if (finishCb)
    {
        finishCb(false,
                 reinterpret_cast<AgentClientToken>(this),
                 AgentResponse {});
    }
}

} // namespace rpc
} // namespace proxy
} // namespace network