# Repository Setup

- **Build**: CMake; Conan 2 deps via [cmake-conan](https://github.com/conan-io/cmake-conan) — catch2/3.16.1 (testing), trompeloeil/49 (mocking), sdl/3.4.16, vulkan-headers/1.4.357.0 and vulkan-loader/1.4.357.0; tests via CTest.
- **Platform**: Windows 11 only.
- **Compiler**: MSVC, C++23; all warnings enabled and treated as errors; external-header warnings ignored.
- **Quality**: git pre-commit hooks run clang-format (changed files) and clang-tidy.
- **Executables**: `Galaxies` is a Windows GUI-subsystem app; working directory = repository root. Conan runtime DLLs are copied beside executables after build.
