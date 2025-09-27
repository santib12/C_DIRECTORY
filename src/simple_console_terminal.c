#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>

// Global variables
static char g_current_dir[1024];
static char g_current_user[256];
static char g_commandHistory[100][512];
static int g_historyCount = 0;

// Color theme settings
static int g_theme = 0; // 0 = black/white, 1 = green/black, 2 = blue/black, 3 = red/black
static const char* g_bg_color = "\033[40m";  // Black background
static const char* g_fg_color = "\033[37m";  // White text

// Function declarations
static void initialize_terminal();
static void show_welcome();
static void show_prompt();
static void process_command(const char* input);
static void add_to_history(const char* command);
static void show_help();
static void list_directory();
static void change_directory(const char* path);
static void show_current_directory();
static void show_user_info();
static void list_users();
static void execute_system_command(const char* command);
static void show_command_history();
static void set_theme(int theme);
static void show_themes();
static void apply_colors();
static void detect_and_open_ide(const char* path);
static int check_command_exists(const char* command);

int main() {
    initialize_terminal();
    apply_colors(); // Apply black/white theme
    show_welcome();
    
    char input[512];
    while (1) {
        show_prompt();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        
        if (strlen(input) == 0) {
            continue;
        }
        
        // Add to history
        add_to_history(input);
        
        // Process command
        process_command(input);
    }
    
    printf("\nGoodbye!\n");
    return 0;
}

static void initialize_terminal() {
    // Get current user first
    struct passwd *pw = getpwuid(getuid());
    if (pw != NULL) {
        strncpy(g_current_user, pw->pw_name, sizeof(g_current_user) - 1);
        g_current_user[sizeof(g_current_user) - 1] = '\0';
        
        // Start in user's home directory
        strncpy(g_current_dir, pw->pw_dir, sizeof(g_current_dir) - 1);
        g_current_dir[sizeof(g_current_dir) - 1] = '\0';
        
        // Change to home directory
        if (chdir(g_current_dir) != 0) {
            // Fallback to /home if home directory is not accessible
            strcpy(g_current_dir, "/home");
            if (chdir(g_current_dir) != 0) {
                // If even /home fails, just continue with current directory
                printf("Warning: Could not change to home directory\n");
            }
        }
    } else {
        strcpy(g_current_user, "user");
        strcpy(g_current_dir, "/home");
        if (chdir(g_current_dir) != 0) {
            printf("Warning: Could not change to /home directory\n");
        }
    }
}

static void show_welcome() {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    NEXUS TERMINAL v2.0                       ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void show_prompt() {
    printf("%s@nexus:~$ ", g_current_user);
    fflush(stdout);
}

static void process_command(const char* input) {
    char command[256];
    char args[256];
    
    // Parse command and arguments
    if (sscanf(input, "%255s %255[^\n]", command, args) == 2) {
        // Command with arguments
    } else if (sscanf(input, "%255s", command) == 1) {
        // Command without arguments
        args[0] = '\0';
    } else {
        return;
    }
    
    // Convert command to lowercase for case-insensitive matching
    for (int i = 0; command[i]; i++) {
        command[i] = tolower(command[i]);
    }
    
    // Process commands
    if (strcmp(command, "help") == 0) {
        show_help();
    }
    else if (strcmp(command, "dir") == 0 || strcmp(command, "ls") == 0) {
        list_directory();
    }
    else if (strcmp(command, "cd") == 0) {
        change_directory(args);
    }
    else if (strcmp(command, "pwd") == 0) {
        show_current_directory();
    }
    else if (strcmp(command, "whoami") == 0) {
        show_user_info();
    }
    else if (strcmp(command, "users") == 0) {
        list_users();
    }
    else if (strcmp(command, "history") == 0) {
        show_command_history();
    }
    else if (strcmp(command, "clear") == 0) {
        if (system("clear") != 0) {
            printf("Warning: Could not clear screen\n");
        }
    }
    else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        printf("Goodbye!\n");
        exit(0);
    }
    else if (strcmp(command, "echo") == 0) {
        printf("%s\n", args);
    }
    else if (strcmp(command, "theme") == 0) {
        if (strlen(args) == 0) {
            show_themes();
        } else {
            int theme_num = atoi(args);
            set_theme(theme_num);
        }
    }
    else if (strcmp(command, "black") == 0) {
        printf("\033[40m\033[37m\033[2J");
        fflush(stdout);
        printf("Background set to black with white text\n");
    }
    else if (strcmp(command, "ide") == 0) {
        detect_and_open_ide(args);
    }
    else {
        // Try to execute as system command
        execute_system_command(input);
    }
}

static void add_to_history(const char* command) {
    if (g_historyCount < 100) {
        size_t cmd_len = strlen(command);
        size_t max_len = sizeof(g_commandHistory[g_historyCount]) - 1;
        
        if (cmd_len > max_len) {
            // Truncate command if too long
            strncpy(g_commandHistory[g_historyCount], command, max_len);
            g_commandHistory[g_historyCount][max_len] = '\0';
        } else {
            strcpy(g_commandHistory[g_historyCount], command);
        }
        g_historyCount++;
    }
}

