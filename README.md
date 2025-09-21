# C File System Terminal

A native Windows GUI terminal application with an in-memory filesystem, built in C using Win32 API. Features complete Git integration, file management, multi-user system, and a sophisticated trash system for safe file operations.

## Features

- **Native GUI Window** - No console window, clean terminal interface
- **In-Memory Filesystem** - Create directories and files within the terminal
- **Advanced File Operations** - Write, append, read, and manage files with full persistence
- **Multi-User System** - Create custom users and switch between them dynamically
- **File Persistence** - Save entire filesystem to disk with automatic sync
- **Blinking Cursor** - White vertical line cursor that blinks every 500ms
- **Green Text** - Classic terminal appearance with green text on black background
- **No Scroll Bars** - Clean interface without scroll bars
- **Complete Git Integration** - Full Git workflow with 25+ commands and SSH support
- **Trash System** - Soft delete with restore capabilities
- **Real Filesystem Sync** - Files appear in both terminal and File Explorer
- **Interactive Code Editor** - Built-in code editor with syntax highlighting
- **Advanced Text Writing** - Support for line breaks, code formatting, and multi-line text

## Project Structure

```
C_DIRECTORY/
├── src/
│   └── simple_gui_terminal.c    # Source code
├── data/
│   ├── filesystem.dat           # Virtual filesystem data
│   └── USERS/                   # User profiles directory
│       ├── Public/              # Public user directory
│       ├── Admin/               # Admin user directory
│       └── [Custom Users]/      # Dynamically created users
├── build/
│   └── terminal.exe             # Compiled executable
├── build.bat                    # Build script
└── README.md                    # This file
```

## Build Instructions

### Quick Build (Recommended):
```bash
# Run the build script
build.bat
```

### Manual Build:
```bash
# Using MinGW
gcc src/simple_gui_terminal.c -mwindows -lgdi32 -o build/terminal.exe

# Using MSVC
cl src/simple_gui_terminal.c /link /SUBSYSTEM:WINDOWS gdi32.lib /OUT:build/terminal.exe
```

## Running the Application

1. **Double-click** `build/terminal.exe` in Windows Explorer
2. **Or run from Command Prompt/PowerShell:**
   ```cmd
   build/terminal.exe
   ```
3. **Or run from the project directory:**
   ```cmd
   cd C_DIRECTORY
   build/terminal.exe
   ```

## Available Commands

### File System Commands
- `DIR` / `LS` - List directory contents
- `CD <folder>` - Change directory (supports `..` and `~`)
- `MKDIR <name>` / `MD <name>` - Create directory
- `PWD` - Print working directory

### File Operations
- `TOUCH <name>` - Create empty file
- `WRITE <file> <text>` - Write text to file (overwrites existing content)
- `WRITELN <file> <text>` - Write text with line breaks (use `\n` for newlines)
- `WRITECODE <file> <code>` - Write code with formatting (use `\n` for newlines, `\t` for tabs)
- `EDITCODE <file>` - Interactive code editor (Ctrl+S to save, Ctrl+C to cancel)
- `APPEND <file> <text>` - Append text to existing file
- `TYPE <file>` / `CAT <file>` - Display file contents

### File Deletion Commands
- `DEL <name>` / `DELETE <name>` - Permanently delete file (affects both terminal and File Explorer)
- `RMDIR <name>` / `RD <name>` - Permanently delete empty directory (affects both terminal and File Explorer)
- `SOFTDEL <name>` - Delete file/folder from both terminal and File Explorer
- `SOFTDEL /S <name>` - Soft delete file/folder (terminal only, preserves in File Explorer)
- `RESTORE <name>` - Restore file/folder from trash
- `TRASH` - Show trash contents
- `EMPTYTRASH` - Permanently delete all items in trash

### User Management Commands
- `ADDUSER <name>` - Create new user with custom name
- `USER <name>` - Switch to different user
- `WHOAMI` - Show current user
- `USERS` - List all available users

### System Commands
- `SAVE` - Save filesystem to disk
- `SYNC` - Sync virtual filesystem with real filesystem
- `FILEVIEW` - Show files in filesystem tree structure
- `ECHO <text>` - Print text to terminal

