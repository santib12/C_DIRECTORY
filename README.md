# NEXUS TERMINAL v2.0 - Linux Edition

A powerful, clean terminal application for Linux that provides a modern command-line interface with advanced features and IDE integration.

## 🚀 Features

- **Real Filesystem Access**: Works with your actual Linux directories and files
- **IDE Auto-Detection**: Automatically detects and opens with available editors (VS Code, Cursor, Sublime Text, etc.)
- **Command History**: Full command history support (100 commands)
- **User Management**: View and switch between system users
- **Theme Support**: Multiple color themes (black/white, green/black, blue/black, red/black)
- **System Command Execution**: Run any Linux command directly
- **Clean Interface**: Professional terminal interface with minimal clutter
- **Git Integration**: Full git support including clone, push, pull operations

## 📁 Project Structure

```
C_DIRECTORY/
├── src/
│   └── simple_console_terminal.c  # Simple console terminal
├── build/                         # Compiled executables
├── Makefile                       # Build configuration
└── README.md                      # This file
```

## 🛠️ Installation & Setup

### Prerequisites
```bash
# Install build tools
sudo apt-get update
sudo apt-get install -y build-essential
```

### Quick Start
```bash
# Build the terminal
make

# Run the terminal
make run-simple

# Or launch in native window
make launch-simple
```

## 🎯 Usage

### **Simple Console Terminal**
```bash
# Build and launch in native window (recommended)
make launch-simple

# Or run directly
make run-simple
```

**Features:**
- Access real Linux directories and files
- Case-insensitive directory/file operations
- Command history
- User management
- Theme support
- Clean, professional interface

## 📋 Available Commands

### **File System Commands**
- `dir`, `ls` - List directory contents with file/folder icons
- `cd <folder>` - Change directory (use without arguments for home directory)
- `pwd` - Print working directory

### **System Commands**
- `echo <text>` - Print text
- `clear` - Clear screen
- `history` - Show command history (last 100 commands)
- `whoami` - Show current user information
- `users` - List all system users
- `exit`, `quit` - Close application

### **IDE/Editor Commands**
- `ide` - Auto-detect and open with best available IDE
- `ide cursor` - Force open with Cursor
- `ide code` - Force open with VS Code
- `ide subl` - Force open with Sublime Text
- `ide <path>` - Open specific directory/file with detected IDE

### **Theme Commands**
- `theme [0-3]` - Switch between color themes:
  - `0` - Black/White (default)
  - `1` - Green/Black
  - `2` - Blue/Black
  - `3` - Red/Black
- `black` - Force black background with white text

### **System Integration**
- **Any Linux command** - Execute any system command directly
- **Git operations** - Full git support (clone, push, pull, status, etc.)
- **Package managers** - npm, pip, apt, snap, etc.
- **Development tools** - All development commands work normally

## 🔧 Build Options

```bash
# Build the terminal
make

# Run the terminal
make run-simple

# Launch in native window
make launch-simple

# Clean build artifacts
make clean

# Show help
make help
```

## 🎨 Terminal Features

### **IDE Auto-Detection**
The terminal automatically detects and uses the best available IDE:
```bash
# Auto-detect best IDE
ide

# Force specific IDE
ide cursor
ide code
ide subl

# Open specific directory
ide /path/to/project
ide .
```

**Supported IDEs (in priority order):**
1. Cursor (AI-powered editor)
2. Visual Studio Code
3. Sublime Text
4. Atom
5. Vim
6. Nano
7. Gedit

### **Command History**
- Use `history` command to see previous commands
- Commands are automatically saved (last 100 commands)
- Full command history tracking

### **Theme Support**
- Default: Black background with white text
- Available themes: black/white, green/black, blue/black, red/black
- Use `theme [0-3]` command to switch themes
- Use `black` command to force black background

### **System Integration**
- **Git Operations**: Full git support including authentication
- **Package Managers**: npm, pip, apt, snap, etc.
- **Development Tools**: All development commands work normally
- **File Operations**: Complete filesystem access

### **User Management**
- `whoami` - Show current user information
- `users` - List all users on the system
- Terminal starts in the current user's home directory

## 🚀 Quick Commands

```bash
# Start the terminal
./build/simple_terminal

# Build and run
make clean && make

# Get help
make help
```

## 📝 Usage Examples

### **Development Workflow**
```bash
# Clone a repository
git clone https://github.com/user/repo.git

# Open in IDE
ide .

# Run development commands
npm install
npm start

# Check status
git status
```

### **File Operations**
```bash
# Navigate directories
cd /home/user/projects
ls

# Open files
ide file.txt
ide /path/to/directory
```

### **System Management**
```bash
# Check users
users
whoami

# View history
history

# Change theme
theme 1
```

## 📝 Notes

- **Clean Interface**: Professional terminal with minimal clutter
- **Full System Access**: Complete Linux command support
- **IDE Integration**: Smart IDE detection and opening
- **Git Support**: Full git operations including authentication
- **Theme Support**: Multiple color schemes available
- **Command History**: Automatic command tracking (100 commands)
- **User Management**: Complete user system integration

## 🔍 Troubleshooting

### **Build Issues**
```bash
# Install missing dependencies
sudo apt-get install build-essential

# Clean and rebuild
make clean
make
```

### **Terminal Window Issues**
If the terminal doesn't open in a new window, it will fall back to the current terminal.

### **Permission Issues**
Make sure the build directory has proper permissions:
```bash
chmod +x build/*
```

## 🎯 Key Features Summary

- ✅ **IDE Auto-Detection** - Smart detection of available editors
- ✅ **Git Integration** - Full git support with authentication
- ✅ **Command History** - 100 command history tracking
- ✅ **Theme Support** - Multiple color schemes
- ✅ **System Commands** - Execute any Linux command
- ✅ **User Management** - Complete user system integration
- ✅ **Clean Interface** - Professional terminal design
- ✅ **No Redundancy** - Clean, efficient codebase

---

**NEXUS TERMINAL v2.0** - Your clean, powerful Linux terminal companion with IDE integration! 🚀