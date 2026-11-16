#pragma once

/// @file
/// @brief Safe, bounded loading of `main.phil` (plan 0001 §3.2, D21, D32, D47).
///
/// This header includes no POSIX header and mentions no file descriptor or POSIX type. All POSIX
/// calls live only in `src/cli/source_file.cpp` (checked for headers by T-BUILD-6).

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <system_error>

namespace philios {

/// @brief Maximum accepted source size in bytes (2 MiB, not configurable, D21).
inline constexpr std::size_t kMaxSourceBytes = 2'097'152;

/// @brief Result of one chunk-read operation.
///
/// No error and 0 bytes means end of file. An error equal to `std::errc::interrupted` means
/// "retry". Any other error means failure. A result holding an error must report 0 bytes; an error
/// with a nonzero byte count is an internal error in readBoundedSource. Never reports through the
/// global `errno`.
struct ChunkReadResult {
    std::size_t byteCount{0}; ///< Bytes written to the front of the destination.
    std::error_code error{};  ///< Empty on success.
};

/// @brief One bounded read into @p destination (the read seam, §3.2).
///
/// The shipped binary passes only the POSIX reader defined in `source_file.cpp`; unit tests pass
/// fakes defined in their own anonymous namespace (D44).
using ChunkReader = std::function<ChunkReadResult(std::span<char> destination)>;

/// @brief The bounded chunk-reading loop: the only place that enforces kMaxSourceBytes, retries
///        on `std::errc::interrupted`, and discards partial input on failure.
///
/// @param readChunk The read operation; borrowed for the duration of the call and never stored.
/// @param sizeHint Used only to reserve capacity, and only when `0 < sizeHint <= kMaxSourceBytes`;
///                 0 means no hint. Never trusted as the bound.
/// @pre @p readChunk is callable.
/// @return Every byte read before end of file.
/// @post The returned string's size is at most kMaxSourceBytes. The internal buffer never grows
///       beyond kMaxSourceBytes; an oversized file is detected by successfully reading any byte
///       past the limit.
/// @throws StartupError if @p readChunk reports an error other than `interrupted`, or the input is
///         larger than kMaxSourceBytes. InternalError if @p readChunk reports more bytes than its
///         destination holds, or an error together with a nonzero byte count. std::bad_alloc on
///         exhaustion. On any throw no partial buffer is returned or kept (strong guarantee).
///         The retry on `interrupted` is deliberately unbounded (§3.2).
[[nodiscard]] std::string readBoundedSource(const ChunkReader& readChunk, std::size_t sizeHint);

/// @brief Opens `main.phil` in the current directory, checks it, and reads it.
///
/// Sequence (D32 as amended by D47): `open("main.phil", O_RDONLY | O_NOFOLLOW | O_NONBLOCK |
/// O_CLOEXEC | O_NOCTTY)`, `fstat` on the opened descriptor (must be a regular file), bounded
/// `read` through readBoundedSource with `EINTR` retried, then `close`. A non-copyable,
/// non-movable RAII descriptor (file-local) closes the descriptor on every path. Symlinks,
/// directories, FIFOs, sockets, and devices are rejected; opening never blocks on a FIFO and a
/// terminal device cannot become the controlling terminal.
/// @return The file's bytes; ownership passes to the caller, which moves them into the Module
///         before creating any view into them (§3.9).
/// @throws StartupError on any open, inspect, size, or read failure (exit 66).
///         InternalError on a reader invariant failure. std::bad_alloc on exhaustion.
[[nodiscard]] std::string loadMainSource();

} // namespace philios
