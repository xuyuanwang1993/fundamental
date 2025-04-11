#pragma once
#include <exception>
#include <string>
#include <string_view>
#include <system_error>

namespace Fundamental
{

using error_category = std::error_category;
class error_code : public std::error_code {
public:
    error_code() noexcept : std::error_code() {
    }

    error_code(int __v, const error_category& __cat) noexcept : std::error_code(__v, __cat) {
    }

    error_code(int __v, const error_category& __cat, std::string_view details) :
    std::error_code(__v, __cat), details__(details) {
    }

    decltype(auto) make_excepiton() const {
        return std::system_error(*this, details__);
    }
    decltype(auto) make_exception_ptr() const {
        return std::make_exception_ptr(make_excepiton());
    }

    const char* details_c_str() const noexcept {
        return details__.c_str();
    }

    const std::string& details() const noexcept {
        return details__;
    }

    std::string_view details_view() const noexcept {
        return details__;
    }

private:
    const std::string details__;
};

template <typename T>
inline error_code make_error_code(T e, const error_category& __cat, std::string_view details = {}) {
    return error_code(static_cast<std::int32_t>(e), __cat, details);
}
} // namespace Fundamental