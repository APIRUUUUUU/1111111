@echo off
setlocal

REM Crystal Voice VST3 build helper for Windows.
REM Required once: Visual Studio 2022 Community or Build Tools with
REM "Desktop development with C++", CMake 3.22+, Git, and an Internet connection.

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake was not found in PATH.
  echo Install CMake, restart the terminal, then run this file again.
  pause
  exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Git was not found in PATH.
  echo Install Git for Windows, restart the terminal, then run this file again.
  pause
  exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :failed

cmake --build build --config Release --target CrystalVoice_VST3
if errorlevel 1 goto :failed

echo.
echo [DONE] Build completed.
echo The VST3 bundle should be under:
echo build\CrystalVoice_artefacts\Release\VST3\Crystal Voice.vst3
echo.
echo Copy the entire .vst3 folder to:
echo C:\Program Files\Common Files\VST3\
echo Then open FL Studio ^> Options ^> Manage plugins ^> Find installed plugins.
pause
exit /b 0

:failed
echo.
echo [FAILED] See the first error above. The README has troubleshooting notes.
pause
exit /b 1
