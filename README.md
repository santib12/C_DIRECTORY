# C_DIRECTORY
a directory in C.

## Build and run the terminal (Windows)

This repo contains a minimal in-memory filesystem terminal written in C.

### Build

Using GCC (MinGW / Git for Windows):
```
gcc terminal.c -o terminal.exe
```

Using MSVC:
```
cl /nologo /O2 /EHsc terminal.c /Fe:terminal.exe
```

### Run inline (no new window)

- CMD:
```
terminal.exe
```
- PowerShell:
```
./terminal.exe
```
- Git Bash:
```
winpty ./terminal.exe
```

## Build and run the GUI terminal (own native window)

Build with MinGW (no console window):
```
gcc gui_terminal.c -mwindows -o gui_terminal.exe
```

Build with MSVC:
```
cl /nologo /O2 /DUNICODE /D_UNICODE gui_terminal.c /link /SUBSYSTEM:WINDOWS /OUT:gui_terminal.exe
```

Run by double-clicking `gui_terminal.exe` or from CMD/PowerShell.

The GUI provides an output pane and an input line. Press Enter to execute commands.

### Commands inside the terminal

- mkdir <name>
- cd <path> (supports .. and ~)
- ls
- pwd
- touch <name>
- type <name>
- echo <text>
- cls / clear
- save <file>
- load <file>
- help
- exit / quit