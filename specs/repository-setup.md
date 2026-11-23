# Repository Setup

- **Build**: CMake; Conan 2 deps via [cmake-conan](https://github.com/conan-io/cmake-conan) — catch2/3.16.1 (testing), trompeloeil/49 (mocking); tests via CTest.
- **Platform**: Windows 11 only.
- **Compiler**: MSVC, C++23; all warnings enabled and treated as errors; external-header warnings ignored.
- **Quality**: git pre-commit hooks run clang-format (changed files) and clang-tidy.
- **Executables**: working directory = repository root.
