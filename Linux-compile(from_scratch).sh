rm -rf build/linux
cmake --preset linux
cmake --build --preset linux
read -rp "Press Enter to exit..."