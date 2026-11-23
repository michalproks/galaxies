# Galaxies

Galaxies is a 4X space strategy game in which the player commands a single sci-fi empire.
The client renders an animated star, planet, and moon system through Vulkan. The simulation and
server are separate libraries; the in-process server produces render-ready snapshots while the
client owns camera and input state.

## Requirements

- Windows 11
- Visual Studio C++ tools (MSVC)
- Git for Windows (Bash)
- CMake 3.25 or newer
- Ninja
- Conan 2
- `glslangValidator` (installed from the pinned Conan glslang tool requirement)
- `clang-format` and `clang-tidy` at
  `C:\Program Files\LLVM\bin\clang-format.exe` and
  `C:\Program Files\LLVM\bin\clang-tidy.exe`

Run CMake from a Visual Studio Developer PowerShell or Developer Command Prompt so `cl` is
available. CMake uses cmake-conan to install the pinned dependencies listed in `conanfile.txt`,
including SDL3, Vulkan headers/loader, and glslang.

## Configure, build, and test

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

The application and CTest smoke test use the repository root as their working directory.
The GUI application opens a 1280x800 window. WASD pans the view, Z/X zooms in/out, the mouse
wheel zooms around the cursor, and middle-drag pans the view.
To run the one-frame startup smoke test directly, use `Galaxies.exe --smoke-test`.

## Git hooks

Enable the repository's hooks once per clone:

```powershell
git config core.hooksPath .githooks
```

The hook checks formatting of staged C++ files and runs clang-tidy on staged C++ source files,
using the configured build's compilation database. Configure the project with CMake before
committing C++ source changes.
