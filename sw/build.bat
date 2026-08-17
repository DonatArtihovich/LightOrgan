cd build
rm -rf *
cmake .. -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic \
  -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_MAKE_PROGRAM=mingw32-make

cmake --build .
