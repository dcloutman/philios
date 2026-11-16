// T-CLI-10: a terminal device named main.phil is rejected and never becomes the controlling
// terminal (plan 0001 §3.2, §7, §9 T-CLI-10, D47, D48; task 4).
//
// Registered as its own ctest entry through PHILIOS_TCLI10_TEST_CASE; the TEST_CASE name below
// must equal that variable's value exactly. A skip writes `T-CLI-10 SKIPPED: <reason>` to stdout,
// which the entry's SKIP_REGULAR_EXPRESSION matches (§7).
//
// Test-only `_exit` exception (§7): the forked child ends every path with `_exit`, so it never
// runs the parent's atexit handlers or LeakSanitizer's exit-time check, never flushes duplicated
// stdio buffers, and never returns or unwinds into Catch2. This exception covers only this file.
// POSIX calls here are test code; T-BUILD-6 scans only src/.

#include "cli/cli.hpp"
#include "cli/exit_status.hpp"
#include "cli/source_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace philios::test {
namespace {

// Child-to-parent protocol (step 6): the child reports only through its exit status.
constexpr int kChildPass = 10;
constexpr int kSkipSetsid = 20;
constexpr int kSkipUnshare = 21;
constexpr int kSkipIdMaps = 22;
constexpr int kSkipPrivateMounts = 23;
constexpr int kSkipBindMount = 24;
constexpr int kSkipPrecondition = 25;
constexpr int kFailDupOrChdir = 30;
constexpr int kFailStatusNot66 = 31;
constexpr int kFailTtyOpened = 32;
constexpr int kFailTtyWrongErrno = 33;
constexpr int kFailException = 34;

constexpr std::string_view kStartupPrefix = "philios: startup error: main.phil: ";

void closeQuietly(int descriptor) noexcept {
    if (descriptor >= 0) {
        [[maybe_unused]] const int result = ::close(descriptor);
    }
}

/// Owns one descriptor in the parent.
class Descriptor final {
public:
    explicit Descriptor(int descriptor) noexcept : descriptor_(descriptor) {}
    ~Descriptor() { closeQuietly(descriptor_); }

    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) = delete;
    Descriptor& operator=(Descriptor&&) = delete;

    [[nodiscard]] int get() const noexcept { return descriptor_; }
    void reset() noexcept { closeQuietly(std::exchange(descriptor_, -1)); }

private:
    int descriptor_;
};

/// A fresh mkdtemp directory that will hold main.phil; removed by the destructor.
class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        std::error_code error;
        const std::filesystem::path base = std::filesystem::temp_directory_path(error);
        if (error) {
            return;
        }
        std::string pattern = (base / "philios-tcli10-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr) {
            return;
        }
        path_ = pattern;
        mainPhilPath_ = path_ + "/main.phil";
    }

    ~TemporaryDirectory() {
        if (!path_.empty()) {
            [[maybe_unused]] const int unlinked = ::unlink(mainPhilPath_.c_str());
            [[maybe_unused]] const int removed = ::rmdir(path_.c_str());
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] const std::string& mainPhilPath() const noexcept { return mainPhilPath_; }

private:
    std::string path_;
    std::string mainPhilPath_;
};

[[nodiscard]] bool createEmptyFile(const std::string& path) {
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        return false;
    }
    closeQuietly(descriptor);
    return true;
}

/// Writes all of @p text to an existing file (used for /proc/self ID maps in the child).
[[nodiscard]] bool writeWholeFile(const char* path, std::string_view text) {
    const int descriptor = ::open(path, O_WRONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return false;
    }
    std::size_t written = 0;
    bool ok = true;
    while (written < text.size()) {
        const ssize_t result = ::write(descriptor, text.data() + written, text.size() - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            ok = false;
            break;
        }
        written += static_cast<std::size_t>(result);
    }
    closeQuietly(descriptor);
    return ok;
}

/// Step 3: main.phil opens as a character device that is a terminal.
[[nodiscard]] bool terminalPreconditionHolds() {
    const int descriptor = ::open("main.phil", O_RDONLY | O_NONBLOCK | O_NOCTTY);
    if (descriptor < 0) {
        return false;
    }
    struct stat info {};
    const bool holds =
        ::fstat(descriptor, &info) == 0 && S_ISCHR(info.st_mode) && ::isatty(descriptor) == 1;
    closeQuietly(descriptor);
    return holds;
}

/// Steps 2-5 in the child. Returns a protocol exit code; never calls Catch2.
[[nodiscard]] int runChild(int pipeRead, int pipeWrite, const std::string& directory,
                           const std::string& slavePath) {
    closeQuietly(pipeRead);
    if (::dup2(pipeWrite, STDERR_FILENO) < 0) {
        closeQuietly(pipeWrite);
        return kFailDupOrChdir;
    }
    closeQuietly(pipeWrite);

    if (::setsid() < 0) {
        return kSkipSetsid;
    }
    const uid_t userId = ::getuid();
    const gid_t groupId = ::getgid();
    if (::unshare(CLONE_NEWUSER | CLONE_NEWNS) != 0) {
        return kSkipUnshare;
    }
    if (!writeWholeFile("/proc/self/setgroups", "deny") ||
        !writeWholeFile("/proc/self/uid_map", "0 " + std::to_string(userId) + " 1") ||
        !writeWholeFile("/proc/self/gid_map", "0 " + std::to_string(groupId) + " 1")) {
        return kSkipIdMaps;
    }
    if (::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        return kSkipPrivateMounts;
    }
    if (::chdir(directory.c_str()) != 0) {
        return kFailDupOrChdir;
    }
    if (::mount(slavePath.c_str(), "main.phil", nullptr, MS_BIND, nullptr) != 0) {
        return kSkipBindMount;
    }
    if (!terminalPreconditionHolds()) {
        return kSkipPrecondition;
    }

    // Step 4.
    const int status = runTopLevel([]() -> int {
        static_cast<void>(loadMainSource());
        return 0;
    });
    if (status != kExitNoInput) {
        return kFailStatusNot66;
    }

    // Step 5.
    const int tty = ::open("/dev/tty", O_RDONLY | O_NOCTTY);
    const int openError = errno;
    if (tty >= 0) {
        closeQuietly(tty);
        return kFailTtyOpened;
    }
    if (openError != ENXIO) {
        return kFailTtyWrongErrno;
    }
    return kChildPass;
}

