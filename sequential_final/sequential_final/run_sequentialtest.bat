@echo off
setlocal

:: Set the name of your output executable
set EXE_NAME=sequential_app.exe

:: Compile your C++ source file
echo 🔧 Compiling Sequential_Part1_2_3.cpp...
g++ Sequential_Part1_2_3.cpp -o %EXE_NAME% -lsqlite3

:: Check if compilation succeeded
if %errorlevel% neq 0 (
    echo ❌ Compilation failed.
    exit /b %errorlevel%
)

:: Run the compiled executable
echo ▶️ Running %EXE_NAME%...
%EXE_NAME%

endlocal
pause
