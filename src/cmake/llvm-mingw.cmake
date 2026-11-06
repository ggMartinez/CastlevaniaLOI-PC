# CMake toolchain for the bundled llvm-mingw (clang + lld + libc++, x86_64 Windows, UCRT).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
get_filename_component(LLVM_MINGW "${CMAKE_CURRENT_LIST_DIR}/../../tools/llvm-mingw" ABSOLUTE)
set(CMAKE_C_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang.exe")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++.exe")
set(CMAKE_RC_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-windres.exe")
set(CMAKE_AR "${LLVM_MINGW}/bin/llvm-ar.exe")
set(CMAKE_RANLIB "${LLVM_MINGW}/bin/llvm-ranlib.exe")
set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW}/x86_64-w64-mingw32")
# Ship-ready binaries: no libc++/libunwind DLLs to carry around.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static")