[[nodiscard]] std::optional<std::string_view> skipReason(int code) {
    switch (code) {
    case kSkipSetsid:
        return "setsid failed in the child";
    case kSkipUnshare:
        return "unshare(CLONE_NEWUSER | CLONE_NEWNS) failed (unprivileged user namespaces "
               "unavailable?)";
    case kSkipIdMaps:
        return "writing the user or group ID map failed";
    case kSkipPrivateMounts:
        return "making mounts private failed";
    case kSkipBindMount:
        return "bind-mounting the pseudo-terminal slave onto main.phil failed";
    case kSkipPrecondition:
        return "main.phil precondition check (character device and isatty) failed";
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::string failureDescription(int code) {
    switch (code) {
    case kFailDupOrChdir:
        return "dup2 or changing into the temporary directory failed";
    case kFailStatusNot66:
        return "runTopLevel did not return 66";
    case kFailTtyOpened:
        return "/dev/tty opened: the process gained a controlling terminal";
    case kFailTtyWrongErrno:
        return "/dev/tty failed with an error other than ENXIO";
    case kFailException:
        return "an exception was caught in the child";
    default:
        return "unexpected child exit status " + std::to_string(code);
    }
}

void reportSkip(std::string_view reason) {
    std::cout << "T-CLI-10 SKIPPED: " << reason << std::endl;
}

/// Reads @p descriptor to end of file (retrying EINTR).
[[nodiscard]] std::string readToEnd(int descriptor) {
    std::string text;
    std::array<char, 4096> buffer{};
    for (;;) {
        const ssize_t result = ::read(descriptor, buffer.data(), buffer.size());
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (result == 0) {
            break;
        }
        text.append(buffer.data(), static_cast<std::size_t>(result));
    }
    return text;
}

} // namespace

TEST_CASE("T-CLI-10 terminal device main.phil") {
    // Step 1 (parent). mkdtemp, main.phil, and pipe failures are test failures.
    TemporaryDirectory directory;
    REQUIRE_FALSE(directory.path().empty());
    REQUIRE(createEmptyFile(directory.mainPhilPath()));

    std::array<int, 2> pipeEnds{-1, -1};
    REQUIRE(::pipe(pipeEnds.data()) == 0);
    Descriptor readEnd(pipeEnds[0]);
    Descriptor writeEnd(pipeEnds[1]);

    Descriptor master(::posix_openpt(O_RDWR | O_NOCTTY));
    if (master.get() < 0) {
        reportSkip("posix_openpt failed");
        return;
    }
    if (::grantpt(master.get()) != 0) {
        reportSkip("grantpt failed");
        return;
    }
    if (::unlockpt(master.get()) != 0) {
        reportSkip("unlockpt failed");
        return;
    }
    const char* slaveName = ::ptsname(master.get());
    if (slaveName == nullptr) {
        reportSkip("ptsname failed");
        return;
    }
    const std::string slavePath(slaveName);

    // Step 2.
    const pid_t child = ::fork();
    if (child < 0) {
        reportSkip("fork failed");
        return;
    }
    if (child == 0) {
        int code = kFailException;
        try {
            code = runChild(readEnd.get(), writeEnd.get(), directory.path(), slavePath);
        } catch (...) {
            code = kFailException;
        }
        closeQuietly(STDERR_FILENO);
        ::_exit(code);
    }

    // Step 7 (parent): close the write end, drain the pipe, then wait.
    writeEnd.reset();
    const std::string captured = readToEnd(readEnd.get());
    int status = 0;
    pid_t waited = -1;
    do {
        waited = ::waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    REQUIRE(waited == child);

    if (WIFEXITED(status)) {
        const int code = WEXITSTATUS(status);
        if (code == kChildPass) {
            INFO("stderr did not start with the startup prefix: " + captured);
            CHECK(captured.rfind(kStartupPrefix, 0) == 0);
            return;
        }
        if (const std::optional<std::string_view> reason = skipReason(code)) {
            reportSkip(*reason);
            return;
        }
        FAIL_CHECK(failureDescription(code) + "; child stderr: " + captured);
        return;
    }
    if (WIFSIGNALED(status)) {
        FAIL_CHECK("child terminated by signal " + std::to_string(WTERMSIG(status)) +
                   "; child stderr: " + captured);
        return;
    }
    FAIL_CHECK("unexpected wait status " + std::to_string(status) + "; child stderr: " + captured);
}

} // namespace philios::test
