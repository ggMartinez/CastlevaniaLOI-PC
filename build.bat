@echo off
rem Developer build: compiles everything with debug info and lays out a ready-to-run launcher in
rem output\debug\CastlevaniaLoI. Rebuilding keeps what testing left there (game.dll, game\, saves\,
rem logs\, settings.ini); the launcher builds game.dll again when the runner changed.
rem The launcher of this build has a console window that shows its logs; its .pdb files let a
rem debugger (Visual Studio, WinDbg) show function names and source lines.
setlocal
set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
call "%ROOT%\src\scripts\compile.cmd" debug || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\src\scripts\assemble.ps1" ^
  -Build "%ROOT%\output\intermediate\debug" -Out "%ROOT%\output\debug\CastlevaniaLoI" -Pdb || exit /b 1
echo.
echo Launcher: output\debug\CastlevaniaLoI\CastlevaniaLoI.exe
