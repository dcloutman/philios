#include "support/require_value.hpp"

#include <stdexcept>

namespace philios::test {

void failMissingValue() {
    throw std::logic_error("expected a value");
}

} // namespace philios::test
