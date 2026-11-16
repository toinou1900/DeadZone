cd /home/toinou/Documents/GitHub/DeadZone/
rm -rf build/linux
cmake --preset linux
cmake --build build/linux
read -rp "Press Enter to exit..."
