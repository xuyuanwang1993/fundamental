#pragma once
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
    #endif
#endif

#include <string>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include "MimeTypes.hpp"
#include "Reply.hpp"

namespace network::http
{

enum MethodFilter : std::uint32_t
{
    HttpNone     = 0,
    HttpGet      = 0x1,
    HttpPost     = 0x2,
    HttpPut      = 0x4,
    HttpDelete   = 0x8,
    HttpPatch    = 0x10,
    HttpCopy     = 0x20,
    HttpHead     = 0x40,
    HttpOptions  = 0x80,
    HttpLink     = 0x100,
    HttpUnlink   = 0x200,
    HttpPurge    = 0x400,
    HttpLock     = 0x800,
    HttpUnlock   = 0x1000,
    HttpPropfind = 0x2000,
    HttpView     = 0x4000,
    HttpAll      = 0xffffffff,
};
class HttpConnect;
class HttpRequest;
using HttpHandler = std::function<void(HttpConnect&, HttpRequest&)>;
class Server;
struct Reply;
struct Request;
class HttpServer
{

public:
     HttpServer() = default;
      ~HttpServer() = default;

    // Sets if request header's name is case-sensitive.
    // Default is false(it means case-insensitive).
     void SetRequestHeadersNameCaseSensitive(bool yes);
    // Start http server
    // If threadNums is 0 or static_cast<>(-1), (default)thread pool size will assigned std::thread::hardware_concurrency()
     void Start(std::uint16_t port, const std::size_t threadNums = 0);

     void Stop();

public:
    // Regist request pattern with handler function and method mask.
    // Example like: RegistHandler("/createtask",[](HttpConnect& con, HttpRequest& request){},
    // MethodFilter::HttpGet | MethodFilter::HttpPost)
     void RegistHandler(const std::string& pattern, HttpHandler handler, std::uint32_t methodMask);
     std::pair<HttpHandler, std::uint32_t>& GetHandler(const std::string& pattern);
     static MethodFilter MethodStringToMethodFilter(const std::string& httpMethod);

private:
    void OnHttp(Reply& reply, Request& request);

private:
    Server* m_pServerRef = nullptr;
    // Marked if request header's name is case-sensitive.
    bool m_requestHeadersNameCaseSensitive = false;
    std::size_t m_threadNums = 1;
    std::mutex m_mutex;
    // Request pattern to handler map.
    std::unordered_map<std::string, std::pair<HttpHandler, std::uint32_t>> m_handlerMap;
    std::pair<HttpHandler, std::uint32_t> m_handlerEmpty = std::make_pair(nullptr, 0);
};

class  HttpConnect
{
public:
    explicit HttpConnect(Reply& reply);
    ~HttpConnect() = default;
    // Set content value.
    void SetBody(const std::string& value);
    void SetBody(std::string&& value);
    // Append value to content.
    void AppendBody(const std::string& value);
    // Set status code. if not set, default is Reply::ok.
    void SetStatus(Reply::StatusType status);
    // Set content-type. if not set, default is 'json'. 
    // It will use MimeTypes::ExtensionToType to convert `type`.
    void SetContentType(const std::string& type);
    // Set raw content-type.
    void SetRawContentType(const std::string& typeValue);
    // Add header info.
    void AddHeader(const std::string& name, const std::string& value);
    // Get current status
    Reply::StatusType GetStatus() const;

private:
    Reply& m_reply;
};

class  HttpRequest
{
public:
     explicit HttpRequest(Request& request, 
        bool headerNameCaseSensitive, const std::string& queryStr);
      ~HttpRequest() = default;
    // Gets header value with the name, name is case-insensitive.
     const std::string& GetHeader(const std::string& name);
    // Gets all headers
     const std::vector<Header>& GetAllHeaders();
    // Gets body content
     const std::string& GetBody();
    // Gets client's ip. It's ipv6 string if it's ipv6 type
     const std::string& GetIP();
    // Gets client's port
     std::int32_t GetPort();
    // Get Body content and remove it from request
     void PeekBody(std::string& body);
    // Gets query param with the name
     const std::string& GetQueryParam(const std::string& name);
    // Gets all query params, store in outMap.
     void GetAllQueryParam(std::unordered_map<std::string, std::string>& outMap);
    // Gets query str
     std::string GetQueryStr();
    // get http request method
     std::string GetMethod() const;
    //get http request Uri
     std::string GetUri() const;

    friend class HttpServer;

private:
    Request& m_request;
    std::unordered_map<std::string, std::string> m_queryParam;
    std::unordered_map<std::string, std::string> m_headers;
    std::string m_emptyString;
    bool m_isHeaderNameCaseSensitive = false;
    std::string m_queryStr;
};
    
} // namespace network::http
