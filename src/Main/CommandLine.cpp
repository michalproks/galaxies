#include "CommandLine.h"

namespace Main {

bool CommandLineParseResult::isValid() const {
    return error.empty();
}

CommandLineParseResult parseCommandLine(std::span<std::wstring_view const> arguments) {
    CommandLineParseResult result{};
    for (std::wstring_view argument : arguments) {
        if (argument == L"--smoke-test") {
            result.options.smokeTest = true;
        } else {
            result.error = L"Unknown argument: ";
            result.error += argument;
            return result;
        }
    }
    return result;
}

} // namespace Main
