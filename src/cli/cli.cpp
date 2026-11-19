#include "cli/cli.hpp"

#include "cli/exit_status.hpp"
#include "cli/source_file.hpp"
#include "cli/standard_output.hpp"
#include "common/errors.hpp"
#include "runtime/evaluator/execution_context.hpp"
#include "runtime/evaluator/interpreter.hpp"
#include "runtime/evaluator/module.hpp"
#include "runtime/evaluator/script_output.hpp"

#include <cstdio>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace philios {
namespace {

void writeLine(std::string_view line) {
    [[maybe_unused]] const std::size_t written =
        std::fwrite(line.data(), sizeof(char), line.size(), stderr);
    [[maybe_unused]] const int newlineResult = std::fputc(10, stderr);
}

[[nodiscard]] int statusForPhiliosError(const PhiliosError& error) noexcept {
    switch (error.errorClass()) {
    case ErrorClass::Lexical:
    case ErrorClass::Syntax:
    case ErrorClass::Declaration:
        return kExitDataError;
    case ErrorClass::Runtime:
        return kExitSoftware;
    }
    return kExitSoftware;
}

} // namespace

int runTopLevel(const std::function<int()>& body) {
    try {
        return body();
    } catch (const PhiliosError& error) {
        writeLine(formatDiagnostic(error));
        return statusForPhiliosError(error);
    } catch (const StartupError& error) {
        writeLine(formatDiagnostic(error));
        return kExitNoInput;
    } catch (const InternalError& error) {
        writeLine(formatDiagnostic(error));
        return kExitSoftware;
    } catch (const std::bad_alloc&) {
        writeLine("philios: internal error: out of memory");
        return kExitSoftware;
    } catch (const std::exception&) {
        writeLine("philios: internal error: unexpected exception");
        return kExitSoftware;
    } catch (...) {
        writeLine("philios: internal error: unexpected exception");
        return kExitSoftware;
    }
}

int runCli(int argc) {
    if (argc > 1) {
        writeLine("philios: usage error: philios takes no arguments");
        writeLine("usage: philios");
        return kExitUsage;
    }
    return runProject();
}

int runProject() {
    ignoreBrokenPipeSignal();
    ExecutionContext context{OutputWriter{writeStandardOutput}};
    std::string source = loadMainSource();
    const std::unique_ptr<Module> module = Module::load(context, std::move(source));
    return runMain(context, *module);
}

} // namespace philios
