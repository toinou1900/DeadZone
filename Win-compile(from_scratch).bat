rmdir /s /q build\windows
cmake --preset windows
cmake --build --preset windows-release
pause