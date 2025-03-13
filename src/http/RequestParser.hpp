
#pragma once

#include <tuple>
#include <optional>
#include "Request.hpp"

namespace network::http
{

struct Request;

/// Parser for incoming requests.
class RequestParser
{
public:
    /// Construct ready to parse the request method.
    RequestParser();

    /// Reset to initial parser state.
    void Reset();

    /// Parse some data. The std::optional<bool> return value is true when a complete request
    /// has been parsed, false if the data is invalid, std::nullopt when more
    /// data is required. The InputIterator return value indicates how much of the
    /// input has been consumed.
    template <typename InputIterator>
    std::tuple<std::optional<bool>, InputIterator> Parse(Request& req,
        InputIterator begin, InputIterator end, std::size_t totalSize)
    {
        auto leftSize = totalSize;
        while (begin != end)
        {
            std::optional<bool> result = Consume(req, &begin, &leftSize);
            if (result.has_value() /*result || !result*/)
            {
                return std::make_tuple(result, begin);
            }
        }
        return std::make_tuple(std::nullopt, begin);
    }

private:
    /// Handle the next character of input.
    std::optional<bool> Consume(Request& req, char** ppInput, std::size_t* pLeftSize);

    /// Check if a byte is an HTTP character.
    static bool IsChar(int c);

    /// Check if a byte is an HTTP control character.
    static bool IsCtl(int c);

    /// Check if a byte is defined as an HTTP tspecial character.
    static bool IsTspecial(int c);

    /// Check if a byte is a digit.
    static bool IsDigit(int c);

    /// The current state of the parser.
    enum state
    {
        method_start,
        method,
        uri,
        http_version_h,
        http_version_t_1,
        http_version_t_2,
        http_version_p,
        http_version_slash,
        http_version_major_start,
        http_version_major,
        http_version_minor_start,
        http_version_minor,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3,
        body    // parse body state
    } m_state;
};

} // namespace network::http