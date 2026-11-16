@echo off
setlocal
where i686-w64-mingw32-clang++ >nul 2>nul
if errorlevel 1 (
  echo Missing llvm-mingw i686-w64-mingw32-clang++. Add its bin directory to PATH.
  exit /b 1
)
i686-w64-mingw32-clang++ -std=c++11 -O2 -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -mwindows -Wl,-e,_EntryPoint@0 -Wl,--subsystem,windows:6.01 main.cpp -o MedAobHotkey.exe -lshell32 -luser32 -lgdi32 -lkernel32
if errorlevel 1 exit /b 1
echo Built MedAobHotkey.exe
