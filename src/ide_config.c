#include "ide_config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global IDE Configuration
IDEConfig g_ide_configs[MAX_IDES];
int g_ide_count = 0;

// Initialize default IDE configurations
void init_ide_configs(void) {
    g_ide_count = 0;
    
    // VS Code
    if (detect_vscode(g_ide_configs[g_ide_count].path, sizeof(g_ide_configs[g_ide_count].path))) {
        strcpy(g_ide_configs[g_ide_count].name, "vscode");
        strcpy(g_ide_configs[g_ide_count].args, "");
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
    
    // Alternative VS Code command
    if (detect_vscode(g_ide_configs[g_ide_count].path, sizeof(g_ide_configs[g_ide_count].path))) {
        strcpy(g_ide_configs[g_ide_count].name, "code");
        strcpy(g_ide_configs[g_ide_count].args, "");
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
    
    // Notepad
    if (detect_notepad(g_ide_configs[g_ide_count].path, sizeof(g_ide_configs[g_ide_count].path))) {
        strcpy(g_ide_configs[g_ide_count].name, "notepad");
        strcpy(g_ide_configs[g_ide_count].args, "");
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
    
    // Notepad++
    if (detect_notepad_plus_plus(g_ide_configs[g_ide_count].path, sizeof(g_ide_configs[g_ide_count].path))) {
        strcpy(g_ide_configs[g_ide_count].name, "notepad++");
        strcpy(g_ide_configs[g_ide_count].args, "");
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
    
    // Sublime Text
    if (detect_sublime_text(g_ide_configs[g_ide_count].path, sizeof(g_ide_configs[g_ide_count].path))) {
        strcpy(g_ide_configs[g_ide_count].name, "sublime");
        strcpy(g_ide_configs[g_ide_count].args, "");
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
    
    // Atom
    if (detect_atom(g_ide_configs[g_ide_count].path, sizeof(g_ide_configs[g_ide_count].path))) {
        strcpy(g_ide_configs[g_ide_count].name, "atom");
        strcpy(g_ide_configs[g_ide_count].args, "");
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
    
    // Vim
    if (detect_vim(g_ide_configs[g_ide_count].path, sizeof(g_ide_configs[g_ide_count].path))) {
        strcpy(g_ide_configs[g_ide_count].name, "vim");
        strcpy(g_ide_configs[g_ide_count].args, "");
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
}

// Detect VS Code
int detect_vscode(char* path, size_t path_size) {
    // Try common VS Code installation paths
    const char* vscode_paths[] = {
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe",
        "C:\\Program Files\\Microsoft VS Code\\Code.exe",
        "C:\\Program Files (x86)\\Microsoft VS Code\\Code.exe",
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Microsoft VS Code\\bin\\code.cmd"
    };
    
    for (int i = 0; i < 4; i++) {
        char expanded_path[MAX_EDITOR_PATH];
        ExpandEnvironmentStringsA(vscode_paths[i], expanded_path, sizeof(expanded_path));
        
        if (GetFileAttributesA(expanded_path) != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, expanded_path, path_size - 1);
            path[path_size - 1] = '\0';
            return 1;
        }
    }
    
    // Try PATH
    if (system("where code >nul 2>&1") == 0) {
        strcpy(path, "code");
        return 1;
    }
    
    return 0;
}

// Detect Notepad
int detect_notepad(char* path, size_t path_size) {
    const char* notepad_paths[] = {
        "C:\\Windows\\System32\\notepad.exe",
        "C:\\Windows\\notepad.exe"
    };
    
    for (int i = 0; i < 2; i++) {
        if (GetFileAttributesA(notepad_paths[i]) != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, notepad_paths[i], path_size - 1);
            path[path_size - 1] = '\0';
            return 1;
        }
    }
    
    return 0;
}

// Detect Notepad++
int detect_notepad_plus_plus(char* path, size_t path_size) {
    const char* npp_paths[] = {
        "C:\\Program Files\\Notepad++\\notepad++.exe",
        "C:\\Program Files (x86)\\Notepad++\\notepad++.exe",
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Notepad++\\notepad++.exe"
    };
    
    for (int i = 0; i < 3; i++) {
        char expanded_path[MAX_EDITOR_PATH];
        ExpandEnvironmentStringsA(npp_paths[i], expanded_path, sizeof(expanded_path));
        
        if (GetFileAttributesA(expanded_path) != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, expanded_path, path_size - 1);
            path[path_size - 1] = '\0';
            return 1;
        }
    }
    
    return 0;
}

// Detect Sublime Text
int detect_sublime_text(char* path, size_t path_size) {
    const char* sublime_paths[] = {
        "C:\\Program Files\\Sublime Text\\sublime_text.exe",
        "C:\\Program Files (x86)\\Sublime Text\\sublime_text.exe",
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Sublime Text\\sublime_text.exe"
    };
    
    for (int i = 0; i < 3; i++) {
        char expanded_path[MAX_EDITOR_PATH];
        ExpandEnvironmentStringsA(sublime_paths[i], expanded_path, sizeof(expanded_path));
        
        if (GetFileAttributesA(expanded_path) != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, expanded_path, path_size - 1);
            path[path_size - 1] = '\0';
            return 1;
        }
    }
    
    return 0;
}

// Detect Atom
int detect_atom(char* path, size_t path_size) {
    const char* atom_paths[] = {
        "C:\\Users\\%USERNAME%\\AppData\\Local\\atom\\atom.exe",
        "C:\\Program Files\\Atom\\atom.exe",
        "C:\\Program Files (x86)\\Atom\\atom.exe"
    };
    
    for (int i = 0; i < 3; i++) {
        char expanded_path[MAX_EDITOR_PATH];
        ExpandEnvironmentStringsA(atom_paths[i], expanded_path, sizeof(expanded_path));
        
        if (GetFileAttributesA(expanded_path) != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, expanded_path, path_size - 1);
            path[path_size - 1] = '\0';
            return 1;
        }
    }
    
    return 0;
}

// Detect Vim
int detect_vim(char* path, size_t path_size) {
    const char* vim_paths[] = {
        "C:\\Program Files\\Vim\\vim82\\vim.exe",
        "C:\\Program Files (x86)\\Vim\\vim82\\vim.exe",
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Vim\\vim82\\vim.exe"
    };
    
    for (int i = 0; i < 3; i++) {
        char expanded_path[MAX_EDITOR_PATH];
        ExpandEnvironmentStringsA(vim_paths[i], expanded_path, sizeof(expanded_path));
        
        if (GetFileAttributesA(expanded_path) != INVALID_FILE_ATTRIBUTES) {
            strncpy(path, expanded_path, path_size - 1);
            path[path_size - 1] = '\0';
            return 1;
        }
    }
    
    // Try PATH
    if (system("where vim >nul 2>&1") == 0) {
        strcpy(path, "vim");
        return 1;
    }
    
    return 0;
}

// Load IDE configuration from file (future enhancement)
void load_ide_config(void) {
    // TODO: Load from config file
    init_ide_configs();
}

// Save IDE configuration to file (future enhancement)
void save_ide_config(void) {
    // TODO: Save to config file
}

// Add custom IDE
void add_custom_ide(const char* name, const char* path, const char* args) {
    if (g_ide_count < MAX_IDES) {
        strncpy(g_ide_configs[g_ide_count].name, name, MAX_EDITOR_NAME - 1);
        strncpy(g_ide_configs[g_ide_count].path, path, MAX_EDITOR_PATH - 1);
        strncpy(g_ide_configs[g_ide_count].args, args, MAX_EDITOR_ARGS - 1);
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
}
