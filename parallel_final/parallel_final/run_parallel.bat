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

:: Compile parallel.cpp with g++
echo ⚙️ Compiling parallel.cpp with g++...
g++ parallel.cpp sqlite3.o -lstdc++ -lsqlite3 -o parallel.exe
if errorlevel 1 (
    echo ❌ Compilation of parallel.cpp failed.
    pause
    exit /b 1
)

echo ✅ Compilation successful!

:: Run the program
echo ⚙️ Running the program...
parallel.exe
if errorlevel 1 (
    echo ❌ Program execution failed.
    pause
    exit /b 1
)

pause
