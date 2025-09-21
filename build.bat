@echo off
echo Building Terminal Application...
gcc src/simple_gui_terminal.c -mwindows -lgdi32 -o build/terminal.exe
if %ERRORLEVEL% == 0 (
    echo Build successful! Executable created: build/terminal.exe
) else (
    echo Build failed! Check for errors above.
)
pause
