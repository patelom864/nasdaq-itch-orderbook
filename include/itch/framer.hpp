#pragma once

#include "itch/message_table.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace itch {

// Historical NASDAQ_ITCH50 files use BinaryFILE framing: every message is
// preceded by a 2-byte big-endian length. The live multicast feed does not —
// there, one message is one UDP payload. Framing is therefore a property of the
// transport, and it lives in its own module so that changing the transport later
// touches only this file.
enum class FrameError {
    TruncatedPrefix,  // fewer than two bytes remain for a length prefix
    ZeroLength,       // a declared length of zero
    TruncatedRecord,  // the declared length runs past the end of the buffer
    UnknownType,      // type character not defined by ITCH 5.0
    LengthMismatch,   // declared length disagrees with the specification
};

const char* describe(FrameError error) noexcept;

struct FrameOutcome {
    std::size_t messages = 0;
    std::size_t bytes_consumed = 0;
    std::optional<FrameError> error;  // empty iff the buffer framed cleanly to its end
    std::size_t error_offset = 0;     // offset of the length prefix that failed
    char error_type = '\0';           // the type character involved, when one was read

    bool complete() const noexcept { return !error.has_value(); }
};

// Walks `file` as consecutive BinaryFILE records, invoking
// on_message(char type, std::span<const std::byte> record) for each. `record`
// covers the whole message including its type byte and excluding the prefix.
//
// Stops at the first framing violation and reports it. Never throws, never
// mutates `file`, and never hands the callback a record whose length disagrees
// with the specification — which is the precondition the decoders assert on.
template <class OnMessage>
FrameOutcome frame_all(std::span<const std::byte> file, OnMessage&& on_message) {
    FrameOutcome outcome;
    std::size_t offset = 0;

    while (offset < file.size()) {
        if (file.size() - offset < 2) {
            outcome.error = FrameError::TruncatedPrefix;
            outcome.error_offset = offset;
            return outcome;
        }

        const auto declared = static_cast<std::uint16_t>(
            (std::to_integer<unsigned>(file[offset]) << 8) |
            std::to_integer<unsigned>(file[offset + 1]));

        if (declared == 0) {
            outcome.error = FrameError::ZeroLength;
            outcome.error_offset = offset;
            return outcome;
        }
        if (file.size() - offset - 2 < declared) {
            outcome.error = FrameError::TruncatedRecord;
            outcome.error_offset = offset;
            return outcome;
        }

        const auto record = file.subspan(offset + 2, declared);
        const char type = static_cast<char>(std::to_integer<unsigned char>(record[0]));
        const std::uint16_t expected = spec_message_length(type);

        if (expected == 0) {
            outcome.error = FrameError::UnknownType;
            outcome.error_offset = offset;
            outcome.error_type = type;
            return outcome;
        }
        if (expected != declared) {
            outcome.error = FrameError::LengthMismatch;
            outcome.error_offset = offset;
            outcome.error_type = type;
            return outcome;
        }

        on_message(type, record);

        ++outcome.messages;
        offset += static_cast<std::size_t>(2) + declared;
        outcome.bytes_consumed = offset;
    }

    return outcome;
}

}  // namespace itch