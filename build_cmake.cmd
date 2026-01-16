@echo off
REM ------------------------------------
REM PORTLESS 2026, miskaa, MIT License
REM build_cmake.cmd
REM ------------------------------------
echo PORTLESS 2026, miskaa@portless.zip, MIT License
setlocal enabledelayedexpansion
set BUILD_DIR=build
if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)
cd %BUILD_DIR%
cmake ..
REM cmake -S . -B build
cmake --build . --config Release
cd ..
endlocal