### Git Commands (Full Implementation)
- `GIT INIT` - Initialize a new Git repository
- `GIT CLONE <url>` - Clone a repository from URL (supports both HTTPS and SSH)
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
- `GIT RM <file>` - Remove file from Git tracking
- `GIT RM -r <folder>` - Remove folder from Git tracking
- `GIT CLEAN -f` - Remove untracked files
- `GIT CLEAN -fd` - Remove untracked files and directories
- `GIT STASH` - Stash current changes
- `GIT STASH POP` - Apply stashed changes
- `GIT TAG <name>` - Create a tag
- `GIT TAG -l` - List tags
- `GIT CONFIG --global user.name <name>` - Set global username
- `GIT CONFIG --global user.email <email>` - Set global email
- `GIT CONFIG --list` - List Git configuration
- `GIT HELP <command>` - Show help for Git command
- `GIT VERSION` - Show Git version
- `GIT TEST` - Test Git installation

### Utility Commands
- `CLS` / `CLEAR` - Clear screen
- `HELP` - Show all available commands
- `EXIT` / `QUIT` - Close application

## Example Usage

### Basic File Operations
```bash
# Create and navigate directories
MKDIR MyProject
CD MyProject
WRITE readme.txt This is my project documentation
APPEND readme.txt Version 1.0

# Read files
TYPE readme.txt

# Save everything to disk
SAVE
```

### Advanced Text Writing
```bash
# Write text with line breaks
WRITELN script.txt Hello\nWorld\nThis is line 3

# Write code with formatting
WRITECODE main.c #include <stdio.h>\n\nint main() {\n\tprintf("Hello World");\n\treturn 0;\n}

# Interactive code editor
EDITCODE program.py
# Type your code line by line
# Press Ctrl+S to save and exit
# Press Ctrl+C to cancel
```

### File Deletion System
```bash
# Create test files
TOUCH test1.txt
TOUCH test2.txt
WRITE test1.txt "Important data"

# Delete from both terminal and File Explorer
SOFTDEL test1.txt

# Soft delete (terminal only, preserves in File Explorer)
SOFTDEL /S test2.txt

# Check what's in trash
TRASH

# Restore from trash
RESTORE test1.txt

# Check directory
DIR
```

### Multi-User System
```bash
# Create a new user
ADDUSER Developer
ADDUSER Tester

# Switch between users
USER Public
WHOAMI
MKDIR PublicFiles

USER Developer
WHOAMI
MKDIR Projects
WRITE Projects/hello.py print("Hello from Developer!")

USER Admin
WHOAMI
MKDIR AdminFiles

# List all users
USERS
```

### Git Workflow with SSH
```bash
# Set up Git identity
GIT CONFIG --global user.name "Your Name"
GIT CONFIG --global user.email "your.email@example.com"

# Create project and initialize Git
MKDIR MyProjects
CD MyProjects
GIT INIT

# Create files and commit
WRITE README.md "# My Project"
GIT ADD .
GIT COMMIT -m "Initial commit"

# Clone existing repository using SSH
GIT CLONE git@github.com:microsoft/vscode.git
CD vscode
GIT STATUS
GIT BRANCH
GIT LOG
```

## File Structure

The terminal creates a virtual filesystem structure that syncs with the real filesystem:

```
C:\USERS\
├── Public\
│   ├── Documents\
│   ├── Desktop\
│   └── Downloads\
├── Admin\
│   ├── Documents\
│   ├── Desktop\
│   ├── Downloads\
│   └── System\
├── [Custom Users]\
│   ├── Developer\
│   ├── Tester\
│   └── [Any custom name]\
├── Windows\
├── Temp\
└── Documents\
```

## Key Features

### Dual Filesystem Support
- **Virtual Filesystem**: Terminal's in-memory file system
- **Real Filesystem**: Actual Windows file system
- **Automatic Sync**: Changes appear in both terminal and File Explorer
- **Persistence**: All changes saved to disk automatically

