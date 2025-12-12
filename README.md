# Xmass Tree (C++ Desktop App)

A tiny Christmas tree app written in C++. On Windows it’s a Win32 desktop GUI with blinking ornaments and falling snow. On macOS/Linux it currently builds a console “tree” animation so CI can produce binaries everywhere (GUI port welcome).

## Build (CMake)

Prereq: install CMake and a C++ toolchain (Visual Studio or MinGW) and make sure `cmake` is on your PATH.

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

## Build (No CMake, MSVC)
From a **Developer Command Prompt for VS**:
```powershell
cl /std:c++17 /EHsc /DUNICODE /D_UNICODE src\main_win32.cpp user32.lib gdi32.lib /Fe:xmass_tree.exe /link /SUBSYSTEM:WINDOWS
.\xmass_tree.exe
```

## Notes
- Resize the window; the tree, ornaments, and snow re-layout automatically.
- Animation is timer‑driven (~30 FPS).
