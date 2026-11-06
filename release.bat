@echo off
rem Release build for the GitHub Releases page: a clean build, laid out from scratch in
rem output\release\CastlevaniaLoI and zipped as output\release\CastlevaniaLoI-<version>-win64.zip.
rem   release.bat [version]     (default version: dev)
rem The zip holds no game code or data: the launcher builds the game from the player's own disc.
setlocal
set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "VERSION=%~1"
if "%VERSION%"=="" set "VERSION=dev"
call "%ROOT%\src\scripts\compile.cmd" release || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\src\scripts\assemble.ps1" ^
  -Build "%ROOT%\output\intermediate\release" -Out "%ROOT%\output\release\CastlevaniaLoI" -Clean || exit /b 1
set "ZIP=%ROOT%\output\release\CastlevaniaLoI-%VERSION%-win64.zip"
if exist "%ZIP%" del "%ZIP%"
tar -a -c -f "%ZIP%" -C "%ROOT%\output\release" CastlevaniaLoI || exit /b 1
echo.
echo Release: output\release\CastlevaniaLoI-%VERSION%-win64.zip
