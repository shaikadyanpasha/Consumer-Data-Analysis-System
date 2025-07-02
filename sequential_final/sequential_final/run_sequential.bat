@echo off
chcp 65001 > nul

:: Compile sqlite3.c with gcc
echo ⚙️ Compiling sqlite3.c with gcc...
gcc -c sqlite3.c -o sqlite3.o
if errorlevel 1 (
    echo ❌ Compilation of sqlite3.c failed.
    pause
    exit /b 1
)

:: Compile Sequential.cpp with g++
echo ⚙️ Compiling Sequential.cpp with g++...
g++ Sequential.cpp sqlite3.o -lstdc++ -lsqlite3 -o Sequential.exe
if errorlevel 1 (
    echo ❌ Compilation of Sequential.cpp failed.
    pause
    exit /b 1
)

echo ✅ Compilation successful!

:: Run the program
echo ⚙️ Running the program...
Sequential.exe
if errorlevel 1 (
    echo ❌ Program execution failed.
    pause
    exit /b 1
)

pause
