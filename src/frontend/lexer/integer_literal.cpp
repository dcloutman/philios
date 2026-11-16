#include "frontend/lexer/integer_literal.hpp"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace philios {

std::optional<std::int64_t> parseDecimalInteger(std::string_view digits) noexcept {
    std::int64_t value = 0;
    const char* const begin = digits.data();
    const char* const end = begin + digits.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec == std::errc::result_out_of_range) {
        return std::nullopt;
    }
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

} // namespace philios
