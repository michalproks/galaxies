#include "CommandLine.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("Command line parser enables the one-frame smoke test") {
    std::array<std::wstring_view, 1> const arguments{L"--smoke-test"};

    Main::CommandLineParseResult const result = Main::parseCommandLine(arguments);

    CHECK(result.isValid());
    CHECK(result.options.smokeTest);
}

TEST_CASE("Command line parser rejects unsupported arguments") {
    std::array<std::wstring_view, 1> const arguments{L"--unknown"};

    Main::CommandLineParseResult const result = Main::parseCommandLine(arguments);

    CHECK(result.isValid() == false);
    CHECK(result.error == L"Unknown argument: --unknown");
}

TEST_CASE("Command line parser defaults to normal game mode") {
    Main::CommandLineParseResult const result = Main::parseCommandLine({});

    CHECK(result.isValid());
    CHECK(result.options.smokeTest == false);
}
