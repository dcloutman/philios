#include "cli/standard_output.hpp"

#include "common/errors.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <string_view>
#include <system_error>

namespace philios {
namespace {

[[nodiscard]] std::error_code outputErrorFromErrno(int err) noexcept {
    if (err == 0) {
        return std::make_error_code(std::errc::io_error);
    }
    return {err, std::generic_category()};
}

} // namespace

std::error_code writeStandardOutput(std::string_view bytes) noexcept {
    if (!bytes.empty()) {
        errno = 0;
        const std::size_t written = std::fwrite(bytes.data(), sizeof(char), bytes.size(), stdout);
        if (written != bytes.size()) {
            const std::error_code error = outputErrorFromErrno(errno);
            std::clearerr(stdout);
            return error;
        }
    }

    errno = 0;
    if (std::fflush(stdout) != 0) {
        const std::error_code error = outputErrorFromErrno(errno);
        std::clearerr(stdout);
        return error;
    }

    return {};
}

void ignoreBrokenPipeSignal() {
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        throw InternalError("failed to ignore SIGPIPE");
    }
}

} // namespace philios
