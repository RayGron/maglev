@echo off
setlocal

set "PROFILE=%~1"
if "%PROFILE%"=="" set "PROFILE=debug"

if /I not "%PROFILE%"=="debug" if /I not "%PROFILE%"=="release" (
  echo Usage: %~nx0 ^<debug^|release^>
  exit /b 1
)

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "CARGO_BIN=%USERPROFILE%\.cargo\bin\cargo.exe"

set "VS_DEV_CMD=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VS_DEV_CMD%" (
  echo Could not find VsDevCmd.bat at "%VS_DEV_CMD%".
  exit /b 1
)

if not exist "%CARGO_BIN%" (
  echo Could not find cargo.exe at "%CARGO_BIN%".
  exit /b 1
)

call "%VS_DEV_CMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 (
  echo Failed to initialize the Visual Studio build environment.
  exit /b 1
)

set "CARGO_TARGET_DIR=%ROOT%\target\windows-x64"

pushd "%ROOT%"
if /I "%PROFILE%"=="release" (
  "%CARGO_BIN%" build --release
) else (
  "%CARGO_BIN%" build
)
set "EXIT_CODE=%ERRORLEVEL%"
popd

exit /b %EXIT_CODE%
