#include "ide_commands.h"
#include "ide_config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration
typedef struct Directory Directory;

// External references (will be resolved at link time)
extern Directory* g_cwd;
extern char g_currentUser[64];

// IDE Command Implementation
void cmd_ide(const char* args) {
    if (!args || !*args) {
        cmd_ide_help();
        return;
    }
    
    // Parse arguments
    char editor_name[64];
    char additional_args[256] = "";
    
    if (sscanf(args, "%63s %255[^\n]", editor_name, additional_args) < 1) {
        cmd_ide_help();
        return;
    }
    
    // Find the IDE
    int ide_index = find_ide_by_name(editor_name);
    if (ide_index == -1) {
        printf("IDE '%s' not found. Use 'IDE LIST' to see available editors.\n", editor_name);
        return;
    }
    
    // Get current working directory
    char working_dir[1024];
    get_current_working_directory(working_dir, sizeof(working_dir));
    
    // Execute the IDE
    char full_args[512];
    if (strlen(additional_args) > 0) {
        snprintf(full_args, sizeof(full_args), "%s %s", g_ide_configs[ide_index].args, additional_args);
    } else {
        strcpy(full_args, g_ide_configs[ide_index].args);
    }
    
    if (execute_ide(g_ide_configs[ide_index].path, working_dir, full_args)) {
        printf("Opening %s in %s...\n", g_ide_configs[ide_index].name, working_dir);
    } else {
        printf("Failed to open %s. Please check if it's installed.\n", g_ide_configs[ide_index].name);
    }
}

// Find IDE by name
int find_ide_by_name(const char* name) {
    for (int i = 0; i < g_ide_count; i++) {
        if (strcmp(g_ide_configs[i].name, name) == 0 && g_ide_configs[i].enabled) {
            return i;
        }
    }
    return -1;
}

// Execute IDE with working directory
int execute_ide(const char* ide_path, const char* working_dir, const char* args) {
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_NORMAL;
    
    // Build command line
    char command_line[1024];
    if (strlen(args) > 0) {
        snprintf(command_line, sizeof(command_line), "\"%s\" %s \"%s\"", ide_path, args, working_dir);
    } else {
        snprintf(command_line, sizeof(command_line), "\"%s\" \"%s\"", ide_path, working_dir);
    }
    
    // Create process
    BOOL success = CreateProcessA(
        NULL,                    // Application name
        command_line,            // Command line
        NULL,                    // Process security attributes
        NULL,                    // Thread security attributes
        FALSE,                   // Inherit handles
        0,                       // Creation flags
        NULL,                    // Environment
        working_dir,             // Current directory
        &si,                     // Startup info
        &pi                      // Process information
    );
    
    if (success) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }
    
    return 0;
}

// Get current working directory
void get_current_working_directory(char* path, size_t path_size) {
    // Get the main project directory
    char program_dir[1024];
    // This would need to call get_main_project_dir from the main file
    // For now, we'll use a simple approach
    
    // Build the real path based on current virtual directory
    if (g_cwd) {
        // Convert virtual path to real path
        char virtual_path[1024];
        // This would need to be implemented based on your existing path conversion logic
        // For now, use a simple path construction
        snprintf(path, path_size, "C:\\Users\\%s\\Desktop\\DIRECTORY\\C_DIRECTORY\\data\\USERS\\%s", 
                getenv("USERNAME"), g_currentUser);
    } else {
        strcpy(path, "C:\\");
    }
}

// Check if IDE is installed
int is_ide_installed(const char* ide_path) {
    return GetFileAttributesA(ide_path) != INVALID_FILE_ATTRIBUTES;
}

// IDE Help Command
void cmd_ide_help(void) {
    printf("IDE Command Help:\n");
    printf("===============\n");
    printf("Usage: IDE <editor> [options]\n");
    printf("\nAvailable Editors:\n");
    cmd_ide_list();
    printf("\nExamples:\n");
    printf("  IDE vscode              - Open VS Code in current directory\n");
    printf("  IDE code                - Alternative VS Code command\n");
    printf("  IDE notepad             - Open Notepad\n");
    printf("  IDE notepad++           - Open Notepad++\n");
    printf("  IDE sublime             - Open Sublime Text\n");
    printf("  IDE atom                - Open Atom\n");
    printf("  IDE vim                 - Open Vim\n");
    printf("\nVS Code Specific:\n");
    printf("  IDE vscode .            - Open current directory in VS Code\n");
    printf("  IDE vscode filename.txt - Open specific file in VS Code\n");
    printf("  IDE vscode -n           - Open new window in VS Code\n");
}

// IDE List Command
void cmd_ide_list(void) {
    printf("Available IDEs:\n");
    printf("===============\n");
    
    for (int i = 0; i < g_ide_count; i++) {
        if (g_ide_configs[i].enabled) {
            printf("  %-12s - %s\n", g_ide_configs[i].name, g_ide_configs[i].path);
        }
    }
    
    if (g_ide_count == 0) {
        printf("  No IDEs detected. Please install an editor.\n");
    }
}

// IDE Config Command (for future enhancement)
void cmd_ide_config(const char* args) {
    printf("IDE Configuration:\n");
    printf("==================\n");
    printf("This feature will be available in a future update.\n");
    printf("For now, IDEs are auto-detected from common installation paths.\n");
}
