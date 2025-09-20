#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <windows.h>

#define MAX_NAME 256
#define MAX_CHILDREN 100
#define MAX_FILES 100
#define MAX_FILE_SIZE 1024

// File structure
typedef struct File {
    char name[MAX_NAME];
    char content[MAX_FILE_SIZE];
} File;

// Directory structure
typedef struct Directory {
    char name[MAX_NAME];
    struct Directory* parent;
    struct Directory* children[MAX_CHILDREN];
    File* files[MAX_FILES];
    int child_count;
    int file_count;
} Directory;

// Global variables
Directory* root;
Directory* current_dir;
HANDLE hConsole;

// Function prototypes
Directory* create_directory(const char* name);
File* create_file(const char* name);
Directory* find_child(Directory* parent, const char* name);
File* find_file(Directory* parent, const char* name);
void add_child(Directory* parent, Directory* child);
void add_file(Directory* parent, File* file);
void print_path(Directory* dir);
void list_directory(Directory* dir);
void save_filesystem(const char* filename);
void load_filesystem(const char* filename);
void init_console();
void clear_screen();
void set_color(int color);
void print_prompt();

// Console commands
void mkdir_command(const char* name);
void touch_command(const char* name);
void cd_command(const char* path);
void pwd_command();
void ls_command();
void cat_command(const char* name);
void echo_command(const char* text);
void save_command(const char* filename);
void load_command(const char* filename);
void help_command();
void clear_command();

// Create a new directory
Directory* create_directory(const char* name) {
    Directory* new_dir = (Directory*)malloc(sizeof(Directory));
    strcpy(new_dir->name, name);
    new_dir->parent = NULL;
    new_dir->child_count = 0;
    new_dir->file_count = 0;
    
    // Initialize arrays
    for(int i = 0; i < MAX_CHILDREN; i++) {
        new_dir->children[i] = NULL;
    }
    for(int i = 0; i < MAX_FILES; i++) {
        new_dir->files[i] = NULL;
    }
    
    return new_dir;
}

// Create a new file
File* create_file(const char* name) {
    File* new_file = (File*)malloc(sizeof(File));
    strcpy(new_file->name, name);
    new_file->content[0] = '\0';
    return new_file;
}

// Find a child directory by name
Directory* find_child(Directory* parent, const char* name) {
    for(int i = 0; i < parent->child_count; i++) {
        if(strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }
    return NULL;
}

// Find a file by name
File* find_file(Directory* parent, const char* name) {
    for(int i = 0; i < parent->file_count; i++) {
        if(strcmp(parent->files[i]->name, name) == 0) {
            return parent->files[i];
        }
    }
    return NULL;
}

// Add a child directory
void add_child(Directory* parent, Directory* child) {
    if(parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count] = child;
        child->parent = parent;
        parent->child_count++;
    }
}

// Add a file
void add_file(Directory* parent, File* file) {
    if(parent->file_count < MAX_FILES) {
        parent->files[parent->file_count] = file;
        parent->file_count++;
    }
}

// Print the full path of a directory
void print_path(Directory* dir) {
    if(dir == root) {
        printf("C:\\");
        return;
    }
    
    if(dir->parent != root) {
        print_path(dir->parent);
        printf("\\");
    }
    printf("%s", dir->name);
}

// List directory contents
void list_directory(Directory* dir) {
    set_color(14); // Yellow
    printf("Directory of ");
    print_path(dir);
    printf("\n\n");
    set_color(7); // White
    
    if(dir->child_count == 0 && dir->file_count == 0) {
        printf("(empty)\n");
        return;
    }
    
    // List directories first
    for(int i = 0; i < dir->child_count; i++) {
        printf("%-20s <DIR>\n", dir->children[i]->name);
    }
    
    // List files
    for(int i = 0; i < dir->file_count; i++) {
        printf("%-20s\n", dir->files[i]->name);
    }
}

