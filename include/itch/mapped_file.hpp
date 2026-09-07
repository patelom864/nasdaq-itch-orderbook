#pragma once

#include <cstddef>
#include <filesystem>
#include <span>

namespace itch {

// A read-only memory map of an ITCH session file.
//
// Mapped rather than read so that the Phase 3 benchmark measures parser and book
// cost against page-cache-resident memory instead of read() syscall overhead.
// MADV_SEQUENTIAL is advised so the kernel reads ahead and drops pages behind
// the cursor, which keeps a multi-gigabyte replay from evicting everything else.
//
// Move-only. The span returned by bytes() is valid for the object's lifetime.
class MappedFile {
public:
    // Throws std::system_error if the file cannot be opened, stat'd or mapped,
    // and std::runtime_error if it is empty (mmap of length zero fails, and an
    // empty ITCH file is never valid input).
    explicit MappedFile(const std::filesystem::path& path);
    ~MappedFile();

    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    std::span<const std::byte> bytes() const noexcept { return {data_, size_}; }

private:
    const std::byte* data_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace itch