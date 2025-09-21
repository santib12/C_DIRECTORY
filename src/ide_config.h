#ifndef IDE_CONFIG_H
#define IDE_CONFIG_H

#include <stddef.h>

// IDE Configuration Constants
#define MAX_IDES 10
#define MAX_EDITOR_NAME 32
#define MAX_EDITOR_PATH 512
#define MAX_EDITOR_ARGS 128

// IDE Configuration Structure
typedef struct {
    char name[MAX_EDITOR_NAME];
    char path[MAX_EDITOR_PATH];
    char args[MAX_EDITOR_ARGS];
    int enabled;
} IDEConfig;

// Global IDE Configuration
extern IDEConfig g_ide_configs[MAX_IDES];
extern int g_ide_count;

// IDE Detection Functions
void init_ide_configs(void);
int detect_vscode(char* path, size_t path_size);
int detect_notepad(char* path, size_t path_size);
int detect_notepad_plus_plus(char* path, size_t path_size);
int detect_sublime_text(char* path, size_t path_size);
int detect_atom(char* path, size_t path_size);
int detect_vim(char* path, size_t path_size);

// Configuration Management
void load_ide_config(void);
void save_ide_config(void);
void add_custom_ide(const char* name, const char* path, const char* args);

#endif