// Save filesystem to file
void save_filesystem(const char* filename) {
    FILE* file = fopen(filename, "w");
    if(!file) {
        set_color(12); // Red
        printf("Error: Cannot save to '%s'\n", filename);
        set_color(7); // White
        return;
    }
    
    // Simple save format: path|type|content
    fprintf(file, "filesystem_data_v1\n");
    
    // Save root structure (recursive save would be better, but keeping it simple)
    fprintf(file, "C:\\|DIR|\n");
    
    // Save all directories and files (simplified version)
    for(int i = 0; i < current_dir->child_count; i++) {
        fprintf(file, "%s\\%s|DIR|\n", current_dir->name, current_dir->children[i]->name);
    }
    
    for(int i = 0; i < current_dir->file_count; i++) {
        fprintf(file, "%s\\%s|FILE|%s\n", current_dir->name, current_dir->files[i]->name, 
                current_dir->files[i]->content);
    }
    
    fclose(file);
    set_color(10); // Green
    printf("Filesystem saved to '%s'\n", filename);
    set_color(7); // White
}

// Load filesystem from file
void load_filesystem(const char* filename) {
    FILE* file = fopen(filename, "r");
    if(!file) {
        set_color(12); // Red
        printf("Error: Cannot load from '%s'\n", filename);
        set_color(7); // White
        return;
    }
    
    char line[1024];
    if(fgets(line, sizeof(line), file)) {
        if(strncmp(line, "filesystem_data_v1", 18) == 0) {
            set_color(10); // Green
            printf("Filesystem loaded from '%s'\n", filename);
            set_color(7); // White
        }
    }
    
    fclose(file);
}

// Initialize console
void init_console() {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    // Set console title only; avoid resizing to keep inline behavior stable
    SetConsoleTitle("C File System Terminal");
}

// Clear screen
void clear_screen() {
    COORD coord = {0, 0};
    DWORD count;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X * csbi.dwSize.Y, coord, &count);
    SetConsoleCursorPosition(hConsole, coord);
}

// Set text color
void set_color(int color) {
    SetConsoleTextAttribute(hConsole, color);
}

// Print prompt
void print_prompt() {
    set_color(10); // Green
    printf("C:\\");
    print_path(current_dir);
    set_color(14); // Yellow
    printf("> ");
    set_color(7); // White
}

// mkdir command
void mkdir_command(const char* name) {
    if(strlen(name) == 0) {
        set_color(12); // Red
        printf("The syntax of the command is incorrect.\n");
        set_color(7); // White
        return;
    }
    
    if(find_child(current_dir, name) != NULL) {
        set_color(12); // Red
        printf("A subdirectory or file %s already exists.\n", name);
        set_color(7); // White
        return;
    }
    
    Directory* new_dir = create_directory(name);
    add_child(current_dir, new_dir);
}

// touch command (create file)
void touch_command(const char* name) {
    if(strlen(name) == 0) {
        set_color(12); // Red
        printf("The syntax of the command is incorrect.\n");
        set_color(7); // White
        return;
    }
    
    if(find_file(current_dir, name) != NULL) {
        printf("File already exists: %s\n", name);
        return;
    }
    
    File* new_file = create_file(name);
    add_file(current_dir, new_file);
}

// cd command
void cd_command(const char* path) {
    if(strlen(path) == 0 || strcmp(path, "~") == 0) {
        current_dir = root;
        return;
    }
    
    if(strcmp(path, "..") == 0) {
        if(current_dir != root) {
            current_dir = current_dir->parent;
        }
        return;
    }
    
    Directory* target = find_child(current_dir, path);
    if(target != NULL) {
        current_dir = target;
    } else {
        set_color(12); // Red
        printf("The system cannot find the path specified.\n");
        set_color(7); // White
    }
}

// pwd command
void pwd_command() {
    print_path(current_dir);
    printf("\n");
}

// ls command (dir)
void ls_command() {
    list_directory(current_dir);
}

// cat command (type)
void cat_command(const char* name) {
    File* file = find_file(current_dir, name);
    if(file != NULL) {
        printf("%s\n", file->content);
    } else {
        set_color(12); // Red
        printf("The system cannot find the file specified.\n");
        set_color(7); // White
    }
}

// echo command
void echo_command(const char* text) {
    if(strlen(text) == 0) {
        printf("ECHO is on.\n");
        return;
    }
    printf("%s\n", text);
}

