#pragma once

#include <cstddef>
#include <cstdint>

namespace itch {

// Bytes common to every ITCH 5.0 message: message type (1), stock locate (2),
// tracking number (2), timestamp (6). Message-specific fields begin here.
inline constexpr std::size_t kHeaderLength = 11;

// Length in bytes of a complete ITCH 5.0 message of type `message_type`,
// including its one-byte type character and excluding the two-byte length
// prefix that frames it in a BinaryFILE. Returns 0 for any character the
// specification does not define.
//
// Source: Nasdaq TotalView-ITCH 5.0, section 4 (message formats).
std::uint16_t spec_message_length(char message_type) noexcept;

}  // namespace itch