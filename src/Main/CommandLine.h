#ifndef GALAXIES_MAIN_COMMAND_LINE_H
#define GALAXIES_MAIN_COMMAND_LINE_H

#include <span>
#include <string>
#include <string_view>

namespace Main {

struct CommandLineOptions {
    bool smokeTest{};
};

struct CommandLineParseResult {
    CommandLineOptions options;
    std::wstring error;

    [[nodiscard]] bool isValid() const;
};

/** Parses supported arguments, which do not include the executable name. */
[[nodiscard]] CommandLineParseResult parseCommandLine(std::span<std::wstring_view const> arguments);

} // namespace Main

#endif // GALAXIES_MAIN_COMMAND_LINE_H
