# DeadZone Engine

Squelette : SDL3 (fenetre/entrees) + bgfx (rendu). Cible : Windows et Linux.

## Prerequis
- CMake >= 3.24, Git
- **Windows** : Visual Studio 2022 (charge de travail "Developpement Desktop C++")
- **Linux** (Debian/Ubuntu) :
  `sudo apt install build-essential cmake ninja-build git pkg-config libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libgl1-mesa-dev libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev libudev-dev`
  (Arch/CachyOS : `sudo pacman -S base-devel cmake ninja git libx11 libxext libxrandr libxcursor libxi libxfixes libxss mesa wayland libxkbcommon alsa-lib libpulse`)

## Compiler
Windows :
    cmake --preset windows
    cmake --build --preset windows-release
    build\windows\Release\DeadZone.exe

Linux :
    cmake --preset linux
    cmake --build --preset linux
    ./build/linux/DeadZone

La premiere configuration telecharge SDL3 et bgfx (plusieurs minutes, c'est normal).
