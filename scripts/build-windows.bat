@echo off
setlocal EnableExtensions
set "BUILD_DIR=%~dp0..\build"
set "OPENSTALLER_CMAKE_ARGS="
if /I "%~1"=="win2000" (
    set "BUILD_DIR=%~dp0..\build-win2000"
    set "OPENSTALLER_CMAKE_ARGS=-DOPENSTALLER_WIN2000_COMPAT=ON -DOPENSTALLER_FORCE_C_HASH=ON"
)
cmake -S "%~dp0.." -B "%BUILD_DIR%" -G "MinGW Makefiles" %OPENSTALLER_CMAKE_ARGS%
if errorlevel 1 exit /b %errorlevel%
cmake --build "%BUILD_DIR%"
exit /b %errorlevel%
