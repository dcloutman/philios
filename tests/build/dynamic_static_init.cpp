// T-BUILD-5 fixture (plan 0001 §7, §9, D36). This file belongs to no build target; only
// tests/build/check_tidy_scope.cmake reads it. It defines one dynamically initialized
// namespace-scope object whose constructor may throw, which cert-err58-cpp must report when
// clang-tidy runs with the configuration that applies to src/.
#include <string>

namespace philios_tidy_scope_fixture {

const std::string kDynamicallyInitialized(64, 'x');

}  // namespace philios_tidy_scope_fixture
