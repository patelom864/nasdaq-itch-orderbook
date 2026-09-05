#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

namespace itch {
namespace detail {

inline std::uint16_t byteswap(std::uint16_t value) noexcept { return __builtin_bswap16(value); }
inline std::uint32_t byteswap(std::uint32_t value) noexcept { return __builtin_bswap32(value); }
inline std::uint64_t byteswap(std::uint64_t value) noexcept { return __builtin_bswap64(value); }

// memcpy, not a reinterpret_cast: ITCH fields are not aligned to their width, so
// a cast would be undefined behavior and, on some targets, a fault. GCC and Clang
// lower this pair to a single unaligned load plus one bswap instruction.
template <class T>
T load_big_endian(const std::byte* source) noexcept {
    static_assert(std::is_unsigned_v<T>);
    T value{};
    std::memcpy(&value, source, sizeof(T));
    return byteswap(value);
}

}  // namespace detail

// A read-only view of one ITCH record, with big-endian field accessors.
//
// The offsets passed to these accessors come from the specification and are
// fixed at compile time by the decoders in messages.hpp. They are internal
// invariants of this library rather than properties of the input, because the
// framer has already checked that the record's length matches what the
// specification says the type is. That is why an out-of-range offset is an
// assert and not an exception: if it fires, a decoder has the wrong offset.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> record) noexcept : record_(record) {}

    std::size_t size() const noexcept { return record_.size(); }

    std::uint8_t read_u8(std::size_t offset) const noexcept {
        assert(offset + 1 <= record_.size());
        return std::to_integer<std::uint8_t>(record_[offset]);
    }

    char read_char(std::size_t offset) const noexcept {
        return static_cast<char>(read_u8(offset));
    }

    std::uint16_t read_u16(std::size_t offset) const noexcept {
        assert(offset + 2 <= record_.size());
        return detail::load_big_endian<std::uint16_t>(record_.data() + offset);
    }

    std::uint32_t read_u32(std::size_t offset) const noexcept {
        assert(offset + 4 <= record_.size());
        return detail::load_big_endian<std::uint32_t>(record_.data() + offset);
    }

    std::uint64_t read_u64(std::size_t offset) const noexcept {
        assert(offset + 8 <= record_.size());
        return detail::load_big_endian<std::uint64_t>(record_.data() + offset);
    }

    // ITCH timestamps are 6 bytes: nanoseconds since midnight, US/Eastern.
    // Widened into the low 48 bits rather than read as a truncated u64, which
    // would read two bytes past the field.
    std::uint64_t read_u48(std::size_t offset) const noexcept {
        assert(offset + 6 <= record_.size());
        std::array<std::byte, 8> widened{};
        std::memcpy(widened.data() + 2, record_.data() + offset, 6);
        return detail::load_big_endian<std::uint64_t>(widened.data());
    }

    // Fixed-width, space-padded ASCII: stock symbols (8) and MPIDs (4).
    // Returned by value so no decoded message aliases the mapped file.
    template <std::size_t N>
    std::array<char, N> read_alpha(std::size_t offset) const noexcept {
        assert(offset + N <= record_.size());
        std::array<char, N> field{};
        std::memcpy(field.data(), record_.data() + offset, N);
        return field;
    }

private:
    std::span<const std::byte> record_;
};

}  // namespace itch