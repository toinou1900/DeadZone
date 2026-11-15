rm -rf build/linux
cmake --preset linux
cmake --build linux
read -rp "Press Enter to exit..."