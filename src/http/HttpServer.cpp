#include "Server.hpp"

#include "HttpServer.h"
#include <thread>
#include <asio.hpp>

#include "fundamental/basic/string_utils.hpp"
#include "fundamental/basic/url_utils.hpp"
namespace network::http
{

void HttpServer::SetRequestHeadersNameCaseSensitive(bool yes)
{
    m_requestHeadersNameCaseSensitive = yes;
}

void HttpServer::Start(std::uint16_t port, const std::size_t threadNums)
{
    assert(m_pServerRef == nullptr);
    try
    {
        std::string doc("");

        // Initialise the server.
        m_threadNums = threadNums;
        if (m_threadNums == 0)
            m_threadNums = std::thread::hardware_concurrency();
        if (m_threadNums <= 0) // in case of hardware_concurrency fail and return 0
            m_threadNums = 1;
        m_pServerRef = new Server(port, doc, m_threadNums);
        m_pServerRef->SetHttpHandler(std::bind(&HttpServer::OnHttp, this, std::placeholders::_1, std::placeholders::_2));
        std::cout << "http::HttpServer listen on "  << ":" << port << std::endl;
        // Run the server until stopped.
        m_pServerRef->Run();
    }
    catch (std::exception& e)
    {
        std::cerr << "HttpServer::Start exception: " << e.what() << "\n";
    }
}

void HttpServer::Stop()
{
    if (m_pServerRef)
        m_pServerRef->Stop();
}

void HttpServer::RegistHandler(const std::string& pattern,
                               HttpHandler handler, std::uint32_t methodMask)
{
    std::unique_lock guard(m_mutex);
    auto it = m_handlerMap.find(pattern);
    if (it == m_handlerMap.end())
        m_handlerMap[pattern] = std::make_pair(handler, methodMask);
    else
        std::cout << "RegistHandler pattern already exist!\n";
}

std::pair<HttpHandler, std::uint32_t>& HttpServer::GetHandler(const std::string& pattern)
{
    std::unique_lock guard(m_mutex);
    auto it = m_handlerMap.find(pattern);
    if (it != m_handlerMap.end())
        return it->second;
    return m_handlerEmpty;
}

MethodFilter HttpServer::MethodStringToMethodFilter(const std::string& httpMethod)
{
    if (httpMethod == "GET")
        return MethodFilter::HttpGet;
    if (httpMethod == "POST")
        return MethodFilter::HttpPost;
    if (httpMethod == "PUT")
        return MethodFilter::HttpPut;
    if (httpMethod == "DELETE")
        return MethodFilter::HttpDelete;
    if (httpMethod == "PATCH")
        return MethodFilter::HttpPatch;
    if (httpMethod == "COPY")
        return MethodFilter::HttpCopy;
    if (httpMethod == "HEAD")
        return MethodFilter::HttpHead;
    if (httpMethod == "OPTIONS")
        return MethodFilter::HttpOptions;
    if (httpMethod == "LINK")
        return MethodFilter::HttpLink;
    if (httpMethod == "UNLINK")
        return MethodFilter::HttpUnlink;
    if (httpMethod == "PURGE")
        return MethodFilter::HttpPurge;
    if (httpMethod == "LOCK")
        return MethodFilter::HttpLock;
    if (httpMethod == "UNLOCK")
        return MethodFilter::HttpUnlock;
    if (httpMethod == "PROPFIND")
        return MethodFilter::HttpPropfind;
    if (httpMethod == "VIEW")
        return MethodFilter::HttpView;
    return MethodFilter::HttpNone;
}

void HttpServer::OnHttp(Reply& reply, Request& request)
{
    auto& uri           = request.uri;
    auto indexQ         = uri.find_first_of('?');
    std::string pattern = uri;
    if (indexQ != std::string::npos)
    {
        pattern = uri.substr(0, indexQ);
    }

    auto& h     = GetHandler(pattern);
    auto method = MethodStringToMethodFilter(request.method);
    if (!h.first)
    {
        reply = Reply::StockReply(Reply::not_found);
    }
    else if (!(h.second & method))
    {
        reply = Reply::StockReply(Reply::not_implemented);
    }
    else
    {
        // Initialize HttpConnect and HttpRequest, and call h handler.
        std::string queryStr = (indexQ != std::string::npos) ? uri.substr(pattern.length() + 1) : std::string();
        HttpConnect connect(reply);
        HttpRequest req(request, m_requestHeadersNameCaseSensitive, queryStr);
        reply.status = Reply::ok;
        reply.headers.resize(2);
        reply.headers[0].name  = "Content-Length";
        reply.headers[1].name  = "Content-Type";
        reply.headers[1].value = MimeTypes::ExtensionToType("json");
        h.first(connect, req);
        reply.headers[0].value = std::to_string(reply.content.size());
    }
}

HttpConnect::HttpConnect(Reply& reply) :
m_reply(reply)
{
}

void HttpConnect::SetBody(const std::string& value)
{
    m_reply.content = value;
}

void HttpConnect::SetBody(std::string&& value)
{
    m_reply.content = std::move(value);
}

void HttpConnect::AppendBody(const std::string& value)
{
    m_reply.content.append(value);
}

void HttpConnect::SetStatus(Reply::StatusType status)
{
    m_reply.status = status;
}

void HttpConnect::SetContentType(const std::string& type)
{
    m_reply.headers[1].value = MimeTypes::ExtensionToType(type);
}

void HttpConnect::SetRawContentType(const std::string& typeValue)
{
    m_reply.headers[1].value = typeValue;
}

void HttpConnect::AddHeader(const std::string& name, const std::string& value)
{
    auto& header = m_reply.headers.emplace_back();
    header.name  = name;
    header.value = value;
}

Reply::StatusType HttpConnect::GetStatus() const
{
    return m_reply.status;
}

HttpRequest::HttpRequest(Request& request, bool headerNameCaseSensitive, const std::string& queryStr) :
m_request(request),
m_isHeaderNameCaseSensitive(headerNameCaseSensitive),
m_emptyString(),
m_queryStr(queryStr)
{
    // Set header key-value to map.
    for (auto& h : request.headers)
    {
        if (!m_isHeaderNameCaseSensitive)
            Fundamental::StringToLower(h.name);
        m_headers[h.name] = h.value;
    }

    // Set query param.
    if (!queryStr.empty())
    {
        auto vecStr = Fundamental::StringSplit(queryStr, '&');
        for (auto& it : vecStr)
        {
            auto vecPair = Fundamental::StringSplit(it, '=');
            if (vecPair.size() == 2)
            {
                m_queryParam[vecPair[0]] = Fundamental::UrlDecode(vecPair[1]);
            }
        }
    }
}

const std::string& HttpRequest::GetHeader(const std::string& name)
{
    auto it = m_headers.end();
    if (!m_isHeaderNameCaseSensitive)
    {
        std::string lowerStr(name);
        Fundamental::StringToLower(lowerStr);
        it = m_headers.find(lowerStr);
    }
    else
    {
        it = m_headers.find(name);
    }
    if (it != m_headers.end())
        return it->second;
    return m_emptyString;
}

const std::vector<Header>& HttpRequest::GetAllHeaders()
{
    return m_request.headers;
}

const std::string& HttpRequest::GetQueryParam(const std::string& name)
{
    auto it = m_queryParam.find(name);
    if (it != m_queryParam.end())
        return it->second;
    return m_emptyString;
}

void HttpRequest::GetAllQueryParam(std::unordered_map<std::string, std::string>& outMap)
{
    outMap.clear();
    for (auto& it : m_queryParam)
    {
        outMap[it.first] = it.second;
    }
}

std::string HttpRequest::GetQueryStr()
{
    return m_queryStr;
}

std::string HttpRequest::GetMethod() const
{
    return m_request.method;
}

std::string HttpRequest::GetUri() const
{
    return m_request.uri;
}

const std::string& HttpRequest::GetBody()
{
    return m_request.content;
}

const std::string& HttpRequest::GetIP()
{
    return m_request.ip;
}

std::int32_t HttpRequest::GetPort()
{
    return m_request.port;
}

void HttpRequest::PeekBody(std::string& body)
{
    body = std::move(m_request.content);
}

} // namespace network::http