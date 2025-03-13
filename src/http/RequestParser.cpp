
#include "RequestParser.hpp"
#include "Request.hpp"

namespace network::http
{

RequestParser::RequestParser()
  : m_state(method_start)
{
}

void RequestParser::Reset()
{
    m_state = method_start;
}

std::optional<bool> RequestParser::Consume(Request& req, char** ppInput, std::size_t* pLeftSize)
{
    char input = *(*ppInput);
    if (m_state != body)
    {// Not body state,comsume one by one
        ++(*ppInput);
        --(*pLeftSize);
    }
    switch (m_state)
    {
    case method_start:
    if (!IsChar(input) || IsCtl(input) || IsTspecial(input))
    {
        return false;
    }
    else
    {
        m_state = method;
        req.method.push_back(input);
        return std::nullopt; // boost::indeterminate;
    }
    case method:
    if (input == ' ')
    {
        m_state = uri;
        return std::nullopt; // boost::indeterminate;
    }
    else if (!IsChar(input) || IsCtl(input) || IsTspecial(input))
    {
        return false;
    }
    else
    {
        req.method.push_back(input);
        return std::nullopt; // boost::indeterminate;
    }
    case uri:
    if (input == ' ')
    {
        m_state = http_version_h;
        return std::nullopt; // boost::indeterminate;
    }
    else if (IsCtl(input))
    {
        return false;
    }
    else
    {
        req.uri.push_back(input);
        return std::nullopt; // boost::indeterminate;
    }
    case http_version_h:
    if (input == 'H')
    {
        m_state = http_version_t_1;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_t_1:
    if (input == 'T')
    {
        m_state = http_version_t_2;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_t_2:
    if (input == 'T')
    {
        m_state = http_version_p;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_p:
    if (input == 'P')
    {
        m_state = http_version_slash;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_slash:
    if (input == '/')
    {
        req.http_version_major = 0;
        req.http_version_minor = 0;
        m_state = http_version_major_start;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_major_start:
        if (IsDigit(input))
    {
        req.http_version_major = req.http_version_major * 10 + input - '0';
        m_state = http_version_major;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_major:
    if (input == '.')
    {
        m_state = http_version_minor_start;
        return std::nullopt; // boost::indeterminate;
    }
    else if (IsDigit(input))
    {
        req.http_version_major = req.http_version_major * 10 + input - '0';
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_minor_start:
        if (IsDigit(input))
    {
        req.http_version_minor = req.http_version_minor * 10 + input - '0';
        m_state = http_version_minor;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case http_version_minor:
    if (input == '\r')
    {
        m_state = expecting_newline_1;
        return std::nullopt; // boost::indeterminate;
    }
    else if (IsDigit(input))
    {
        req.http_version_minor = req.http_version_minor * 10 + input - '0';
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case expecting_newline_1:
    if (input == '\n')
    {
        m_state = header_line_start;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case header_line_start:
    if (input == '\r')
    {
        m_state = expecting_newline_3;
        return std::nullopt; // boost::indeterminate;
    }
    else if (!req.headers.empty() && (input == ' ' || input == '\t'))
    {
        m_state = header_lws;
        return std::nullopt; // boost::indeterminate;
    }
    else if (!IsChar(input) || IsCtl(input) || IsTspecial(input))
    {
        return false;
    }
    else
    {
        req.headers.push_back(Header());
        req.headers.back().name.push_back(input);
        m_state = header_name;
        return std::nullopt; // boost::indeterminate;
    }
    case header_lws:
    if (input == '\r')
    {
        m_state = expecting_newline_2;
        return std::nullopt; // boost::indeterminate;
    }
    else if (input == ' ' || input == '\t')
    {
        return std::nullopt; // boost::indeterminate;
    }
    else if (IsCtl(input))
    {
        return false;
    }
    else
    {
        m_state = header_value;
        req.headers.back().value.push_back(input);
        return std::nullopt; // boost::indeterminate;
    }
    case header_name:
    if (input == ':')
    {
        m_state = space_before_header_value;
        return std::nullopt; // boost::indeterminate;
    }
    else if (!IsChar(input) || IsCtl(input) || IsTspecial(input))
    {
        return false;
    }
    else
    {
        req.headers.back().name.push_back(input);
        return std::nullopt; // boost::indeterminate;
    }
    case space_before_header_value:
    if (input == ' ')
    {
        m_state = header_value;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case header_value:
    if (input == '\r')
    {
        m_state = expecting_newline_2;
        return std::nullopt; // boost::indeterminate;
    }
    else if (IsCtl(input))
    {
        return false;
    }
    else
    {
        req.headers.back().value.push_back(input);
        return std::nullopt; // boost::indeterminate;
    }
    case expecting_newline_2:
    if (input == '\n')
    {
        m_state = header_line_start;
        return std::nullopt; // boost::indeterminate;
    }
    else
    {
        return false;
    }
    case expecting_newline_3:
    {
        req.contentTransferred = 0;
        auto result = (input == '\n');
        if (!result)
            return false;
        if (req.isDataTransmissionFinishWhenParseAllHeader())
            return result;
        // Ready to append body data
        m_state = body;
        return std::nullopt;
    }
    case body:
    {
        // Consume all size
        req.contentTransferred += (*pLeftSize);
        req.content.append(*ppInput, *pLeftSize);
        (*ppInput) += (*pLeftSize);
        *pLeftSize = 0;
        if (req.isDataTransmissionFinishWhenParseAllHeader())
            return true;
        return std::nullopt;
    }
    default:
    return false;
    }
}

bool RequestParser::IsChar(int c)
{
    return c >= 0 && c <= 127;
}

bool RequestParser::IsCtl(int c)
{
    return (c >= 0 && c <= 31) || (c == 127);
}

bool RequestParser::IsTspecial(int c)
{
    switch (c)
    {
    case '(': case ')': case '<': case '>': case '@':
    case ',': case ';': case ':': case '\\': case '"':
    case '/': case '[': case ']': case '?': case '=':
    case '{': case '}': case ' ': case '\t':
    return true;
    default:
    return false;
    }
}

bool RequestParser::IsDigit(int c)
{
    return c >= '0' && c <= '9';
}

} // namespace network::http