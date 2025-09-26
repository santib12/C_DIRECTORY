# NEXUS TERMINAL

A powerful native Windows GUI terminal application with an in-memory filesystem, built in C using Win32 API. Features complete Git integration, file management, multi-user system, C++/Python execution, npm/React development, command history, and a sophisticated trash system for safe file operations.

## Features

- **Native GUI Window** - No console window, clean terminal interface with "NEXUS TERMINAL" branding
- **In-Memory Filesystem** - Create directories and files within the terminal
- **Advanced File Operations** - Write, append, read, and manage files with full persistence
- **Multi-User System** - Built-in Admin and Public users plus dynamic user creation with secure authentication
- **File Persistence** - Save entire filesystem to disk with automatic sync
- **Command History** - Navigate through previous commands with up/down arrow keys (like CMD/GitBash)
- **C++ & Python Execution** - Compile and run C++ files, execute Python scripts directly
- **npm/React Development** - Full npm package management and React development support
- **Project Detection** - Automatically detects Node.js, React, Vue, Angular, C++, Python projects
- **Case-Insensitive Commands** - Type commands in any case (uppercase, lowercase, mixed)
- **Real Filesystem Sync** - Files appear in both terminal and File Explorer
- **Complete Git Integration** - Full Git workflow with 25+ commands and SSH support
- **Trash System** - Soft delete with restore capabilities
- **Interactive Code Editor** - Built-in code editor with syntax highlighting
- **Advanced Text Writing** - Support for line breaks, code formatting, and multi-line text
- **IDE Integration** - Open external editors (VS Code, Cursor, Notepad++, etc.) in new windows
- **Auto-Detection** - Automatically finds installed editors on your system
- **Secure Authentication** - SHA-256 password hashing with account lockout protection
- **Session Management** - Timeout-based sessions with privilege levels
- **Theme System** - Multiple built-in themes (Classic, White, Dark) with customization
- **Settings Management** - Comprehensive configuration system with persistence
- **Security Logging** - Complete audit trail of authentication events

## Project Structure

```
C_DIRECTORY/
├── src/
│   └── simple_gui_terminal.c    # Source code
├── data/
│   ├── filesystem.dat           # Virtual filesystem data
│   └── USERS/                   # User profiles directory
│       ├── Public/              # Public user directory
│       │   ├── Desktop/
│       │   ├── Documents/
│       │   ├── Downloads/
│       │   ├── Settings/
│       │   └── README.txt
│       └── Admin/               # Admin user directory
│           ├── Desktop/
│           ├── Documents/
│           ├── Downloads/
│           ├── Settings/
│           ├── System/          # Admin security & settings
│           │   ├── auth.dat     # Encrypted authentication data
│           │   ├── settings.dat # User settings
│           │   ├── themes/      # Custom themes directory
│           │   └── logs/        # Security audit logs
│           └── README.txt
├── build/
│   └── terminal.exe             # Compiled executable
├── build.bat                    # Build script
├── C Terminal.bat               # Launch script
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

## Quick Start

### First Time Setup
1. **Run the terminal** - It will show startup instructions
2. **Login to Admin**: `LOGIN Admin admin123`
3. **Change password**: `CHPASSWD admin123 <your_new_password>`
4. **Try themes**: `THEME white` or `THEME dark`
5. **View settings**: `SETTINGS`

### Basic Usage
- **Public Access**: No login required for basic file operations
- **Admin Access**: Login required for security features and advanced settings
- **Theme Switching**: Change appearance instantly with `THEME <name>`
- **Settings**: Customize font, colors, window size, and behavior
- **Help**: Type `HELP` for all available commands

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
- `ADDUSER <name>` - Create new dynamic user
- `LOGIN <username> <password>` - Login to any user
- `LOGOUT` - Logout current user
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

### IDE Commands (Open External Editors)
- `IDE vscode` - Open VS Code in new window
- `IDE code` - Alternative VS Code command
- `IDE cursor` - Open Cursor in new window
- `IDE notepad` - Open Notepad
- `IDE notepad++` - Open Notepad++ in new instance
- `IDE sublime` - Open Sublime Text
- `IDE atom` - Open Atom
- `IDE vim` - Open Vim
- `IDE LIST` - List all available editors
- `IDE HELP` - Show IDE command help

### Authentication Commands
- `LOGIN <username> <password>` - Login to user account
- `LOGOUT` - Logout from current session
- `CHPASSWD <old_password> <new_password>` - Change password
- `SESSIONS` - Show active sessions (Admin only)
- `WHOAMI` - Show current user and session info

### Theme & Customization Commands
- `THEME <name>` - Switch theme (classic, white, dark)
- `THEME LIST` - List available themes
- `SETTINGS` - Show current settings
- `SETTINGS RESET` - Reset settings to defaults
- `SET <setting> <value>` - Set configuration value
- `GET <setting>` - Get configuration value

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
# Create dynamic users
ADDUSER Developer
ADDUSER Tester

# Login to users
LOGIN Admin admin123
LOGIN Developer dev123
LOGIN Public public123

# Switch between users
USER Public
WHOAMI
MKDIR PublicFiles

USER Developer
WHOAMI
MKDIR Projects
WRITE Projects/hello.py "print('Hello from Developer!')"

USER Admin
WHOAMI
MKDIR AdminFiles
WRITE AdminFiles/secure.txt "Admin-only content"

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

### IDE Integration
```bash
# Check available editors
IDE LIST

