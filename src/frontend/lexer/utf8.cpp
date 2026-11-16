#include "frontend/lexer/utf8.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace philios {
namespace {

[[nodiscard]] bool isContinuation(unsigned char byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

} // namespace

std::optional<Utf8Sequence> decodeUtf8(std::string_view text, std::size_t offset) noexcept {
    if (offset >= text.size()) {
        return std::nullopt;
    }
    // Reads the byte @p distance bytes after @p offset. The bound is checked against the bytes
    // remaining after offset (offset < size here), so no index addition can wrap (security
    // review SEC-0001-04).
    const std::size_t remaining = text.size() - offset;
    const auto at = [&text, offset, remaining](std::size_t distance) -> std::optional<unsigned char> {
        if (distance >= remaining) {
            return std::nullopt;
        }
        return static_cast<unsigned char>(text[offset + distance]);
    };

    const unsigned char first = static_cast<unsigned char>(text[offset]);
    if (first <= 0x7FU) {
        return Utf8Sequence{static_cast<char32_t>(first), 1};
    }
    if (first >= 0xC2U && first <= 0xDFU) {
        const auto b1 = at(1U);
        if (!b1 || !isContinuation(*b1)) {
            return std::nullopt;
        }
        const char32_t codePoint = (static_cast<char32_t>(first & 0x1FU) << 6U) |
                                   static_cast<char32_t>(*b1 & 0x3FU);
        return Utf8Sequence{codePoint, 2};
    }
    if (first >= 0xE0U && first <= 0xEFU) {
        const auto b1 = at(1U);
        const auto b2 = at(2U);
        if (!b1 || !b2 || !isContinuation(*b1) || !isContinuation(*b2)) {
            return std::nullopt;
        }
        if (first == 0xE0U && *b1 < 0xA0U) {
            return std::nullopt;
        }
        if (first == 0xEDU && *b1 >= 0xA0U) {
            return std::nullopt;
        }
        const char32_t codePoint = (static_cast<char32_t>(first & 0x0FU) << 12U) |
                                   (static_cast<char32_t>(*b1 & 0x3FU) << 6U) |
                                   static_cast<char32_t>(*b2 & 0x3FU);
        return Utf8Sequence{codePoint, 3};
    }
    if (first >= 0xF0U && first <= 0xF4U) {
        const auto b1 = at(1U);
        const auto b2 = at(2U);
        const auto b3 = at(3U);
        if (!b1 || !b2 || !b3 || !isContinuation(*b1) || !isContinuation(*b2) ||
            !isContinuation(*b3)) {
            return std::nullopt;
        }
        if (first == 0xF0U && *b1 < 0x90U) {
            return std::nullopt;
        }
        if (first == 0xF4U && *b1 > 0x8FU) {
            return std::nullopt;
        }
        const char32_t codePoint = (static_cast<char32_t>(first & 0x07U) << 18U) |
                                   (static_cast<char32_t>(*b1 & 0x3FU) << 12U) |
                                   (static_cast<char32_t>(*b2 & 0x3FU) << 6U) |
                                   static_cast<char32_t>(*b3 & 0x3FU);
        return Utf8Sequence{codePoint, 4};
    }
    return std::nullopt;
}

bool isDisallowedControl(char32_t codePoint) noexcept {
    if (codePoint == 0x09U || codePoint == 0x0AU || codePoint == 0x0DU) {
        return false;
    }
    return (codePoint <= 0x1FU) || (codePoint == 0x7FU) ||
           (codePoint >= 0x80U && codePoint <= 0x9FU);
}

bool isBidiFormatting(char32_t codePoint) noexcept {
    return (codePoint >= 0x202AU && codePoint <= 0x202EU) ||
           (codePoint >= 0x2066U && codePoint <= 0x2069U);
}

} // namespace philios
