# Galaxies

Galaxies is a 4X space strategy game in which the player commands a single sci-fi empire.
The project currently contains the Windows/MSVC build scaffold and an application entry point.

## Requirements

- Windows 11
- Visual Studio C++ tools (MSVC)
- Git for Windows (Bash)
- CMake 3.25 or newer
- Ninja
- Conan 2
- `clang-format` and `clang-tidy` at
  `C:\Program Files\LLVM\bin\clang-format.exe` and
  `C:\Program Files\LLVM\bin\clang-tidy.exe`

Run CMake from a Visual Studio Developer PowerShell or Developer Command Prompt so `cl` is
available. CMake uses cmake-conan to install the pinned dependencies listed in `conanfile.txt`.

## Configure, build, and test

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

The application and CTest smoke test use the repository root as their working directory.

## Git hooks

Enable the repository's hooks once per clone:

```powershell
git config core.hooksPath .githooks
```

The hook checks formatting of staged C++ files and runs clang-tidy on staged C++ source files,
using the configured build's compilation database. Configure the project with CMake before
committing C++ source changes.
