#pragma once

/// @file
/// @brief Test-only capture of the process's stderr (descriptor 2), shared by the CLI unit tests
///        that check top-level reports (T-SEC-8, T-CLI-8; plan 0001 §7.1, D44).
///
/// Captures at the descriptor level, so it sees output written through `std::cerr`, `stderr`, or
/// the descriptor directly, independent of how runTopLevel writes. Test code only: POSIX calls
/// here are outside `src/` (T-BUILD-6 scans only `src/`).

#include <cstdio>
#include <memory>
#include <string>

namespace philios::test {

/// @brief Redirects descriptor 2 to an anonymous temporary file for its lifetime.
///
/// Not copyable or movable. Single-threaded use only.
class StderrCapture final {
public:
    /// @post Descriptor 2 refers to the capture file.
    /// @throws std::runtime_error if the redirection cannot be set up (nothing is changed then).
    StderrCapture();

    /// @brief Flushes and restores the original descriptor 2.
    /// @throws Nothing.
    ~StderrCapture();

    StderrCapture(const StderrCapture&) = delete;
    StderrCapture& operator=(const StderrCapture&) = delete;
    StderrCapture(StderrCapture&&) = delete;
    StderrCapture& operator=(StderrCapture&&) = delete;

    /// @brief Everything written to descriptor 2 since construction.
    /// @throws std::runtime_error on a read failure; std::bad_alloc.
    [[nodiscard]] std::string text();

private:
    struct FileCloser {
        void operator()(std::FILE* file) const noexcept;
    };

    std::unique_ptr<std::FILE, FileCloser> file_;
    int savedDescriptor_{-1};
};

} // namespace philios::test
