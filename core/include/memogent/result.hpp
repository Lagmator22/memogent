// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <utility>
#include <variant>

namespace mem {

enum class ErrorCode : int {
    Ok = 0,
    InvalidArgument = 1,
    NotFound = 2,
    AlreadyExists = 3,
    FailedPrecondition = 4,
    OutOfRange = 5,
    Unimplemented = 6,
    Internal = 7,
    Unavailable = 8,
    DataCorrupted = 9,
    IoError = 10,
};

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;

    Error() = default;
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
};

// Lightweight Result<T,Error> without requiring std::expected on older toolchains.
template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error err) : data_(std::move(err)) {}

    [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(data_); }
    [[nodiscard]] bool has_value() const noexcept { return ok(); }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] const T& value() const& { return std::get<T>(data_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(data_)); }
    [[nodiscard]] const Error& error() const { return std::get<Error>(data_); }

private:
    std::variant<T, Error> data_;
};

}  // namespace mem
