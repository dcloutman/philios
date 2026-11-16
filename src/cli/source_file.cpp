#include "cli/source_file.hpp"

#include "common/errors.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// This is the only file under src/ that may include <fcntl.h>, <sys/stat.h>, or <unistd.h> and
// call open, fstat, read, or close (D32, D45, T-BUILD-6). It must never include <sys/syscall.h>.

namespace philios {
namespace {

[[nodiscard]] StartupError startupFromErrno(const char* operation) {
    const std::error_code error(errno, std::generic_category());
    return StartupError(std::string(operation) + " failed: " + error.message());
}

[[nodiscard]] StartupError startupFromErrorCode(const char* operation, std::error_code error) {
    return StartupError(std::string(operation) + " failed: " + error.message());
}

class Descriptor final {
public:
    explicit Descriptor(int descriptor) noexcept : descriptor_(descriptor) {}

    ~Descriptor() {
        if (descriptor_ >= 0) {
            [[maybe_unused]] const int result = ::close(descriptor_);
        }
    }

    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) = delete;
    Descriptor& operator=(Descriptor&&) = delete;

    [[nodiscard]] int get() const noexcept {
        return descriptor_;
    }

    void closeChecked() {
        if (descriptor_ < 0) {
            return;
        }
        if (::close(descriptor_) != 0) {
            descriptor_ = -1;
            throw startupFromErrno("close");
        }
        descriptor_ = -1;
    }

private:
    int descriptor_{-1};
};

} // namespace

std::string readBoundedSource(const ChunkReader& readChunk, std::size_t sizeHint) {
    constexpr std::size_t kChunkSize = 8192;

    std::string source;
    if (sizeHint > 0 && sizeHint <= kMaxSourceBytes) {
        source.reserve(sizeHint);
    }

    std::array<char, kChunkSize> chunk{};
    while (true) {
        const std::size_t remaining = kMaxSourceBytes - source.size();
        const std::size_t limit = remaining == 0 ? 1 : std::min(chunk.size(), remaining);
        std::span<char> destination(chunk.data(), limit);

        const ChunkReadResult result = readChunk(destination);
        if (result.byteCount > destination.size()) {
            throw InternalError("chunk reader reported a byte count larger than destination");
        }
        if (result.error) {
            if (result.byteCount != 0) {
                throw InternalError("chunk reader reported an error with a nonzero byte count");
            }
            if (result.error == std::errc::interrupted) {
                continue;
            }
            throw startupFromErrorCode("read", result.error);
        }
        if (result.byteCount == 0) {
            return source;
        }
        if (remaining == 0) {
            throw StartupError("source file exceeds 2097152 bytes");
        }

        source.append(chunk.data(), result.byteCount);
    }
}

std::string loadMainSource() {
    constexpr int kOpenFlags = O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC | O_NOCTTY;

    const int descriptor = ::open("main.phil", kOpenFlags);
    if (descriptor < 0) {
        throw startupFromErrno("open");
    }
    Descriptor file(descriptor);

    struct stat info {};
    if (::fstat(file.get(), &info) != 0) {
        throw startupFromErrno("fstat");
    }
    if (!S_ISREG(info.st_mode)) {
        throw StartupError("main.phil is not a regular file");
    }

    std::size_t sizeHint = 0;
    if (info.st_size > 0) {
        const auto rawSize = static_cast<std::uintmax_t>(info.st_size);
        if (rawSize <= static_cast<std::uintmax_t>(kMaxSourceBytes)) {
            sizeHint = static_cast<std::size_t>(rawSize);
        }
    }

    const ChunkReader reader = [&file](std::span<char> destination) {
        const ssize_t result = ::read(file.get(), destination.data(), destination.size());
        if (result < 0) {
            return ChunkReadResult{
                0,
                std::error_code(errno, std::generic_category()),
            };
        }
        return ChunkReadResult{static_cast<std::size_t>(result), {}};
    };

    std::string source = readBoundedSource(reader, sizeHint);
    file.closeChecked();
    return source;
}

} // namespace philios
