#include "CommandLine.h"

#include <Client.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>

#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace {

void showError(wchar_t const *message) {
    MessageBoxW(nullptr, message, L"Galaxies", MB_OK | MB_ICONERROR);
}

void showError(std::string const &message) {
    MessageBoxA(nullptr, message.c_str(), "Galaxies", MB_OK | MB_ICONERROR);
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argumentCount{};
    wchar_t **rawArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (rawArguments == nullptr) {
        showError(L"Could not read the command line.");
        return 1;
    }

    std::vector<std::wstring_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argumentCount > 0 ? argumentCount - 1 : 0));
    for (int index = 1; index < argumentCount; ++index) {
        arguments.emplace_back(rawArguments[index]);
    }
    LocalFree(reinterpret_cast<HLOCAL>(rawArguments));

    Main::CommandLineParseResult const parsed = Main::parseCommandLine(arguments);
    if (!parsed.isValid()) {
        showError(parsed.error.c_str());
        return 2;
    }

    try {
        return parsed.options.smokeTest ? Client::runSmokeTest() : Client::runGame();
    } catch (std::exception const &error) {
        if (parsed.options.smokeTest) {
            std::string const debugMessage = std::string("Galaxies smoke test failed: ") + error.what() + "\n";
            OutputDebugStringA(debugMessage.c_str());
            return 1;
        }
        showError(error.what());
        return 1;
    }
}
