@echo off
rem Compiles the translators (eerecomp, vurecomp) and the runner/launcher with the llvm-mingw in tools\.
rem   compile.cmd debug    output\build\debug: optimized, with debug info (a .pdb beside each binary) and a
rem                        console window for the logs; with output\explore\generated (explore.ps1) it also
rem                        builds the game's code into game.dll there, for exploring and testing
rem   compile.cmd release  output\build\release: what release.bat ships, without debug info or console
setlocal
for %%i in ("%~dp0..\..") do set "ROOT=%%~fi"
set "TOOLS=%ROOT%\tools"
set "CFG=%~1"
if /i "%CFG%"=="debug" goto :cfg_ok
if /i "%CFG%"=="release" goto :cfg_ok
echo usage: compile.cmd debug^|release
exit /b 2
:cfg_ok
set "OUT=%ROOT%\output\build\%CFG%"

rem Everything external (compiler, CMake, Ninja, libraries, ps2-web-recomp) comes from tools\,
rem which fetch-tools.ps1 fills on the first build.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fetch-tools.ps1" || exit /b 1
set "PATH=%TOOLS%\cmake\bin;%TOOLS%\ninja;%PATH%"

if /i "%CFG%"=="debug" (
  set "GEN="
  if exist "%ROOT%\output\explore\generated\units.hpp" set "GEN=%ROOT%\output\explore\generated"
  set "FLAGS=-DCMAKE_BUILD_TYPE=RelWithDebInfo -DPS2_LAUNCHER_CONSOLE=ON"
  set "FLAGS_CXX=-O2 -g -gcodeview -DNDEBUG"
  set "FLAGS_LINK=-Wl,--pdb="
) else (
  set "GEN="
  set "FLAGS=-DCMAKE_BUILD_TYPE=Release -DPS2_LAUNCHER_CONSOLE=OFF"
  set "FLAGS_CXX=-O3 -DNDEBUG"
  set "FLAGS_LINK=-s"
)

cmake -S "%TOOLS%\ps2-web-recomp" -B "%OUT%" -G Ninja %FLAGS% ^
  "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=%FLAGS_CXX%" "-DCMAKE_CXX_FLAGS_RELEASE=%FLAGS_CXX%" ^
  "-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=%FLAGS_LINK%" "-DCMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO=%FLAGS_LINK%" ^
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=%FLAGS_LINK%" "-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=%FLAGS_LINK%" ^
  -DCMAKE_TOOLCHAIN_FILE="%ROOT%\src\cmake\llvm-mingw.cmake" ^
  -DPS2_WGPU_DIR="%TOOLS%\wgpu-native" ^
  -DSDL2_DIR="%TOOLS%\SDL2\x86_64-w64-mingw32\lib\cmake\SDL2" ^
  -DPS2_IMGUI_DIR="%TOOLS%\imgui" -DPS2_LAUNCHER_DIR="%ROOT%\src\launcher" ^
  -DPS2_RUNNER_NAME=CastlevaniaLoI -DPS2_GENERATED="%GEN%" || exit /b 1
cmake --build "%OUT%" || exit /b 1
