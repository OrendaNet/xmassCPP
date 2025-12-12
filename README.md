# Xmass Tree (C++ Desktop App)

A tiny cross‑platform “desktop sticker” Christmas tree. It opens as a borderless, transparent, always‑on‑top overlay with blinking ornaments and falling snow on Windows, macOS, and Linux. Rendering uses GLFW + OpenGL for identical behavior everywhere.

## Build (CMake)

Prereq: install CMake and a C++ toolchain and make sure `cmake` is on your PATH. The build pulls GLFW automatically via CMake FetchContent.

### Windows
- Install Visual Studio (Desktop C++ workload) and CMake.

### macOS
- Install Xcode Command Line Tools: `xcode-select --install`
- Install CMake (e.g., Homebrew `brew install cmake`).

### Ubuntu / Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake xorg-dev libgl1-mesa-dev
```

### Visual Studio / MSVC
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\xmass_tree.exe
```

### MinGW
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
.\build\xmass_tree.exe
```

## Notes
- The overlay opens bottom‑right; drag with left mouse to move.
- Press `C` to toggle click‑through so you can interact with apps behind it.
- Press `R` to re‑randomize ornaments/snow for the current size.
- Press `Esc` or `Q` to close.
- Legacy sources `src/main_win32.cpp` and `src/main_console.cpp` are kept for reference but are not built.