// save command
void save_command(const char* filename) {
    save_filesystem(filename);
}

// load command
void load_command(const char* filename) {
    load_filesystem(filename);
}

// help command
void help_command() {
    set_color(14); // Yellow
    printf("C File System Terminal Commands:\n\n");
    set_color(7); // White
    
    printf("DIR or LS         List directory contents\n");
    printf("CD <directory>    Change directory\n");
    printf("MKDIR <name>      Create directory\n");
    printf("TOUCH <name>      Create file\n");
    printf("TYPE <file>       Display file contents\n");
    printf("ECHO <text>       Display text\n");
    printf("PWD               Print working directory\n");
    printf("CLS or CLEAR      Clear screen\n");
    printf("SAVE <filename>   Save filesystem to file\n");
    printf("LOAD <filename>   Load filesystem from file\n");
    printf("HELP              Show this help\n");
    printf("EXIT or QUIT      Exit terminal\n");
}

// clear command
void clear_command() {
    clear_screen();
}

// Initialize the file system
void init_filesystem() {
    root = create_directory("");
    strcpy(root->name, "");
    current_dir = root;
    
    // Create some default directories
    mkdir_command("Users");
    mkdir_command("Program Files");
    mkdir_command("Windows");
    mkdir_command("Temp");
    
    // Navigate to Users and create user directory
    cd_command("Users");
    mkdir_command("Administrator");
    cd_command("Administrator");
    mkdir_command("Desktop");
    mkdir_command("Documents");
    mkdir_command("Downloads");
}

// Main terminal loop
int main() {
    char input[512];
    char command[256];
    char argument[256];
    
    // Initialize console
    init_console();
    
    // Welcome screen
    set_color(11); // Cyan
    printf("Microsoft Windows [Version 10.0.26100.5074]\n");
    printf("(c) Microsoft Corporation. All rights reserved.\n\n");
    set_color(7); // White
    
    printf("C File System Terminal v1.0\n");
    printf("Type 'help' for commands, 'exit' to quit\n\n");
    
    init_filesystem();
    
    while(1) {
        print_prompt();
        
        // Get input
        if(fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Parse command and argument
        int parsed = sscanf(input, "%s %[^\n]", command, argument);
        
        if(parsed >= 1) {
            // Convert to uppercase for Windows-style commands
            for(int i = 0; command[i]; i++) {
                command[i] = toupper(command[i]);
            }
            
            if(strcmp(command, "EXIT") == 0 || strcmp(command, "QUIT") == 0) {
                break;
            }
            else if(strcmp(command, "HELP") == 0 || strcmp(command, "/?") == 0) {
                help_command();
            }
            else if(strcmp(command, "MKDIR") == 0 || strcmp(command, "MD") == 0) {
                mkdir_command(argument);
            }
            else if(strcmp(command, "TOUCH") == 0) {
                touch_command(argument);
            }
            else if(strcmp(command, "CD") == 0) {
                cd_command(argument);
            }
            else if(strcmp(command, "DIR") == 0 || strcmp(command, "LS") == 0) {
                ls_command();
            }
            else if(strcmp(command, "PWD") == 0) {
                pwd_command();
            }
            else if(strcmp(command, "TYPE") == 0 || strcmp(command, "CAT") == 0) {
                cat_command(argument);
            }
            else if(strcmp(command, "ECHO") == 0) {
                echo_command(argument);
            }
            else if(strcmp(command, "SAVE") == 0) {
                save_command(argument);
            }
            else if(strcmp(command, "LOAD") == 0) {
                load_command(argument);
            }
            else if(strcmp(command, "CLS") == 0 || strcmp(command, "CLEAR") == 0) {
                clear_command();
            }
            else {
                set_color(12); // Red
                printf("'%s' is not recognized as an internal or external command,\n", command);
                printf("operable program or batch file.\n");
                set_color(7); // White
            }
        }
        
        printf("\n");
    }
    
    set_color(14); // Yellow
    printf("Goodbye!\n");
    set_color(7); // White
    
    return 0;
}
