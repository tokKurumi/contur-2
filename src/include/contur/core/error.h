/// @file error.h
/// @brief Error codes and Result<T> type for fallible operations.
///
/// Result<T> is inspired by Rust's Result<T, E> — it holds either a
/// success value of type T or an ErrorCode indicating what went wrong.
/// A specialization Result<void> is provided for operations that produce
/// no value on success.

#pragma once

#include <cassert>
#include <cstdint>
#include <string_view>
#include <utility>
#include <variant>

namespace contur {

    /// @brief Error codes returned by kernel subsystem operations.
    enum class ErrorCode : std::int32_t {
        Ok = 0,
        OutOfMemory,
        InvalidPid,
        InvalidAddress,
        DivisionByZero,
        InvalidInstruction,
        SegmentationFault,
        DeviceError,
        ProcessNotFound,
        PermissionDenied,
        Timeout,
        DeadlockDetected,
        InvalidState,
        InvalidArgument,
        ResourceBusy,
        NotFound,
        AlreadyExists,
        BufferFull,
        BufferEmpty,
        EndOfFile,
        NotImplemented,
    };

    /// @brief Returns a human-readable name for the given error code.
    [[nodiscard]] constexpr std::string_view errorCodeToString(ErrorCode code) noexcept
    {
        switch (code) {
        case ErrorCode::Ok: return "Ok";
        case ErrorCode::OutOfMemory: return "OutOfMemory";
        case ErrorCode::InvalidPid: return "InvalidPid";
        case ErrorCode::InvalidAddress: return "InvalidAddress";
        case ErrorCode::DivisionByZero: return "DivisionByZero";
        case ErrorCode::InvalidInstruction: return "InvalidInstruction";
        case ErrorCode::SegmentationFault: return "SegmentationFault";
        case ErrorCode::DeviceError: return "DeviceError";
        case ErrorCode::ProcessNotFound: return "ProcessNotFound";
        case ErrorCode::PermissionDenied: return "PermissionDenied";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::DeadlockDetected: return "DeadlockDetected";
        case ErrorCode::InvalidState: return "InvalidState";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::ResourceBusy: return "ResourceBusy";
        case ErrorCode::NotFound: return "NotFound";
        case ErrorCode::AlreadyExists: return "AlreadyExists";
        case ErrorCode::BufferFull: return "BufferFull";
        case ErrorCode::BufferEmpty: return "BufferEmpty";
        case ErrorCode::EndOfFile: return "EndOfFile";
        case ErrorCode::NotImplemented: return "NotImplemented";
        }
        return "Unknown";
    }

    /// @brief A result type that holds either a success value of type T or an ErrorCode.
    ///
    /// Inspired by Rust's Result<T, E>. Use the static factory methods `ok()` and
    /// `error()` to construct instances. Check `isOk()` / `isError()` before accessing
    /// the contained value or error code.
    ///
    /// @tparam T The success value type.
    template <typename T>
    class [[nodiscard]] Result
    {
    public:
        /// @brief Constructs a successful Result containing the given value.
        [[nodiscard]] static Result ok(T value)
        {
            return Result(std::move(value));
        }

        /// @brief Constructs a failed Result with the given error code.
        [[nodiscard]] static Result error(ErrorCode code)
        {
            assert(code != ErrorCode::Ok && "Cannot create error Result with Ok code");
            return Result(code);
        }

        /// @brief Returns true if this Result holds a success value.
        [[nodiscard]] bool isOk() const noexcept
        {
            return std::holds_alternative<T>(storage_);
        }

        /// @brief Returns true if this Result holds an error code.
        [[nodiscard]] bool isError() const noexcept
        {
            return std::holds_alternative<ErrorCode>(storage_);
        }

        /// @brief Returns a const reference to the success value.
        /// @pre `isOk()` must be true. Undefined behavior otherwise.
        [[nodiscard]] const T& value() const&
        {
            assert(isOk() && "Accessing value of error Result");
            return std::get<T>(storage_);
        }

        /// @brief Returns an rvalue reference to the success value (move semantics).
        /// @pre `isOk()` must be true.
        [[nodiscard]] T&& value() &&
        {
            assert(isOk() && "Accessing value of error Result");
            return std::get<T>(std::move(storage_));
        }

        /// @brief Returns a mutable reference to the success value.
        /// @pre `isOk()` must be true.
        [[nodiscard]] T& value() &
        {
            assert(isOk() && "Accessing value of error Result");
            return std::get<T>(storage_);
        }

        /// @brief Returns the error code.
        /// @pre `isError()` must be true. Undefined behavior otherwise.
        [[nodiscard]] ErrorCode errorCode() const noexcept
        {
            assert(isError() && "Accessing errorCode of ok Result");
            return std::get<ErrorCode>(storage_);
        }

        /// @brief Returns the value if ok, or the provided default value if error.
        [[nodiscard]] T valueOr(T defaultValue) const&
        {
            if (isOk()) {
                return std::get<T>(storage_);
            }
            return defaultValue;
        }

    private:
        explicit Result(T value) : storage_(std::move(value)) {}
        explicit Result(ErrorCode code) : storage_(code) {}

        std::variant<T, ErrorCode> storage_;
    };

    /// @brief Specialization of Result for void — operations that produce no value on success.
    template <>
    class [[nodiscard]] Result<void>
    {
    public:
        /// @brief Constructs a successful void Result.
        [[nodiscard]] static Result ok()
        {
            return Result(ErrorCode::Ok);
        }

        /// @brief Constructs a failed void Result with the given error code.
        [[nodiscard]] static Result error(ErrorCode code)
        {
            assert(code != ErrorCode::Ok && "Cannot create error Result with Ok code");
            return Result(code);
        }

        /// @brief Returns true if this Result represents success.
        [[nodiscard]] bool isOk() const noexcept { return code_ == ErrorCode::Ok; }

        /// @brief Returns true if this Result represents failure.
        [[nodiscard]] bool isError() const noexcept { return code_ != ErrorCode::Ok; }

        /// @brief Returns the error code (Ok if successful).
        [[nodiscard]] ErrorCode errorCode() const noexcept { return code_; }

    private:
        explicit Result(ErrorCode code) : code_(code) {}

        ErrorCode code_;
    };

} // namespace contur