### Advanced Text Writing
- **WRITELN**: Write text with automatic line break processing (`\n` → actual newlines)
- **WRITECODE**: Write code with formatting support (`\n` for newlines, `\t` for tabs)
- **EDITCODE**: Interactive code editor with real-time editing and Ctrl+S save

### Trash System
- **Soft Delete**: `SOFTDEL /S` removes from terminal only
- **Permanent Delete**: `SOFTDEL` removes from both terminal and File Explorer
- **Restore**: `RESTORE` brings back soft-deleted items
- **Trash Management**: `TRASH` and `EMPTYTRASH` commands

### Multi-User System
- **Dynamic User Creation**: `ADDUSER` creates users with custom names
- **User Validation**: Usernames must contain only letters, numbers, and underscores
- **Isolated Directories**: Each user has separate file space
- **Profile Persistence**: User data saved between sessions
- **Empty Start**: New users start with only README.txt (no preset directories)

### Git Integration
- **Complete Git Support**: All major Git commands implemented
- **SSH Support**: Clone and push using SSH keys (recommended)
- **HTTPS Support**: Clone and push using Personal Access Tokens
- **Real Git Operations**: Uses system Git installation
- **Isolated Repositories**: Git operations don't affect project files
- **Automatic Sync**: Git changes sync between terminal and File Explorer

## Technical Details

- **Language**: C
- **API**: Win32 API
- **Font**: Consolas (monospaced)
- **Colors**: Green text on black background
- **Cursor**: White blinking vertical line
- **Memory**: In-memory filesystem with configurable limits
- **Git Integration**: Full system Git integration with isolation
- **File Sync**: Real-time synchronization with Windows file system
- **User Management**: Dynamic user creation and management
- **Text Processing**: Advanced text formatting and code editing

## Git Authentication Setup

### SSH Keys (Recommended)
1. **Generate SSH key** (if not already done):
   ```bash
   ssh-keygen -t rsa -b 4096 -C "your-email@example.com"
   ```

2. **Add to SSH agent**:
   ```bash
   ssh-add ~/.ssh/id_rsa
   ```

3. **Add public key to GitHub**:
   - Copy: `cat ~/.ssh/id_rsa.pub`
   - Add to GitHub Settings → SSH Keys

4. **Test connection**:
   ```bash
   ssh -T git@github.com
   ```

5. **Use SSH URLs**:
   ```bash
   GIT CLONE git@github.com:username/repository.git
   ```

### Personal Access Token (Alternative)
1. **Create token** on GitHub Settings → Developer settings → Personal access tokens
2. **Configure Git**:
   ```bash
   GIT CONFIG --global user.name "Your Name"
   GIT CONFIG --global user.email "your.email@example.com"
   ```
3. **Use HTTPS URLs** with token as password when prompted

## Troubleshooting

### Git Commands Not Working
1. Ensure Git is installed on your system
2. Run `GIT TEST` to verify Git installation
3. Check Git configuration with `GIT CONFIG --list`
4. For SSH: Test with `ssh -T git@github.com`
5. For HTTPS: Use Personal Access Token as password

### Files Not Appearing in File Explorer
1. Run `SYNC` to sync virtual filesystem with real filesystem
2. Check if you're in the correct user directory
3. Use `PWD` to see current directory
4. Files are automatically synced on startup

### User Management Issues
1. Usernames can only contain letters, numbers, and underscores
2. Use `USERS` to see all available users
3. New users start with empty directories (only README.txt)
4. Use `USER <name>` to switch between users

### Trash System
- Use `TRASH` to see deleted items
- Use `RESTORE <name>` to restore specific items
- Use `EMPTYTRASH` to permanently delete all trash items
- `SOFTDEL` deletes from both terminal and File Explorer by default
- Use `SOFTDEL /S` for terminal-only deletion

## Version History

- **v3.0** - Added multi-user system, interactive code editor, advanced text writing, and improved file deletion
- **v2.5** - Added SSH Git support, cleaned up debug output, and improved user experience
- **v2.0** - Added complete Git integration, trash system, and real filesystem sync
- **v1.5** - Added multi-user system and file persistence
- **v1.0** - Basic terminal with in-memory filesystem