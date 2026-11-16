#include "cli/cli.hpp"
#include "cli/exit_status.hpp"

/// @brief Process entry point (plan 0001 §3.1, §4).
///
/// Delegates the whole run to philios::runTopLevel. The last-resort `catch (...)` guarantees that
/// no exception escapes `main` (CERT ERR51) and maps it to 70 (D30).
int main(int argc, [[maybe_unused]] char* argv[]) {
    try {
        return philios::runTopLevel([argc] { return philios::runCli(argc); });
    } catch (...) {
        return philios::kExitSoftware;
    }
}
