#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace testing_support {

// Builds ITCH wire bytes for tests.
//
// Deliberately does NOT use itch::detail::byteswap. Big-endian bytes are emitted
// most-significant first by explicit shifts, so these fixtures remain a genuinely
// independent statement of the wire format and can catch a byte-order regression
// in the production reader.
class RecordBuilder {
public:
    RecordBuilder& u8(std::uint8_t value) {
        bytes_.push_back(static_cast<std::byte>(value));
        return *this;
    }

    RecordBuilder& ch(char value) { return u8(static_cast<std::uint8_t>(value)); }

    RecordBuilder& big_endian(std::uint64_t value, std::size_t width) {
        for (std::size_t i = width; i-- > 0;) {
            u8(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
        }
        return *this;
    }

    RecordBuilder& u16(std::uint16_t value) { return big_endian(value, 2); }
    RecordBuilder& u32(std::uint32_t value) { return big_endian(value, 4); }
    RecordBuilder& u48(std::uint64_t value) { return big_endian(value, 6); }
    RecordBuilder& u64(std::uint64_t value) { return big_endian(value, 8); }

    // Space-padded to `width`, truncated if longer.
    RecordBuilder& alpha(std::string_view text, std::size_t width) {
        for (std::size_t i = 0; i < width; ++i) {
            ch(i < text.size() ? text[i] : ' ');
        }
        return *this;
    }

    // The message body, without the BinaryFILE length prefix.
    const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

    // The message with its 2-byte big-endian BinaryFILE length prefix.
    std::vector<std::byte> framed() const {
        std::vector<std::byte> out;
        out.reserve(bytes_.size() + 2);
        const auto length = static_cast<std::uint16_t>(bytes_.size());
        out.push_back(static_cast<std::byte>((length >> 8) & 0xFF));
        out.push_back(static_cast<std::byte>(length & 0xFF));
        out.insert(out.end(), bytes_.begin(), bytes_.end());
        return out;
    }

private:
    std::vector<std::byte> bytes_;
};

// Convenience: the 11-byte header common to every ITCH message.
inline RecordBuilder header(char type, std::uint16_t stock_locate,
                            std::uint16_t tracking_number, std::uint64_t timestamp_ns) {
    RecordBuilder builder;
    builder.ch(type).u16(stock_locate).u16(tracking_number).u48(timestamp_ns);
    return builder;
}

}  // namespace testing_support