rmdir /s /q build\windows
cmake --preset windows
cmake --build windows
pause