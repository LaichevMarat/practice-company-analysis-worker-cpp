@echo off
rem Build script for the practice project (class WORKER, C++17).
rem Requirements: g++ 9+ from MSYS2 / MinGW-w64 available in PATH.
rem Usage: build.cmd
setlocal

where g++ >nul 2>nul
if errorlevel 1 (
    echo [error] g++ not found in PATH. Install MSYS2/MinGW-w64 or use CMake.
    exit /b 1
)

if not exist build mkdir build

echo [1/2] Building worker_app.exe ...
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude ^
    src\worker.cpp src\main.cpp -o build\worker_app.exe
if errorlevel 1 exit /b 1

echo [2/2] Building worker_tests.exe ...
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude -Itests ^
    src\worker.cpp tests\test_worker.cpp -o build\worker_tests.exe
if errorlevel 1 exit /b 1

echo Build finished: build\worker_app.exe, build\worker_tests.exe
endlocal
