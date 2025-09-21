#ifndef IDE_COMMANDS_H
#define IDE_COMMANDS_H

#include <stddef.h>

// IDE Command Function
void cmd_ide(const char* args);

// IDE Detection and Execution
int detect_ide(const char* editor_name, char* ide_path, size_t path_size);
int execute_ide(const char* ide_path, const char* working_dir, const char* args);
int find_ide_by_name(const char* name);

// IDE Help and Listing
void cmd_ide_help(void);
void cmd_ide_list(void);
void cmd_ide_config(const char* args);

// Utility Functions
void get_current_working_directory(char* path, size_t path_size);
int is_ide_installed(const char* ide_path);

#endif
