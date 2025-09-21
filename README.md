
# C File System Terminal

A native Windows GUI terminal application with an in-memory filesystem, built in C using Win32 API.

## Features

- **Native GUI Window** - No console window, clean terminal interface
- **In-Memory Filesystem** - Create directories and files within the terminal
- **File Operations** - Write, append, read, and manage files
- **Multi-User System** - Switch between different users (Administrator, Public, Guest, Developer, Tester)
- **File Persistence** - Save entire filesystem to disk
- **Blinking Cursor** - White vertical line cursor that blinks every 500ms
- **Green Text** - Classic terminal appearance with green text on black background
- **No Scroll Bars** - Clean interface without scroll bars

## Build Instructions

### Build with MinGW (Recommended):
```bash
gcc simple_gui_terminal.c -mwindows -lgdi32 -o terminal_final.exe
```

### Build with MSVC:
```bash
cl /nologo /O2 simple_gui_terminal.c /link /SUBSYSTEM:WINDOWS gdi32.lib /OUT:terminal_final.exe
```

## Running the Application

1. **Double-click** `terminal_final.exe` in Windows Explorer
2. **Or run from Command Prompt/PowerShell:**
   ```cmd
   terminal_final.exe
   ```

## Available Commands

### File System Commands
- `DIR` / `LS` - List directory contents
- `CD <folder>` - Change directory (supports `..` and `~`)
- `MKDIR <name>` - Create directory
- `PWD` - Print working directory

### File Operations
- `TOUCH <name>` - Create empty file
- `WRITE <file> <text>` - Write text to file (overwrites existing content)
- `APPEND <file> <text>` - Append text to existing file
- `TYPE <file>` - Display file contents

### System Commands
- `SAVEFS <path>` - Save entire filesystem to disk
- `LOADFS <path>` - Load filesystem from disk
- `USER <name>` - Switch to different user
- `WHOAMI` - Show current user
- `USERS` - List all available users
- `FILEVIEW` - Show files in filesystem tree structure
- `ECHO <text>` - Print text to terminal

### Git Commands (Working Implementation)
- `GIT INIT` - Initialize a new Git repository
- `GIT CLONE <url>` - Clone a repository from URL
- `GIT ADD <file>` - Add file to staging area
- `GIT ADD .` - Add all files to staging area
- `GIT COMMIT -m <msg>` - Commit staged changes with message
- `GIT STATUS` - Show repository status
- `GIT LOG` - Show commit history
- `GIT DIFF` - Show changes between commits
- `GIT BRANCH` - List branches
- `GIT BRANCH <name>` - Create new branch
- `GIT CHECKOUT <branch>` - Switch to branch
- `GIT MERGE <branch>` - Merge branch into current
- `GIT PULL` - Pull changes from remote
- `GIT PUSH` - Push changes to remote
- `GIT REMOTE -v` - List remote repositories
- `GIT REMOTE ADD <name> <url>` - Add remote repository
- `GIT FETCH` - Fetch changes from remote
- `GIT RESET <file>` - Unstage file
- `GIT RESET --hard` - Reset to last commit
- `GIT STASH` - Stash current changes
- `GIT STASH POP` - Apply stashed changes
- `GIT TAG <name>` - Create a tag
- `GIT TAG -l` - List tags
- `GIT CONFIG --global user.name <name>` - Set global username
- `GIT CONFIG --global user.email <email>` - Set global email
- `GIT CONFIG --list` - List Git configuration
- `GIT HELP <command>` - Show help for Git command
- `GIT VERSION` - Show Git version

### Utility Commands
- `CLS` / `CLEAR` - Clear screen
- `HELP` - Show all available commands
- `EXIT` / `QUIT` - Close application

## Example Usage

```bash
# Create and navigate directories
MKDIR MyProject
CD MyProject
WRITE readme.txt This is my project documentation
APPEND readme.txt Version 1.0

# Read files
TYPE readme.txt

# Save everything to disk
SAVEFS C:\Users\YourName\Desktop\MyTerminalFiles

# Switch users and view filesystem structure
USER Administrator
USER DUMMY_DATA
FILEVIEW
WHOAMI

# Git workflow example
MKDIR MyProjects
CD MyProjects
GIT CLONE https://github.com/microsoft/vscode.git
CD vscode
GIT STATUS
GIT BRANCH
GIT LOG
```

## File Structure

The terminal creates a virtual filesystem structure:
```
C:\
├── Users\
│   ├── Administrator\
│   ├── Public\
│   ├── Guest\
│   ├── Developer\
│   ├── Tester\
│   └── DUMMY_DATA\
├── Windows\
├── Temp\
└── Documents\
```

## Git Integration Status

**Current Status**: All Git commands are fully functional and use your system's Git installation. Commands execute real Git operations with proper feedback.

**Available Git Commands**: 25+ Git commands including:
- Repository management (INIT, CLONE)
- File staging (ADD, RESET)
- Committing (COMMIT, STATUS, LOG, DIFF)
- Branching (BRANCH, CHECKOUT, MERGE)
- Remote operations (PULL, PUSH, FETCH, REMOTE)
- Advanced features (STASH, TAG, CONFIG)

## Git Workflow Examples

### **Cloning a Repository**
```bash
# 1. Create or navigate to your projects folder
MKDIR MyProjects
CD MyProjects

# 2. Clone the repository
GIT CLONE https://github.com/microsoft/vscode.git

# 3. Enter the cloned directory
CD vscode

# 4. Check the repository status
GIT STATUS
```

**Note**: The terminal uses its own virtual filesystem. Use `DIR` to see folders and `CD` to navigate. Git commands work within the terminal's current directory.

### **Git Authentication Setup**
Before using Git commands, configure your Git identity:
```bash
# Set your Git identity (required for commits)
GIT CONFIG --global user.name "Your Name"
GIT CONFIG --global user.email "your.email@example.com"

# Check your configuration
GIT CONFIG --list
```

**For GitHub access, choose one authentication method:**
- **SSH Keys** (recommended): Set up SSH keys in your GitHub account
- **Personal Access Token**: Use token as password when prompted
- **Username/Password**: Less secure, not recommended

### **Working with Git**
```bash
# Check what files have changed
GIT STATUS

# Add all files to staging
GIT ADD .

# Or add specific files
GIT ADD filename.txt

# Commit your changes
GIT COMMIT -m "Added new feature"

# Push to remote repository
GIT PUSH

# Pull latest changes
GIT PULL
```

### **Branching Workflow**
```bash
# List all branches
GIT BRANCH

# Create new branch
GIT BRANCH feature-branch

# Switch to branch
GIT CHECKOUT feature-branch

# Merge branch back to main
GIT CHECKOUT main
GIT MERGE feature-branch
```

## Technical Details

- **Language**: C
- **API**: Win32 API
- **Font**: Consolas (monospaced)
- **Colors**: Green text on black background
- **Cursor**: White blinking vertical line
- **Memory**: In-memory filesystem with configurable limits