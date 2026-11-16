// Unit tests for the read seam in src/cli/source_file.hpp (plan 0001 §3.2, D21, D32;
// T-CLI-8 and T-CLI-9, task 4).
//
// Every fake ChunkReader is defined in this file's anonymous namespace and calls no POSIX
// function (D44).

#include "cli/cli.hpp"
#include "cli/exit_status.hpp"
#include "cli/source_file.hpp"
#include "common/errors.hpp"
#include "support/stderr_capture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace philios::test {
namespace {

/// State shared by a fake reader's copies (std::function requires a copyable callable).
struct FakeSource {
    std::string data;
    std::size_t offset{0};
    std::size_t calls{0};
};

/// Copies the next bytes of @p source into @p destination.
[[nodiscard]] ChunkReadResult serve(FakeSource& source, std::span<char> destination) {
    const std::string_view remaining = std::string_view(source.data).substr(source.offset);
    const std::size_t count = std::min(remaining.size(), destination.size());
    [[maybe_unused]] const std::size_t copied = remaining.copy(destination.data(), count);
    source.offset += count;
    return ChunkReadResult{count, {}};
}

/// Serves @p data, then reports end of file.
[[nodiscard]] ChunkReader makeDataReader(std::string data) {
    auto source = std::make_shared<FakeSource>();
    source->data = std::move(data);
    return [source](std::span<char> destination) { return serve(*source, destination); };
}

/// Serves @p data, then reports @p error with @p reportedCount bytes.
[[nodiscard]] ChunkReader makeDataThenErrorReader(std::string data, std::errc error,
                                                  std::size_t reportedCount) {
    auto source = std::make_shared<FakeSource>();
    source->data = std::move(data);
    return [source, error, reportedCount](std::span<char> destination) {
        if (source->offset < source->data.size()) {
            return serve(*source, destination);
        }
        return ChunkReadResult{reportedCount, std::make_error_code(error)};
    };
}

/// Reports `interrupted` once, then serves @p data, then end of file.
[[nodiscard]] ChunkReader makeInterruptedOnceReader(std::string data) {
    auto source = std::make_shared<FakeSource>();
    source->data = std::move(data);
    return [source](std::span<char> destination) {
        ++source->calls;
        if (source->calls == 1) {
            return ChunkReadResult{0, std::make_error_code(std::errc::interrupted)};
        }
        return serve(*source, destination);
    };
}

/// Reports one byte more than the destination holds, without an error.
[[nodiscard]] ChunkReader makeOverReportingReader() {
    return [](std::span<char> destination) {
        return ChunkReadResult{destination.size() + 1, {}};
    };
}

/// Reports @p error together with a nonzero byte count on the first call.
[[nodiscard]] ChunkReader makeErrorWithBytesReader(std::errc error) {
    return [error](std::span<char> /*destination*/) {
        return ChunkReadResult{5, std::make_error_code(error)};
    };
}

/// Calls readBoundedSource and discards the result (for throw checks).
void readAndDiscard(const ChunkReader& reader, std::size_t sizeHint) {
    static_cast<void>(readBoundedSource(reader, sizeHint));
}

constexpr std::string_view kProgram = "function main():\n    pass\n";

constexpr std::array<std::size_t, 4> kSizeHints = {0, kMaxSourceBytes, kMaxSourceBytes + 1,
                                                   std::numeric_limits<std::size_t>::max()};

} // namespace

TEST_CASE("T-CLI-8 a read error after a partial read throws the startup error") {
    const ChunkReader reader = makeDataThenErrorReader(std::string(kProgram), std::errc::io_error, 0);
    CHECK_THROWS_AS(readAndDiscard(reader, 0), StartupError);
}

TEST_CASE("T-CLI-8 a partial-read failure is reported through runTopLevel as exit 66") {
    const ChunkReader reader = makeDataThenErrorReader(std::string(kProgram), std::errc::io_error, 0);
    StderrCapture capture;
    const int status = runTopLevel([&reader]() -> int {
        static_cast<void>(readBoundedSource(reader, 0));
        return 0;
    });
    const std::string text = capture.text();
    CHECK(status == kExitNoInput);
    CHECK(text.find("philios: startup error: main.phil: ") == 0);
}

TEST_CASE("T-CLI-8 interrupted once then success loads the whole source") {
    const ChunkReader reader = makeInterruptedOnceReader(std::string(kProgram));
    CHECK(readBoundedSource(reader, 0) == kProgram);
}

TEST_CASE("T-CLI-8 a byte count larger than the destination is an internal error") {
    const ChunkReader reader = makeOverReportingReader();
    CHECK_THROWS_AS(readAndDiscard(reader, 0), InternalError);
}

TEST_CASE("T-CLI-8 an error reported with a nonzero byte count is an internal error") {
    SECTION("io_error with 5 bytes") {
        const ChunkReader reader = makeErrorWithBytesReader(std::errc::io_error);
        CHECK_THROWS_AS(readAndDiscard(reader, 0), InternalError);
    }
    SECTION("interrupted with 5 bytes") {
        const ChunkReader reader = makeErrorWithBytesReader(std::errc::interrupted);
        CHECK_THROWS_AS(readAndDiscard(reader, 0), InternalError);
    }
}

TEST_CASE("T-CLI-9 exactly 2,097,152 bytes load with any size hint") {
    const std::string data(kMaxSourceBytes, 'x');
    for (const std::size_t hint : kSizeHints) {
        const ChunkReader reader = makeDataReader(data);
        const std::string loaded = readBoundedSource(reader, hint);
        CHECK(loaded.size() == kMaxSourceBytes);
        CHECK(loaded == data);
    }
}

TEST_CASE("T-CLI-9 2,097,153 bytes throw the startup error with any size hint") {
    const std::string data(kMaxSourceBytes + 1, 'x');
    for (const std::size_t hint : kSizeHints) {
        const ChunkReader reader = makeDataReader(data);
        CHECK_THROWS_AS(readAndDiscard(reader, hint), StartupError);
    }
}

} // namespace philios::test