# Open VS Code in current directory (new window)
IDE vscode

# Open Cursor in current directory (new window)
IDE cursor

# Open Notepad++ in new instance
IDE notepad++

# Open Notepad
IDE notepad

# Get help for IDE commands
IDE HELP

# Navigate to project and open in editor
CD MyProject
IDE vscode
# VS Code opens in new window with current directory loaded
```

### Authentication & Security
```bash
# Login to Admin account (default password)
LOGIN Admin admin123

# Change password
CHPASSWD admin123 MyNewSecurePassword123

# Check current session
WHOAMI

# View active sessions (Admin only)
SESSIONS

# Logout
LOGOUT
```

### Theme Customization
```bash
# List available themes
THEME LIST

# Switch to white theme
THEME white

# Switch to dark theme
THEME dark

# Switch back to classic green
THEME classic

# View current settings
SETTINGS

# Change font size
SET font_size 16

# Change cursor blink speed
SET cursor_blink_speed 300

# Change window size
SET window_width 1000
SET window_height 700

# Reset all settings to defaults
SETTINGS RESET
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
- **Built-in Users**: Admin and Public users with secure authentication
- **Dynamic User Creation**: `ADDUSER` creates users with custom names
- **Secure Login**: SHA-256 password hashing with account lockout protection
- **Isolated Directories**: Each user has separate file space
- **Profile Persistence**: User data saved between sessions
- **Dual Storage**: Users saved to both virtual filesystem and file explorer
- **System Integration**: Admin user has additional System directory for security

### Git Integration
- **Complete Git Support**: All major Git commands implemented
- **SSH Support**: Clone and push using SSH keys (recommended)
- **HTTPS Support**: Clone and push using Personal Access Tokens
- **Real Git Operations**: Uses system Git installation
- **Isolated Repositories**: Git operations don't affect project files
- **Automatic Sync**: Git changes sync between terminal and File Explorer

### IDE Integration
- **Auto-Detection**: Automatically finds installed editors on your system
- **Multiple Editors**: Supports VS Code, Cursor, Notepad++, Notepad, Sublime Text, Atom, Vim
- **New Window Support**: Each editor opens in its own window (no conflicts)
- **Working Directory Aware**: Opens editors in current terminal directory
- **Smart Arguments**: Uses appropriate flags for each editor (e.g., `-n` for VS Code/Cursor)
- **Error Handling**: Clear messages for missing or unavailable editors
- **Help System**: Built-in help and list commands for IDE functionality

### Security & Authentication
- **SHA-256 Password Hashing**: Industry-standard password security with random salt
- **Account Lockout Protection**: 5 failed attempts = 5-minute lockout
- **Session Management**: Configurable timeout with activity tracking
- **Privilege Levels**: Public (0), Admin (1), SuperAdmin (2) access control
- **Security Logging**: Complete audit trail of authentication events
- **Password Requirements**: Minimum 6 characters with strength validation
- **Session IDs**: Unique session tracking for security monitoring

### Theme & Customization
- **Built-in Themes**: Classic (green), White, and Dark themes
- **Real-time Switching**: Change themes instantly without restart
- **Comprehensive Settings**: Font, colors, window size, behavior options
- **Settings Persistence**: All preferences saved automatically
- **Customizable Elements**: Text color, background, cursor, selection colors
- **Window Management**: Resizable with saved dimensions
- **User Preferences**: Individual settings per user account

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

### Authentication Issues
1. **Default Admin Account**: Username: `Admin`, Password: `admin123`
2. **Account Locked**: Wait 5 minutes after 5 failed attempts, or restart terminal
3. **Password Change**: Use `CHPASSWD <old_password> <new_password>`
4. **Session Timeout**: Sessions expire after 30 minutes of inactivity (configurable)
5. **Check Session**: Use `WHOAMI` to see current user and session status

### Theme & Settings Issues
1. **Theme Not Applied**: Use `THEME LIST` to see available themes
2. **Settings Not Saved**: Settings are automatically saved, use `SETTINGS` to verify
3. **Reset Settings**: Use `SETTINGS RESET` to restore defaults
4. **Font Issues**: Use `SET font_name <name>` and `SET font_size <size>`
5. **Window Size**: Use `SET window_width <width>` and `SET window_height <height>`

## Version History

- **v5.0** - Added secure authentication system with SHA-256 hashing, session management, theme system, and comprehensive settings management
- **v4.0** - Added IDE integration with auto-detection, new window support, and Cursor editor support
- **v3.0** - Added multi-user system, interactive code editor, advanced text writing, and improved file deletion
- **v2.5** - Added SSH Git support, cleaned up debug output, and improved user experience
- **v2.0** - Added complete Git integration, trash system, and real filesystem sync
- **v1.5** - Added multi-user system and file persistence
- **v1.0** - Basic terminal with in-memory filesystem