static void show_help() {
    printf("\n=== NEXUS TERMINAL - Available Commands ===\n");
    printf("help          - Show this help message\n");
    printf("dir/ls        - List directory contents\n");
    printf("cd <path>     - Change directory\n");
    printf("pwd           - Show current directory\n");
    printf("whoami        - Show current user info\n");
    printf("users         - List available users\n");
    printf("history       - Show command history\n");
    printf("clear         - Clear screen\n");
    printf("echo <text>   - Print text\n");
    printf("theme [0-3]  - Change color theme\n");
    printf("ide [path]    - Open IDE/editor (auto-detects available)\n");
    printf("ide cursor    - Force open with Cursor\n");
    printf("ide code      - Force open with VS Code\n");
    printf("exit/quit     - Exit terminal\n");
    printf("\nYou can also run any Linux command directly!\n");
    printf("Examples: git status, npm install, code .\n");
    printf("===========================================\n\n");
}

static void list_directory() {
    DIR *dir = opendir(g_current_dir);
    if (dir == NULL) {
        printf("Error: Cannot access directory '%s'\n", g_current_dir);
        return;
    }
    
    printf("\nContents of %s:\n", g_current_dir);
    printf("----------------------------------------\n");
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; // Skip hidden files
        }
        
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", g_current_dir, entry->d_name);
        
        struct stat file_stat;
        if (stat(full_path, &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode)) {
                printf("📁 %s/\n", entry->d_name);
            } else {
                printf("📄 %s\n", entry->d_name);
            }
        } else {
            printf("❓ %s\n", entry->d_name);
        }
    }
    
    closedir(dir);
    printf("\n");
}

static void change_directory(const char* path) {
    if (strlen(path) == 0) {
        // Change to home directory
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) {
            strncpy(g_current_dir, pw->pw_dir, sizeof(g_current_dir) - 1);
            g_current_dir[sizeof(g_current_dir) - 1] = '\0';
        }
        return;
    }
    
    char new_path[2048];
    
    if (path[0] == '/') {
        // Absolute path
        strncpy(new_path, path, sizeof(new_path) - 1);
        new_path[sizeof(new_path) - 1] = '\0';
    } else {
        // Relative path
        snprintf(new_path, sizeof(new_path), "%s/%s", g_current_dir, path);
    }
    
    // Check if directory exists
    struct stat dir_stat;
    if (stat(new_path, &dir_stat) != 0) {
        printf("Error: Directory '%s' not found\n", path);
        return;
    }
    
    if (!S_ISDIR(dir_stat.st_mode)) {
        printf("Error: '%s' is not a directory\n", path);
        return;
    }
    
    // Change directory
    if (chdir(new_path) == 0) {
        strncpy(g_current_dir, new_path, sizeof(g_current_dir) - 1);
        g_current_dir[sizeof(g_current_dir) - 1] = '\0';
        printf("Changed to: %s\n", g_current_dir);
    } else {
        printf("Error: Cannot change to directory '%s'\n", path);
    }
}

static void show_current_directory() {
    printf("Current directory: %s\n", g_current_dir);
}

static void show_user_info() {
    printf("\n=== User Information ===\n");
    printf("Current user: %s\n", g_current_user);
    printf("User ID: %d\n", getuid());
    printf("Current directory: %s\n", g_current_dir);
    printf("========================\n\n");
}

static void list_users() {
    printf("\n=== Available Users ===\n");
    
    struct passwd *pw;
    setpwent(); // Reset to beginning of password file
    
    while ((pw = getpwent()) != NULL) {
        printf("User: %s (UID: %d)\n", pw->pw_name, pw->pw_uid);
    }
    
    endpwent();
    printf("========================\n\n");
}

static void execute_system_command(const char* command) {
    printf("Executing: %s\n", command);
    int result = system(command);
    if (result != 0) {
        printf("Command failed with exit code: %d\n", result);
    }
}

static void show_command_history() {
    printf("\n=== Command History ===\n");
    for (int i = 0; i < g_historyCount; i++) {
        printf("%3d: %s\n", i + 1, g_commandHistory[i]);
    }
    printf("========================\n\n");
}

static void apply_colors() {
    // Clear screen and set colors
    if (system("clear") != 0) {
        // Fallback if clear command fails
        printf("\033[2J"); // Clear screen
    }
    printf("\033[H");  // Move cursor to top-left
    printf("%s%s", g_bg_color, g_fg_color);
    printf("\033[2J"); // Clear screen with background color
    fflush(stdout);
}

static void set_theme(int theme) {
    g_theme = theme;
    
    switch (theme) {
        case 0: // Black/White
            g_bg_color = "\033[40m";
            g_fg_color = "\033[37m";
            break;
        case 1: // Green/Black
            g_bg_color = "\033[40m";
            g_fg_color = "\033[32m";
            break;
        case 2: // Blue/Black
            g_bg_color = "\033[40m";
            g_fg_color = "\033[34m";
            break;
        case 3: // Red/Black
            g_bg_color = "\033[40m";
            g_fg_color = "\033[31m";
            break;
        default:
            printf("Invalid theme number. Use 'theme' to see available themes.\n");
            return;
    }
    
    apply_colors();
    printf("Theme changed to %d\n", theme);
}

