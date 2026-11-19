#include "support/capturing_output.hpp"

#include "runtime/evaluator/script_output.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace philios::test {

CapturingOutput::CapturingOutput() : state_(std::make_shared<State>()) {}

OutputWriter CapturingOutput::writer() const {
    return [state = state_](std::string_view bytes) -> std::error_code {
        state->writes.emplace_back(bytes);
        return state->result;
    };
}

void CapturingOutput::setResult(std::error_code result) noexcept {
    state_->result = result;
}

std::string CapturingOutput::bytes() const {
    std::string all;
    for (const std::string& write : state_->writes) {
        all += write;
    }
    return all;
}

std::vector<std::string> CapturingOutput::writes() const {
    return state_->writes;
}

std::size_t CapturingOutput::callCount() const noexcept {
    return state_->writes.size();
}

} // namespace philios::test
