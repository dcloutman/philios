#pragma once

/// @file
/// @brief Test-only capturing fake OutputWriter, shared by every unit test that constructs an
///        ExecutionContext (plan 0002 §3.6, §7 task 4, T2-ECHO-8, T2-ECHO-9).
///
/// Records every write it receives and returns a configurable result, so tests can check the
/// exact bytes passed to the writer, how many times it was called, and how callers react to a
/// failing writer. Test code only.

#include "runtime/evaluator/script_output.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace philios::test {

/// @brief A capturing fake output destination.
///
/// The recorded state is shared with every writer obtained from writer(), through a
/// `std::shared_ptr`, so a writer stays valid even if it outlives this object. Copies share the
/// same state. Single-threaded use only.
class CapturingOutput final {
public:
    /// @brief Creates an empty capture whose writers succeed (return an empty code).
    /// @throws std::bad_alloc on exhaustion.
    CapturingOutput();

    /// @brief Returns a non-empty OutputWriter that appends each call's bytes to this capture and
    ///        returns the currently configured result (see setResult).
    ///
    /// The bytes are recorded even when the configured result is a failure.
    /// @return A writer sharing this capture's state.
    /// @throws std::bad_alloc on exhaustion.
    [[nodiscard]] OutputWriter writer() const;

    /// @brief Sets the result returned by every later write.
    /// @param result Empty for success; non-empty to simulate a failed write or flush.
    /// @throws Nothing.
    void setResult(std::error_code result) noexcept;

    /// @brief All bytes received so far, concatenated in call order.
    /// @throws std::bad_alloc on exhaustion.
    [[nodiscard]] std::string bytes() const;

    /// @brief The bytes of each call, one entry per call, in call order.
    /// @throws std::bad_alloc on exhaustion.
    [[nodiscard]] std::vector<std::string> writes() const;

    /// @brief The number of times any writer from this capture has been called.
    /// @throws Nothing.
    [[nodiscard]] std::size_t callCount() const noexcept;

private:
    struct State {
        std::vector<std::string> writes;
        std::error_code result;
    };

    std::shared_ptr<State> state_;
};

} // namespace philios::test
