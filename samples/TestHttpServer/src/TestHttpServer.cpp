
#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/md5_utils.hpp"
#include "http/HttpServer.h"
#include <iostream>
#include <optional>
using namespace network;
void OnHttpJson(http::HttpConnect& con, http::HttpRequest& request) {
    // Test response json data
    con.SetBody("{\"code\":0,\"message\":\"hello http standalone\"}");
    // set Content-Type header value, default is json
    con.SetContentType("json");
    // Get request header value
    // request.GetHeader("auth");
    // SetStatus default is http::Reply::ok
    // con.SetStatus(http::Reply::ok);
    // Get query value
    // auto& queryVal = request.GetQueryParam("a");
    std::cout << "Content-Type:" << request.GetHeader("Content-Type") << std::endl;
    std::cout << "request playload size:" << request.GetBody().size() << std::endl;
    // std::cout << "request playload:" << request.GetBody() << std::endl;
}

void OnHttpBinary(http::HttpConnect& con, http::HttpRequest& request) {
    // Test response binary data
    for (int i = 0; i < 1024 * 1024 * 26; ++i) {
        con.AppendBody("{\"code\":0,\"message\":\"hello http standalone\"}");
    }
    con.SetContentType("binary");
    con.SetStatus(http::Reply::ok);
}

void OnFile(http::HttpConnect& con, http::HttpRequest& request) {
    auto method = request.GetMethod();
    FINFO("method:", method);
    FINFO("uri:", request.GetUri());
    auto& file = request.GetQueryParam("file");
    FINFO("file=", file);
    auto& headers      = request.GetAllHeaders();
    std::string origin = "*";
    for (auto& i : headers) {
        if (i.name == "origin") {
            origin = i.value;
            break;
        }
    }
    if (file.empty()) {
        FWARN("file can't be empty!");
        con.SetStatus(http::Reply::StatusType::forbidden);
        return;
    }
    if (method == "GET") {
        std::string dataStr;
        bool ret = Fundamental::fs::ReadFile(file, dataStr);
        if (!ret) {
            con.SetStatus(http::Reply::StatusType::forbidden);
            con.SetBody(file + " not found!");
        } else {
            FINFO("size:", dataStr.size());
            con.SetContentType("binary");
            con.SetBody(dataStr);
            con.AddHeader("Access-Control-Allow-Origin", origin);
            con.SetStatus(http::Reply::ok);
        }
    } else if (method == "POST") {
        decltype(auto) body = request.GetBody();
        FINFO(" size:", body.size());
        bool ret = Fundamental::fs::WriteFile(file, body.data(), body.size(), true);
        if (!ret) {
            con.SetStatus(http::Reply::StatusType::forbidden);
            con.SetBody(file + " is inaccessible!");
        }
        std::string dataStr = Fundamental::CryptMD5(body.data(), static_cast<std::uint32_t>(body.size()));
        con.SetContentType("binary");
        con.SetBody(dataStr);
        con.AddHeader("Access-Control-Allow-Origin", origin);
        con.SetStatus(http::Reply::ok);
    } else {
        con.SetStatus(http::Reply::StatusType::forbidden);
        con.SetBody(method + " is not supported!");
    }
}

int main() {
    http::HttpServer server;
    server.RegistHandler("/json", OnHttpJson, http::MethodFilter::HttpGet);
    server.RegistHandler("/binary", OnHttpBinary, http::MethodFilter::HttpAll);
    server.RegistHandler("/file", OnFile, http::MethodFilter::HttpGet | http::MethodFilter::HttpPost);
    server.Start(1200);
    return 0;
}