static void show_themes() {
    printf("\n=== Available Themes ===\n");
    printf("0 - Black/White (default)\n");
    printf("1 - Green/Black\n");
    printf("2 - Blue/Black\n");
    printf("3 - Red/Black\n");
    printf("\nUsage: theme [0-3]\n");
    printf("Example: theme 1\n");
    printf("========================\n\n");
}

static int check_command_exists(const char* command) {
    char test_command[512];
    snprintf(test_command, sizeof(test_command), "which %s > /dev/null 2>&1", command);
    return system(test_command) == 0;
}

static void detect_and_open_ide(const char* path) {
    char target_path[2048];
    char preferred_ide[64] = "";
    
    // Check if path starts with an IDE name
    if (strncmp(path, "cursor", 6) == 0 && (path[6] == ' ' || path[6] == '\0')) {
        strcpy(preferred_ide, "cursor");
        // Skip "cursor " part if present
        if (path[6] == ' ') {
            path += 7;
        } else {
            path = "";
        }
    } else if (strncmp(path, "code", 4) == 0 && (path[4] == ' ' || path[4] == '\0')) {
        strcpy(preferred_ide, "code");
        // Skip "code " part if present
        if (path[4] == ' ') {
            path += 5;
        } else {
            path = "";
        }
    } else if (strncmp(path, "subl", 4) == 0 && (path[4] == ' ' || path[4] == '\0')) {
        strcpy(preferred_ide, "subl");
        // Skip "subl " part if present
        if (path[4] == ' ') {
            path += 5;
        } else {
            path = "";
        }
    }
    
    // Determine target path
    if (strlen(path) == 0) {
        // No path specified, use current directory
        strncpy(target_path, g_current_dir, sizeof(target_path) - 1);
        target_path[sizeof(target_path) - 1] = '\0';
    } else if (path[0] == '/') {
        // Absolute path
        strncpy(target_path, path, sizeof(target_path) - 1);
        target_path[sizeof(target_path) - 1] = '\0';
    } else {
        // Relative path
        snprintf(target_path, sizeof(target_path), "%s/%s", g_current_dir, path);
    }
    
    if (strlen(preferred_ide) > 0) {
        printf("Using preferred IDE: %s\n", preferred_ide);
    } else {
        printf("Detecting available IDE/editor...\n");
    }
    
    // Check for preferred IDE first, then fall back to detection
    if (strlen(preferred_ide) > 0) {
        if (check_command_exists(preferred_ide)) {
            printf("Opening with %s...\n", preferred_ide);
            char command[4096];
            snprintf(command, sizeof(command), "%s \"%s\"", preferred_ide, target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with %s\n", preferred_ide);
            }
        } else {
            printf("Error: %s not found. Falling back to auto-detection...\n", preferred_ide);
            preferred_ide[0] = '\0'; // Clear preferred IDE to trigger auto-detection
        }
    }
    
    // Auto-detection if no preferred IDE or preferred IDE not found
    if (strlen(preferred_ide) == 0) {
        // Check for various IDEs/editors in order of preference
        if (check_command_exists("cursor")) {
            printf("Opening with Cursor...\n");
            char command[4096];
            snprintf(command, sizeof(command), "cursor \"%s\"", target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with Cursor\n");
            }
        }
        else if (check_command_exists("code")) {
            printf("Opening with Visual Studio Code...\n");
            char command[4096];
            snprintf(command, sizeof(command), "code \"%s\"", target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with VS Code\n");
            }
        }
        else if (check_command_exists("subl")) {
            printf("Opening with Sublime Text...\n");
            char command[4096];
            snprintf(command, sizeof(command), "subl \"%s\"", target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with Sublime Text\n");
            }
        }
        else if (check_command_exists("atom")) {
            printf("Opening with Atom...\n");
            char command[4096];
            snprintf(command, sizeof(command), "atom \"%s\"", target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with Atom\n");
            }
        }
        else if (check_command_exists("vim")) {
            printf("Opening with Vim...\n");
            char command[4096];
            snprintf(command, sizeof(command), "vim \"%s\"", target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with Vim\n");
            }
        }
        else if (check_command_exists("nano")) {
            printf("Opening with Nano...\n");
            char command[4096];
            snprintf(command, sizeof(command), "nano \"%s\"", target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with Nano\n");
            }
        }
        else if (check_command_exists("gedit")) {
            printf("Opening with Gedit...\n");
            char command[4096];
            snprintf(command, sizeof(command), "gedit \"%s\"", target_path);
            if (system(command) != 0) {
                printf("Warning: Failed to open with Gedit\n");
            }
        }
        else {
            printf("No IDE/editor detected. Available options:\n");
            printf("- Install VS Code: sudo snap install code --classic\n");
            printf("- Install Cursor: Download from cursor.sh\n");
            printf("- Install Sublime Text: sudo snap install sublime-text --classic\n");
            printf("- Use built-in editors: vim, nano, gedit\n");
            printf("\nYou can also use: code ., cursor ., subl . etc.\n");
        }
    }
}
