#include "http_server.hpp"
#include "http_connection.h"
#include "http_request.hpp"
#include "http_response.hpp"
#include "http_router.hpp"

#include "fundamental/basic/filesystem_utils.hpp"
#include "fundamental/basic/url_utils.hpp"
#include "fundamental/thread_pool/thread_pool.h"
#include "istream"
namespace network::http
{
const http_handler http_server::s_default_file_handler =
    [](std::shared_ptr<http_connection> conn, http_response& response, http_request& request) {
        do {
            auto root_path = conn->get_root_path();
            if (root_path.empty()) {
                response.stock_response(http_response::response_type::forbidden);
                return;
            }
            auto method = request.get_method();
            if (method != MethodFilter::HttpGet) {
                response.stock_response(http_response::response_type::not_implemented);
                break;
            }
            //     // Decode url to path.
            std::string requestPath = Fundamental::UrlDecode(request.get_uri());
            if (requestPath.empty()) {
                requestPath = "/";
            } else {
                // Request path must be absolute and not contain "..".
                if (requestPath[0] != '/' || requestPath.find("..") != std::string::npos) {
                    response.stock_response(http_response::response_type::bad_request);
                    return;
                }
            }

            // If path ends in slash (i.e. is a directory) then add "index.html".
            if (requestPath[requestPath.size() - 1] == '/') {
                requestPath += "index.html";
            }

            // Determine the file extension.
            std::size_t last_slash_pos = requestPath.find_last_of("/");
            std::size_t last_dot_pos   = requestPath.find_last_of(".");
            std::string extension;
            if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos) {
                extension = requestPath.substr(last_dot_pos + 1);
            }

            // Open the file to send back.
            std::string full_path = root_path + requestPath;
            if (!std::filesystem::is_regular_file(full_path)) {
                response.stock_response(http_response::response_type::not_found);
                break;
            }
            response.set_raw_content_type(ExtensionToType(extension));
            auto& pool = Fundamental::ThreadPool::DefaultPool();
            if (pool.Count() < 4) pool.Spawn(4 - pool.Count());
            pool.Enqueue([conn, full_path]() {
                auto& response = conn->get_response();
                Fundamental::ScopeGuard g([&]() { response.perform_response(); });

                std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
                if (!is) {
                    response.stock_response(http_response::not_found);
                    return;
                }
                auto size = std::filesystem::file_size(full_path);
                // notify headeres already
                response.set_body_size(size);
                std::array<std::uint8_t, 8192> read_buf;
                std::condition_variable cv;
                std::mutex notify_mutex;
                std::size_t max_pending_size = 4 * 1024 * 1024; // 4M
                std::size_t max_wait_ms      = 5000;
                auto h                       = response.notify_pending_size.Connect([&](std::size_t pending_size) {
                    if (pending_size >= max_pending_size) return;
                    std::scoped_lock<std::mutex> locker(notify_mutex);
                    cv.notify_one();
                });
                Fundamental::ScopeGuard g2([&]() { // disconnect to wait for send finished
                    response.notify_pending_size.DisConnect(h);
                });
                while (size > 0) {
                    auto read_count = is.read((char*)read_buf.data(), read_buf.size()).gcount();
                    if (read_count > 0) {
                        auto read_size = static_cast<std::size_t>(read_count);
                        response.append_body(read_buf.data(), read_size);
                        if (read_size >= size) {
                            break;
                        }
                        size -= read_size;
                        std::unique_lock<std::mutex> locker(notify_mutex);
                        auto ret = cv.wait_for(locker, std::chrono::milliseconds(max_wait_ms), [&]() -> bool {
                            return response.get_data_pending_size() < max_pending_size;
                        });
                        if (!ret) {
                            FDEBUG("http connection send abort,network is too poor");
                            break;
                        }
                    } else {
                        break;
                    }
                }
            });

        } while (0);
    };

void http_server::enable_default_handler(http_handler handler, std::uint32_t methodMask) {
    router_.set_default_route_table({ handler, methodMask });
}
} // namespace network::http
