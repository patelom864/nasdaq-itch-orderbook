#include "itch/mapped_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace itch {

MappedFile::MappedFile(const std::filesystem::path& path) {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "open " + path.string());
    }

    struct stat info {};
    if (::fstat(fd, &info) != 0) {
        const int failure = errno;
        ::close(fd);
        throw std::system_error(failure, std::generic_category(), "fstat " + path.string());
    }
    if (info.st_size == 0) {
        ::close(fd);
        throw std::runtime_error(path.string() + " is empty");
    }

    const auto size = static_cast<std::size_t>(info.st_size);
    void* mapping = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    const int mmap_failure = errno;

    // The mapping outlives the descriptor, so close it either way.
    ::close(fd);

    if (mapping == MAP_FAILED) {
        throw std::system_error(mmap_failure, std::generic_category(), "mmap " + path.string());
    }

    data_ = static_cast<const std::byte*>(mapping);
    size_ = size;

    // Advisory only; a failure here costs throughput, not correctness.
    ::madvise(mapping, size_, MADV_SEQUENTIAL);
}

MappedFile::~MappedFile() {
    if (data_ != nullptr) {
        ::munmap(const_cast<std::byte*>(data_), size_);
    }
}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)), size_(std::exchange(other.size_, 0)) {}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        if (data_ != nullptr) {
            ::munmap(const_cast<std::byte*>(data_), size_);
        }
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}

}  // namespace itch