/// @file fs_utils.h
/// @brief Inline helpers for reading and writing RegisterValue through IFileSystem.
///
/// Files store values as ASCII decimal strings.  These utilities wrap the
/// open/read/write/close sequence and handle text serialization so that callers
/// do not have to deal with byte buffers or std::from_chars directly.

#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "contur/core/error.h"
#include "contur/core/types.h"

#include "contur/fs/file_descriptor.h"
#include "contur/fs/i_filesystem.h"

namespace contur {

    /// @brief Parses a decimal integer string into a RegisterValue.
    /// @param text Input string (leading/trailing whitespace is ignored).
    /// @return Parsed value on success; InvalidState if the string is not a valid integer.
    [[nodiscard]] inline Result<RegisterValue> parseRegisterValue(std::string_view text) noexcept
    {
        while (!text.empty() &&
               (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' || text.front() == '\r'))
        {
            text.remove_prefix(1);
        }
        while (!text.empty() &&
               (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' || text.back() == '\r'))
        {
            text.remove_suffix(1);
        }

        RegisterValue value = 0;
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc())
        {
            return Result<RegisterValue>::error(ErrorCode::InvalidState);
        }
        return Result<RegisterValue>::ok(value);
    }

    /// @brief Reads a RegisterValue stored as decimal text from a file.
    ///
    /// Opens @p path for reading, reads up to 64 bytes, closes the descriptor,
    /// then parses the content as a decimal integer.
    ///
    /// @param fs   File system instance that owns the path.
    /// @param path Absolute path of the file to read.
    /// @return Parsed value on success; NotFound/EndOfFile/InvalidState on failure.
    [[nodiscard]] inline Result<RegisterValue> readFileValue(IFileSystem &fs, const std::string &path)
    {
        auto openResult = fs.open(path, OpenMode::Read);
        if (openResult.isError())
        {
            return Result<RegisterValue>::error(openResult.errorCode());
        }

        std::array<std::byte, 64> buffer{};
        auto readResult = fs.read(openResult.value(), std::span<std::byte>(buffer.data(), buffer.size()));
        (void)fs.close(openResult.value());

        if (readResult.isError())
        {
            return Result<RegisterValue>::error(readResult.errorCode());
        }
        if (readResult.value() == 0)
        {
            return Result<RegisterValue>::error(ErrorCode::EndOfFile);
        }

        const std::string_view text{reinterpret_cast<const char *>(buffer.data()), readResult.value()};
        return parseRegisterValue(text);
    }

    /// @brief Writes a RegisterValue as decimal text to a file, truncating first.
    ///
    /// Opens @p path with Create|Write|Truncate, serialises @p value with
    /// std::to_string, writes the bytes, then closes the descriptor.
    ///
    /// @param fs    File system instance that owns the path.
    /// @param path  Absolute path of the file to write.
    /// @param value Value to serialise.
    /// @return Ok on success; error code on open/write/close failure.
    [[nodiscard]] inline Result<void> writeFileValue(IFileSystem &fs, const std::string &path, RegisterValue value)
    {
        auto openResult = fs.open(path, OpenMode::Create | OpenMode::Write | OpenMode::Truncate);
        if (openResult.isError())
        {
            return Result<void>::error(openResult.errorCode());
        }

        const std::string text = std::to_string(value);
        std::vector<std::byte> bytes(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            bytes[i] = static_cast<std::byte>(text[i]);
        }

        auto writeResult = fs.write(openResult.value(), std::span<const std::byte>(bytes.data(), bytes.size()));
        (void)fs.close(openResult.value());

        if (writeResult.isError())
        {
            return Result<void>::error(writeResult.errorCode());
        }
        return Result<void>::ok();
    }

} // namespace contur
