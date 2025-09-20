# C_DIRECTORY
a directory in C.

## Build and run the GUI terminal (native window)

Build with MinGW (no console window):
```
gcc gui_terminal.c -mwindows -o gui_terminal.exe
```

Build with MSVC:
```
cl /nologo /O2 /DUNICODE /D_UNICODE gui_terminal.c /link /SUBSYSTEM:WINDOWS /OUT:gui_terminal.exe
```

Run by double-clicking `gui_terminal.exe` or from CMD/PowerShell.

The GUI provides a terminal-like window with a command line and scrollback. Press Enter to execute commands.

### Commands inside the terminal

- mkdir <name>
- cd <path> (supports .. and ~)
- ls
- pwd
- touch <name>
- type <name>
- echo <text>
- cls / clear
- help
- exit / quit