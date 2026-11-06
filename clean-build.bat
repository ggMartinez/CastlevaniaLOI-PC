@echo off
rem A build from nothing, as a fresh clone gets it: deletes tools\ and output\ (the downloaded
rem tools, ps2-web-recomp's checkout, every build and what testing left in output\debug: game.dll,
rem saves, settings) and runs build.bat, which fetches and builds everything again.
rem It stops first if tools\ps2-web-recomp has changes not saved into src\patches\ps2-web-recomp.patch.
setlocal
set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\src\scripts\update-patch.ps1" -Check
if errorlevel 1 (
  echo Save them first: powershell -File src\scripts\update-patch.ps1
  echo Or, to throw them away, delete tools\ps2-web-recomp yourself.
  exit /b 1
)
for %%D in (output tools) do (
  if exist "%ROOT%\%%D" (
    echo Deleting %%D\
    rmdir /s /q "%ROOT%\%%D"
    if exist "%ROOT%\%%D" (
      echo Could not delete all of %%D\: close the launcher or anything using its files, and try again.
      exit /b 1
    )
  )
)
call "%ROOT%\build.bat"