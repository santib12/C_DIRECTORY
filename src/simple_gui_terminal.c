#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>

#define MAX_NAME 256
#define MAX_CHILDREN 100
#define MAX_FILES 100
#define MAX_FILE_SIZE 2048

// Forward declarations
static void get_main_project_dir(char* buffer, size_t size);
static void cmd_sync(void);
static void cmd_writeln(const char* args);
static void cmd_writecode(const char* args);
static void cmd_editcode(const char* args);
static void cmd_adduser(const char* args);

// ---------------- In-memory filesystem ----------------
typedef struct File {
    char name[MAX_NAME];
    char content[MAX_FILE_SIZE];
} File;

typedef struct Directory {
    char name[MAX_NAME];
    struct Directory* parent;
    struct Directory* children[MAX_CHILDREN];
    File* files[MAX_FILES];
    int child_count;
    int file_count;
} Directory;

static Directory* g_root = NULL;
static Directory* g_cwd = NULL;
static Directory* g_home = NULL;
static char g_currentUser[64] = "Public";

// Forward declarations
static void fs_load_from_disk(void);
static void fs_save_to_disk(void);
static void save_filesystem_recursive(Directory* dir, FILE* f, const char* path);
static void join_path(char* out, size_t out_sz, const char* base, const char* name);
static void load_users_from_realfilesystem(void);
static void sync_all_directories(void);
static void sync_directory_recursive(Directory* virtual_dir, const char* real_path);

// Cursor blinking
static UINT_PTR g_cursorTimer = 0;
static BOOL g_cursorVisible = TRUE;

// Edit mode variables
static int g_editMode = 0;
static File* g_editFile = NULL;
static char g_editBuffer[2048];
static int g_editBufferPos = 0;

static Directory* fs_create_dir(const char* name) {
    Directory* d = (Directory*)malloc(sizeof(Directory));
    if (!d) return NULL;
    memset(d, 0, sizeof(*d));
    strncpy(d->name, name, sizeof(d->name)-1);
    return d;
}

static File* fs_create_file(const char* name) {
    File* f = (File*)malloc(sizeof(File));
    if (!f) return NULL;
    memset(f, 0, sizeof(*f));
    strncpy(f->name, name, sizeof(f->name)-1);
    return f;
}

static Directory* fs_find_child(Directory* parent, const char* name) {
    for (int i = 0; i < parent->child_count; ++i) {
        if (_stricmp(parent->children[i]->name, name) == 0) return parent->children[i];
    }
    return NULL;
}

static File* fs_find_file(Directory* parent, const char* name) {
    for (int i = 0; i < parent->file_count; ++i) {
        if (_stricmp(parent->files[i]->name, name) == 0) return parent->files[i];
    }
    return NULL;
}

static void fs_add_child(Directory* parent, Directory* child) {
    if (parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = child;
        child->parent = parent;
    }
}

static void fs_add_file(Directory* parent, File* file) {
    if (parent->file_count < MAX_FILES) {
        parent->files[parent->file_count++] = file;
    }
}

static void fs_print_path(Directory* dir, char* out, size_t out_sz) {
    if (dir == g_root) {
        snprintf(out, out_sz, "C:\\USERS");
        return;
    }
    char tmp[1024] = {0};
    Directory* cur = dir;
    while (cur && cur != g_root) {
        char seg[300];
        snprintf(seg, sizeof(seg), "%s", cur->name);
        if (tmp[0] == '\0') {
            snprintf(tmp, sizeof(tmp), "%s", seg);
        } else {
            char buf[1024];
            snprintf(buf, sizeof(buf), "%s\\%s", seg, tmp);
            strncpy(tmp, buf, sizeof(tmp)-1);
        }
        cur = cur->parent;
    }
    snprintf(out, out_sz, "C:\\USERS\\%s", tmp);
}



static void fs_init(void) {
    g_root = fs_create_dir("");
    Directory* windows = fs_create_dir("Windows");
    Directory* temp = fs_create_dir("Temp");
    fs_add_child(g_root, windows);
    fs_add_child(g_root, temp);
    
    // Create Public and Admin user profiles directly under root
    Directory* public_user = fs_create_dir("Public");
    Directory* admin_user = fs_create_dir("Admin");
    
    fs_add_child(g_root, public_user);
    fs_add_child(g_root, admin_user);
    
    // Create some default directories for each user
    Directory* public_docs = fs_create_dir("Documents");
    Directory* public_desktop = fs_create_dir("Desktop");
    Directory* public_downloads = fs_create_dir("Downloads");
    
    Directory* admin_docs = fs_create_dir("Documents");
    Directory* admin_desktop = fs_create_dir("Desktop");
    Directory* admin_downloads = fs_create_dir("Downloads");
    Directory* admin_system = fs_create_dir("System");
    
    fs_add_child(public_user, public_docs);
    fs_add_child(public_user, public_desktop);
    fs_add_child(public_user, public_downloads);
    
    fs_add_child(admin_user, admin_docs);
    fs_add_child(admin_user, admin_desktop);
    fs_add_child(admin_user, admin_downloads);
    fs_add_child(admin_user, admin_system);
    
    // README.txt files will be added by fs_load_from_disk() if needed
    
    // Set default user to Public
    g_home = public_user;
    g_cwd = g_home;
    
    // Try to load existing filesystem first
    fs_load_from_disk();
    
    // Load any additional users from real filesystem
    load_users_from_realfilesystem();
    
    // Comprehensive auto-sync on startup to load all directories and files from real filesystem
    sync_all_directories();
}

// ---------------- GUI helpers ----------------
static HWND g_hOut = NULL;
static HFONT g_hMono = NULL;
static HBRUSH g_hbrBlack = NULL;
static WNDPROC g_OutputPrevProc = NULL;
static int g_inputStart = 0;

static void gui_append(const char* text) {
    int len = GetWindowTextLengthA(g_hOut);
    SendMessageA(g_hOut, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(g_hOut, EM_REPLACESEL, FALSE, (LPARAM)text);
}

static void gui_println(const char* text) {
    gui_append(text);
    gui_append("\r\n");
}

static void gui_print_prompt(void) {
    char path[1024];
    fs_print_path(g_cwd, path, sizeof(path));
    char prompt[1200];
    snprintf(prompt, sizeof(prompt), "%s> ", path);
    gui_append(prompt);
}

static void gui_clear(void) {
    SetWindowTextA(g_hOut, "");
}

static void gui_show_prompt_and_arm_input(void) {
    gui_print_prompt();
    g_inputStart = GetWindowTextLengthA(g_hOut);
    SendMessageA(g_hOut, EM_SETSEL, (WPARAM)g_inputStart, (LPARAM)g_inputStart);
    SetFocus(g_hOut);
    
    // Start cursor blinking timer
    if (g_cursorTimer) {
        KillTimer(GetParent(g_hOut), g_cursorTimer);
    }
    g_cursorTimer = SetTimer(GetParent(g_hOut), 1, 500, NULL); // 500ms blink
    
    // Force immediate redraw to show cursor
    InvalidateRect(g_hOut, NULL, TRUE);
    g_cursorVisible = TRUE;
    
    // Force immediate cursor draw
    InvalidateRect(g_hOut, NULL, FALSE);
}

// Load filesystem from disk
static void fs_load_from_disk(void) {
    char fs_file[1024];
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    snprintf(fs_file, sizeof(fs_file), "%s\\data\\filesystem.dat", program_dir);
    
    // Ensure the data directory exists
    char data_dir[1024];
    snprintf(data_dir, sizeof(data_dir), "%s\\data", program_dir);
    CreateDirectoryA(data_dir, NULL);
    
    FILE* f = fopen(fs_file, "r");
    if (!f) {
        // No saved filesystem found, using initialized filesystem
        // Add README.txt files to users if they don't exist
        Directory* public_user = fs_find_child(g_root, "Public");
        Directory* admin_user = fs_find_child(g_root, "Admin");
        
        if (public_user && !fs_find_file(public_user, "README.txt")) {
            File* public_readme = fs_create_file("README.txt");
            if (public_readme) {
                strcpy(public_readme->content, "Welcome to Public Profile!\n\nThis is your personal workspace.\n\nCommon commands:\n- DIR: List files\n- CD: Change directory\n- MKDIR: Create folder\n- TOUCH: Create file\n- WRITE: Write to file\n- TYPE: Read file\n- USER: Switch users\n- WHOAMI: Show current user");
                fs_add_file(public_user, public_readme);
            }
        }
        
        if (admin_user && !fs_find_file(admin_user, "README.txt")) {
            File* admin_readme = fs_create_file("README.txt");
            if (admin_readme) {
                strcpy(admin_readme->content, "Welcome to Admin Profile!\n\nYou have administrative access.\n\nSystem directories:\n- System: System files and configurations\n- Documents: Admin documents\n- Desktop: Admin desktop files\n- Downloads: Downloaded files");
                fs_add_file(admin_user, admin_readme);
            }
        }
        return;
    }
    
    // Loading saved filesystem silently
    
    char line[1024];
    Directory* current_dir = g_root;
    
    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) == 0) continue;
        
        if (strncmp(line, "DIR:", 4) == 0) {
            // Directory entry: DIR:path
            char* path = line + 4;
            if (strcmp(path, "C:\\USERS") == 0) {
                current_dir = g_root;
            } else {
                // Find the directory in the filesystem
                current_dir = g_root;
                char* token = strtok(path + 3, "\\"); // Skip "C:"
                while (token && current_dir) {
                    if (strcmp(token, "USERS") == 0) {
                        // Skip USERS, it's the root
                    } else {
                        current_dir = fs_find_child(current_dir, token);
                    }
                    token = strtok(NULL, "\\");
                }
            }
        } else if (strncmp(line, "FILE:", 5) == 0) {
            // File entry: FILE:name|content
            char* file_info = line + 5;
            char* pipe_pos = strchr(file_info, '|');
            if (pipe_pos) {
                *pipe_pos = 0;
                char* filename = file_info;
                char* content = pipe_pos + 1;
                
                if (current_dir) {
                    File* f = fs_create_file(filename);
                    if (f) {
                        // Unescape content
                        char* src = content;
                        char* dst = f->content;
                        while (*src && (dst - f->content) < sizeof(f->content) - 1) {
                            if (*src == '\\' && *(src + 1) == '|') {
                                *dst++ = '|';
                                src += 2;
                            } else if (*src == '\\' && *(src + 1) == 'n') {
                                *dst++ = '\n';
                                src += 2;
                            } else if (*src == '\\' && *(src + 1) == 'r') {
                                *dst++ = '\r';
                                src += 2;
                            } else {
                                *dst++ = *src++;
                            }
                        }
                        *dst = '\0';
                        fs_add_file(current_dir, f);
                    }
                }
            }
        }
    }
    
    fclose(f);
    gui_println("Filesystem loaded successfully");
}

// Save filesystem to disk
static void fs_save_to_disk(void) {
    char fs_file[1024];
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    snprintf(fs_file, sizeof(fs_file), "%s\\data\\filesystem.dat", program_dir);
    
    // Ensure the data directory exists
    char data_dir[1024];
    snprintf(data_dir, sizeof(data_dir), "%s\\data", program_dir);
    CreateDirectoryA(data_dir, NULL);
    
    FILE* f = fopen(fs_file, "w");
    if (!f) {
        gui_println("Failed to save filesystem");
        return;
    }
    
    // Save the filesystem structure
    save_filesystem_recursive(g_root, f, "C:\\USERS");
    
    fclose(f);
    
}

static void load_users_from_realfilesystem(void) {
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char users_dir[1024];
    snprintf(users_dir, sizeof(users_dir), "%s\\data\\USERS", program_dir);
    
    // Check if USERS directory exists
    DWORD attrs = GetFileAttributesA(users_dir);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return; // No USERS directory
    }
    
    // Find all subdirectories in USERS (these are user directories)
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", users_dir);
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search_path, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        // Skip . and .. entries
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        // Skip if not a directory
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        
        // Check if this user already exists in virtual filesystem
        Directory* existing_user = fs_find_child(g_root, findData.cFileName);
        if (existing_user) {
            continue; // User already exists
        }
        
        // Create new user directory in virtual filesystem
        Directory* new_user = fs_create_dir(findData.cFileName);
        if (!new_user) {
            continue;
        }
        
        // Add user to root directory
        fs_add_child(g_root, new_user);
        
        // Create README.txt for the new user
        File* readme = fs_create_file("README.txt");
        if (readme) {
            snprintf(readme->content, sizeof(readme->content), 
                "Welcome to %s's directory!\n\n"
                "This is your personal workspace.\n"
                "You can create files and folders here.\n\n"
                "Available commands:\n"
                "- MKDIR <name> - Create directory\n"
                "- TOUCH <name> - Create file\n"
                "- WRITE <file> <text> - Write to file\n"
                "- EDITCODE <file> - Interactive code editor\n"
                "- TYPE <file> - View file contents\n"
                "- And many more! Type HELP for full list.", findData.cFileName);
            
            fs_add_file(new_user, readme);
        }
        
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}

static void sync_all_directories(void) {
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char users_dir[1024];
    snprintf(users_dir, sizeof(users_dir), "%s\\data\\USERS", program_dir);
    
    // Check if USERS directory exists
    DWORD attrs = GetFileAttributesA(users_dir);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return; // No USERS directory
    }
    
    // Sync each user directory
    for (int i = 0; i < g_root->child_count; i++) {
        Directory* user_dir = g_root->children[i];
        if (!user_dir) continue;
        
        char user_real_path[1024];
        snprintf(user_real_path, sizeof(user_real_path), "%s\\%s", users_dir, user_dir->name);
        
        // Check if the real user directory exists
        DWORD user_attrs = GetFileAttributesA(user_real_path);
        if (user_attrs != INVALID_FILE_ATTRIBUTES && (user_attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Sync this user's directory recursively
            sync_directory_recursive(user_dir, user_real_path);
        }
    }
    
    // Show success message
    gui_println("Successfully synced directories and files.");
}

static void sync_directory_recursive(Directory* virtual_dir, const char* real_path) {
    if (!virtual_dir || !real_path) return;
    
    // Scan the real directory for files and subdirectories
    char search_path[2048];
    snprintf(search_path, sizeof(search_path), "%s\\*", real_path);
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search_path, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    
    do {
        // Skip . and .. entries
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        // Check if it's a directory
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Check if this directory exists in virtual filesystem
            Directory* existing = fs_find_child(virtual_dir, findData.cFileName);
            if (!existing) {
                // Create the directory in virtual filesystem
                Directory* new_dir = fs_create_dir(findData.cFileName);
                if (new_dir) {
                    fs_add_child(virtual_dir, new_dir);
                }
                existing = new_dir;
            }
            
            // Recursively sync this subdirectory
            if (existing) {
                char subdir_real_path[2048];
                snprintf(subdir_real_path, sizeof(subdir_real_path), "%s\\%s", real_path, findData.cFileName);
                sync_directory_recursive(existing, subdir_real_path);
            }
        } else {
            // It's a file - check if this file exists in virtual filesystem
            File* existing_file = fs_find_file(virtual_dir, findData.cFileName);
            if (!existing_file) {
                // Read the file content from real filesystem
                char full_path[2048];
                snprintf(full_path, sizeof(full_path), "%s\\%s", real_path, findData.cFileName);
                
                HANDLE hFile = CreateFileA(full_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD fileSize = GetFileSize(hFile, NULL);
                    if (fileSize > 0 && fileSize < 10240) { // Only read files smaller than 10KB
                        char* content = (char*)malloc(fileSize + 1);
                        if (content) {
                            DWORD bytesRead;
                            if (ReadFile(hFile, content, fileSize, &bytesRead, NULL)) {
                                content[bytesRead] = '\0';
                                
                                // Create the file in virtual filesystem
                                File* new_file = fs_create_file(findData.cFileName);
                                if (new_file) {
                                    strncpy(new_file->content, content, MAX_FILE_SIZE - 1);
                                    new_file->content[MAX_FILE_SIZE - 1] = '\0';
                                    fs_add_file(virtual_dir, new_file);
                                }
                            }
                            free(content);
                        }
                    }
                    CloseHandle(hFile);
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}

// Helper function to recursively save filesystem
static void save_filesystem_recursive(Directory* dir, FILE* f, const char* path) {
    if (!dir || !f) return;
    
    // Save current directory
    fprintf(f, "DIR:%s\n", path);
    
    // Save files in this directory
    for (int i = 0; i < dir->file_count; ++i) {
        if (dir->files[i]) {
            // Escape pipe characters in content
            char escaped_content[2048];
            char* src = dir->files[i]->content;
            char* dst = escaped_content;
            while (*src && (dst - escaped_content) < sizeof(escaped_content) - 1) {
                if (*src == '|') {
                    *dst++ = '\\';
                    *dst++ = '|';
                } else if (*src == '\n') {
                    *dst++ = '\\';
                    *dst++ = 'n';
                } else if (*src == '\r') {
                    *dst++ = '\\';
                    *dst++ = 'r';
                } else {
                    *dst++ = *src;
                }
                src++;
            }
            *dst = '\0';
            
            fprintf(f, "FILE:%s|%s\n", dir->files[i]->name, escaped_content);
        }
    }
    
    // Recursively save subdirectories
    for (int i = 0; i < dir->child_count; ++i) {
        if (dir->children[i]) {
            char new_path[1024];
            snprintf(new_path, sizeof(new_path), "%s\\%s", path, dir->children[i]->name);
            save_filesystem_recursive(dir->children[i], f, new_path);
        }
    }
}

// ---------------- Command processing ----------------
static void cmd_help(void) {
    gui_println("Commands:");
    gui_println("  DIR, LS               List directory contents");
    gui_println("  CD <dir>|..|~         Change directory");
    gui_println("  MKDIR <name>          Create directory");
    gui_println("  RMDIR <name>          Remove empty directory");
    gui_println("  TOUCH <name>          Create empty file");
    gui_println("  DEL <name>            Delete file (permanent)");
    gui_println("  SOFTDEL <name>        Soft delete (terminal only)");
    gui_println("  SOFTDEL /F <name>     Force delete (both terminal and File Explorer)");
    gui_println("  RESTORE <name>        Restore from trash");
    gui_println("  TRASH                 Show trash contents");
    gui_println("  EMPTYTRASH            Permanently delete trash");
    gui_println("  TYPE <file>           Show file contents");
    gui_println("  WRITE <file> <text>   Replace file content with text");
    gui_println("  WRITELN <file> <text> Write text with line breaks (use \\n)");
    gui_println("  WRITECODE <file> <code> Write code with formatting (use \\n, \\t)");
    gui_println("  EDITCODE <file> Interactive code editor (Ctrl+S to save, Ctrl+C to cancel)");
    gui_println("  APPEND <file> <text>  Append text to file");
    gui_println("  ECHO <text>           Print text");
    gui_println("  PWD                   Print working directory");
    gui_println("  SAVEFS <path>         Save entire filesystem to disk");
    gui_println("  USER <name>           Switch to user");
    gui_println("  ADDUSER <name>        Create new user");
    gui_println("  WHOAMI                Show current user");
    gui_println("  USERS                 List all users");
    gui_println("  FILEVIEW              Show files in filesystem tree structure");
    gui_println("  SYNC                  Sync virtual filesystem with real filesystem");
    gui_println("  SAVE                  Save filesystem to disk");
    gui_println("  CLS, CLEAR            Clear screen");
    gui_println("  EXIT                  Quit");
    gui_println("");
    gui_println("Git Commands (working with system Git):");
    gui_println("  GIT INIT               Initialize a new Git repository");
    gui_println("  GIT CLONE <url>        Clone a repository from URL");
    gui_println("  GIT ADD <file>         Add file to staging area");
    gui_println("  GIT ADD .              Add all files to staging area");
    gui_println("  GIT COMMIT -m <msg>    Commit staged changes with message");
    gui_println("  GIT STATUS             Show repository status");
    gui_println("  GIT LOG                Show commit history");
    gui_println("  GIT DIFF               Show changes between commits");
    gui_println("  GIT BRANCH             List branches");
    gui_println("  GIT BRANCH <name>      Create new branch");
    gui_println("  GIT CHECKOUT <branch>  Switch to branch");
    gui_println("  GIT MERGE <branch>     Merge branch into current");
    gui_println("  GIT PULL               Pull changes from remote");
    gui_println("  GIT PUSH               Push changes to remote");
    gui_println("  GIT REMOTE -v          List remote repositories");
    gui_println("  GIT REMOTE ADD <name> <url>  Add remote repository");
    gui_println("  GIT FETCH              Fetch changes from remote");
    gui_println("  GIT RESET <file>       Unstage file");
    gui_println("  GIT RESET --hard       Reset to last commit");
    gui_println("  GIT STASH              Stash current changes");
    gui_println("  GIT STASH POP          Apply stashed changes");
    gui_println("  GIT TAG <name>         Create a tag");
    gui_println("  GIT TAG -l             List tags");
    gui_println("  GIT CONFIG --global user.name <name>  Set global username");
    gui_println("  GIT CONFIG --global user.email <email>  Set global email");
    gui_println("  GIT CONFIG --list      List Git configuration");
    gui_println("  GIT HELP <command>     Show help for Git command");
    gui_println("  GIT VERSION            Show Git version");
    gui_println("  GIT TEST               Test Git installation");
    gui_println("  GIT PWD                Show where Git operations happen");
}

static void cmd_pwd(void) {
    char path[1024];
    fs_print_path(g_cwd, path, sizeof(path));
    gui_println(path);
}


static void cmd_dir(void) {
    char head[1024];
    fs_print_path(g_cwd, head, sizeof(head));
    char title[1200];
    snprintf(title, sizeof(title), " Directory of %s", head);
    gui_println(title);
    gui_println("");
    for (int i = 0; i < g_cwd->child_count; ++i) {
        char line[400];
        snprintf(line, sizeof(line), "%-24s <DIR>", g_cwd->children[i]->name);
        gui_println(line);
    }
    for (int i = 0; i < g_cwd->file_count; ++i) {
        gui_println(g_cwd->files[i]->name);
    }
}

static void cmd_mkdir(const char* name) {
    if (!name || !*name) { 
        gui_println("The syntax of the command is incorrect."); 
        return; 
    }
    
    // Check if directory already exists in virtual filesystem
    if (fs_find_child(g_cwd, name)) { 
        gui_println("A subdirectory or file already exists."); 
        return; 
    }
    
    // Create the directory in virtual filesystem
    Directory* d = fs_create_dir(name);
    if (d) {
        fs_add_child(g_cwd, d);
        
        // Get the real path based on current directory
        char virtual_path[1024];
        fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
        
        // Get program directory
        char program_dir[1024];
        get_main_project_dir(program_dir, sizeof(program_dir));
        
        // Convert virtual path to real path
        char real_path[1024];
        const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
        if (profiles_pos) {
            snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
        } else {
            snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
        }
        
        // Add the directory name to the path
        char full_real_path[1024];
        join_path(full_real_path, sizeof(full_real_path), real_path, name);
        
        // Create the directory on disk (hidden)
        CreateDirectoryA(full_real_path, NULL);
        
        char msg[256];
        snprintf(msg, sizeof(msg), "Directory '%s' created successfully.", name);
        gui_println(msg);
        
        // Auto-save filesystem
        fs_save_to_disk();
    } else {
        gui_println("Failed to create directory.");
    }
}

static void cmd_touch(const char* name) {
    if (!name || !*name) { gui_println("The syntax of the command is incorrect."); return; }
    if (fs_find_file(g_cwd, name)) { gui_println("File already exists."); return; }
    
    // Create the file in virtual filesystem
    File* f = fs_create_file(name);
    if (f) {
        fs_add_file(g_cwd, f);
        
        // Get the real path based on current directory
        char virtual_path[1024];
        fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
        
        // Convert virtual path to real path using absolute paths
        char program_dir[1024];
        get_main_project_dir(program_dir, sizeof(program_dir));
        
        char real_path[1024];
        const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
        if (profiles_pos) {
            snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
        } else {
            snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
        }
        
        // Add the filename to the path
        char full_real_path[1024];
        join_path(full_real_path, sizeof(full_real_path), real_path, name);
        
        
        // Create empty file on disk (hidden)
        HANDLE hFile = CreateFileA(full_real_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            gui_println("File created successfully in real filesystem.");
        } else {
            gui_println("Failed to create file in real filesystem.");
        }
        
        char msg[256];
        snprintf(msg, sizeof(msg), "File '%s' created successfully.", name);
        gui_println(msg);
        
        // Auto-save filesystem
        fs_save_to_disk();
    }
}

static void cmd_del(const char* name) {
    if (!name || !*name) {
        gui_println("Usage: DEL <filename>");
        gui_println("Example: DEL myfile.txt");
        return;
    }
    
    // Check if file exists in virtual filesystem
    File* f = fs_find_file(g_cwd, name);
    if (!f) {
        gui_println("File not found.");
        return;
    }
    
    // Remove file from virtual filesystem
    for (int i = 0; i < g_cwd->file_count; ++i) {
        if (strcmp(g_cwd->files[i]->name, name) == 0) {
            free(g_cwd->files[i]);
            // Shift remaining files
            for (int j = i; j < g_cwd->file_count - 1; ++j) {
                g_cwd->files[j] = g_cwd->files[j + 1];
            }
            g_cwd->file_count--;
            break;
        }
    }
    
    // Remove file from real filesystem
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char virtual_path[1024];
    fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
    
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    }
    
    char full_real_path[1024];
    join_path(full_real_path, sizeof(full_real_path), real_path, name);
    
    // Delete the file from real filesystem
    BOOL result = DeleteFileA(full_real_path);
    if (result) {
        gui_println("File deleted from both terminal and File Explorer.");
    } else {
        DWORD error = GetLastError();
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to delete file from File Explorer. Error: %lu", error);
        gui_println(error_msg);
    }
    
    // Auto-save filesystem
    fs_save_to_disk();
    gui_println("File deleted successfully.");
}

static void cmd_rmdir(const char* name) {
    if (!name || !*name) {
        gui_println("Usage: RMDIR <foldername>");
        gui_println("Example: RMDIR myfolder");
        return;
    }
    
    // Check if directory exists in virtual filesystem
    Directory* d = fs_find_child(g_cwd, name);
    if (!d) {
        gui_println("Directory not found.");
        return;
    }
    
    // Check if directory is empty
    if (d->child_count > 0 || d->file_count > 0) {
        gui_println("Directory is not empty. Use RMDIR /S to force delete.");
        return;
    }
    
    // Remove directory from virtual filesystem
    for (int i = 0; i < g_cwd->child_count; ++i) {
        if (strcmp(g_cwd->children[i]->name, name) == 0) {
            free(g_cwd->children[i]);
            // Shift remaining directories
            for (int j = i; j < g_cwd->child_count - 1; ++j) {
                g_cwd->children[j] = g_cwd->children[j + 1];
            }
            g_cwd->child_count--;
            break;
        }
    }
    
    // Remove directory from real filesystem
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char virtual_path[1024];
    fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
    
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    }
    
    char full_real_path[1024];
    join_path(full_real_path, sizeof(full_real_path), real_path, name);
    
    // Delete the directory from real filesystem
    BOOL result = RemoveDirectoryA(full_real_path);
    if (result) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Directory deleted from both terminal and File Explorer: %s", full_real_path);
        gui_println(msg);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "Directory deleted from terminal. Real directory deletion failed: %s", full_real_path);
        gui_println(msg);
    }
    
    // Auto-save filesystem
    fs_save_to_disk();
    gui_println("Directory deleted successfully.");
}

// Trash system for soft deletes
typedef struct TrashItem {
    char name[256];
    char path[1024];
    int is_directory;
    char content[4096]; // For files
    struct TrashItem* next;
} TrashItem;

static TrashItem* g_trash = NULL;

static void add_to_trash(const char* name, const char* path, int is_directory, const char* content) {
    TrashItem* item = (TrashItem*)malloc(sizeof(TrashItem));
    if (!item) return;
    
    strncpy(item->name, name, sizeof(item->name) - 1);
    item->name[sizeof(item->name) - 1] = '\0';
    
    strncpy(item->path, path, sizeof(item->path) - 1);
    item->path[sizeof(item->path) - 1] = '\0';
    
    item->is_directory = is_directory;
    
    if (content) {
        strncpy(item->content, content, sizeof(item->content) - 1);
        item->content[sizeof(item->content) - 1] = '\0';
    } else {
        item->content[0] = '\0';
    }
    
    item->next = g_trash;
    g_trash = item;
}

static void cmd_softdel(const char* name) {
    if (!name || !*name) {
        gui_println("Usage: SOFTDEL <filename> or SOFTDEL <foldername>");
        gui_println("         SOFTDEL /S <filename> (soft delete - terminal only)");
        gui_println("Example: SOFTDEL myfile.txt (deletes from both terminal and File Explorer)");
        gui_println("         SOFTDEL /S myfile.txt (soft delete - terminal only)");
        gui_println("Note: Default behavior deletes from both terminal and File Explorer");
        return;
    }
    
    // Check for soft delete flag
    int soft_delete_only = 0;
    if (strncmp(name, "/S ", 3) == 0) {
        soft_delete_only = 1;
        name += 3; // Skip "/S "
        // Skip leading spaces
        while (*name && isspace((unsigned char)*name)) name++;
    }
    
    // Check if it's a file
    File* f = fs_find_file(g_cwd, name);
    if (f) {
        // Get current path for trash
        char current_path[1024];
        fs_print_path(g_cwd, current_path, sizeof(current_path));
        
        if (soft_delete_only) {
            // Add to trash (soft delete - terminal only)
            add_to_trash(name, current_path, 0, f->content);
        } else {
            // Delete from real filesystem
            char program_dir[1024];
            get_main_project_dir(program_dir, sizeof(program_dir));
            
            char virtual_path[1024];
            fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
            
            char real_path[1024];
            const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
            if (profiles_pos) {
                snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
            } else {
                snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
            }
            
            char full_real_path[1024];
            join_path(full_real_path, sizeof(full_real_path), real_path, name);
            
            // Delete the file from real filesystem
            BOOL delete_result = DeleteFileA(full_real_path);
            if (!delete_result) {
                DWORD error = GetLastError();
                char error_msg[512];
                snprintf(error_msg, sizeof(error_msg), "Warning: Failed to delete file from File Explorer. Error: %lu", error);
                gui_println(error_msg);
            }
        }
        
        // Remove file from virtual filesystem
        for (int i = 0; i < g_cwd->file_count; ++i) {
            if (strcmp(g_cwd->files[i]->name, name) == 0) {
                free(g_cwd->files[i]);
                // Shift remaining files
                for (int j = i; j < g_cwd->file_count - 1; ++j) {
                    g_cwd->files[j] = g_cwd->files[j + 1];
                }
                g_cwd->file_count--;
                break;
            }
        }
        
        char msg[256];
        if (soft_delete_only) {
            snprintf(msg, sizeof(msg), "File '%s' moved to trash (soft delete - terminal only).", name);
        } else {
            snprintf(msg, sizeof(msg), "File '%s' deleted from both terminal and File Explorer.", name);
        }
        gui_println(msg);
        
        // Auto-save filesystem
        fs_save_to_disk();
        return;
    }
    
    // Check if it's a directory
    Directory* d = fs_find_child(g_cwd, name);
    if (d) {
        // Get current path for trash
        char current_path[1024];
        fs_print_path(g_cwd, current_path, sizeof(current_path));
        
        if (soft_delete_only) {
            // Add to trash (soft delete - terminal only)
            add_to_trash(name, current_path, 1, NULL);
        } else {
            // Delete from real filesystem
            char program_dir[1024];
            get_main_project_dir(program_dir, sizeof(program_dir));
            
            char virtual_path[1024];
            fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
            
            char real_path[1024];
            const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
            if (profiles_pos) {
                snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
            } else {
                snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
            }
            
            char full_real_path[1024];
            join_path(full_real_path, sizeof(full_real_path), real_path, name);
            
            // Delete the directory from real filesystem
            BOOL delete_result = RemoveDirectoryA(full_real_path);
            if (!delete_result) {
                DWORD error = GetLastError();
                char error_msg[512];
                snprintf(error_msg, sizeof(error_msg), "Warning: Failed to delete directory from File Explorer. Error: %lu", error);
                gui_println(error_msg);
            }
        }
        
        // Remove directory from virtual filesystem
        for (int i = 0; i < g_cwd->child_count; ++i) {
            if (strcmp(g_cwd->children[i]->name, name) == 0) {
                free(g_cwd->children[i]);
                // Shift remaining directories
                for (int j = i; j < g_cwd->child_count - 1; ++j) {
                    g_cwd->children[j] = g_cwd->children[j + 1];
                }
                g_cwd->child_count--;
                break;
            }
        }
        
        char msg[256];
        if (soft_delete_only) {
            snprintf(msg, sizeof(msg), "Directory '%s' moved to trash (soft delete - terminal only).", name);
        } else {
            snprintf(msg, sizeof(msg), "Directory '%s' deleted from both terminal and File Explorer.", name);
        }
        gui_println(msg);
        
        // Auto-save filesystem
        fs_save_to_disk();
        return;
    }
    
    gui_println("File or directory not found.");
}

static void cmd_restore(const char* name) {
    if (!name || !*name) {
        gui_println("Usage: RESTORE <filename> or RESTORE <foldername>");
        gui_println("Example: RESTORE myfile.txt");
        gui_println("Use TRASH to see deleted items");
        return;
    }
    
    // Find item in trash
    TrashItem* prev = NULL;
    TrashItem* current = g_trash;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            // Remove from trash
            if (prev) {
                prev->next = current->next;
            } else {
                g_trash = current->next;
            }
            
            if (current->is_directory) {
                // Restore directory
                Directory* new_dir = fs_create_dir(name);
                if (new_dir) {
                    fs_add_child(g_cwd, new_dir);
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Directory '%s' restored from trash.", name);
                    gui_println(msg);
                }
            } else {
                // Restore file
                File* new_file = fs_create_file(name);
                if (new_file) {
                    strncpy(new_file->content, current->content, sizeof(new_file->content) - 1);
                    new_file->content[sizeof(new_file->content) - 1] = '\0';
                    fs_add_file(g_cwd, new_file);
                    char msg[256];
                    snprintf(msg, sizeof(msg), "File '%s' restored from trash.", name);
                    gui_println(msg);
                }
            }
            
            free(current);
            fs_save_to_disk();
            return;
        }
        prev = current;
        current = current->next;
    }
    
    gui_println("Item not found in trash.");
}

static void cmd_trash(void) {
    gui_println("Trash contents:");
    gui_println("===============");
    
    if (!g_trash) {
        gui_println("Trash is empty.");
        return;
    }
    
    TrashItem* current = g_trash;
    int count = 0;
    
    while (current) {
        char type[16];
        strcpy(type, current->is_directory ? "DIR" : "FILE");
        
        char msg[512];
        snprintf(msg, sizeof(msg), "[%d] %s %s (from %s)", count + 1, type, current->name, current->path);
        gui_println(msg);
        
        current = current->next;
        count++;
    }
    
    char count_msg[256];
    snprintf(count_msg, sizeof(count_msg), "Total items in trash: %d", count);
    gui_println(count_msg);
}

static void cmd_emptytrash(void) {
    gui_println("Emptying trash...");
    
    TrashItem* current = g_trash;
    int count = 0;
    
    while (current) {
        TrashItem* next = current->next;
        free(current);
        current = next;
        count++;
    }
    
    g_trash = NULL;
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Trash emptied. %d items permanently deleted.", count);
    gui_println(msg);
}

static void split_name_and_text(const char* arg, char* outName, size_t name_sz, const char** outText) {
    while (*arg && isspace((unsigned char)*arg)) arg++;
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i + 1 < name_sz) { outName[i++] = *arg++; }
    outName[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;
    *outText = arg;
}

static void cmd_write(const char* args) {
    char name[MAX_NAME]; const char* text = NULL; split_name_and_text(args ? args : "", name, sizeof(name), &text);
    if (name[0] == '\0') { gui_println("Usage: WRITE <file> <text>"); return; }
    File* f = fs_find_file(g_cwd, name);
    if (!f) { f = fs_create_file(name); if (f) fs_add_file(g_cwd, f); }
    if (!f) { gui_println("Out of memory creating file."); return; }
    strncpy(f->content, text ? text : "", sizeof(f->content) - 1);
    f->content[sizeof(f->content) - 1] = '\0';
    
    // Also write to real file system
    char virtual_path[1024];
    fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
    
    // Convert virtual path to real path using absolute paths
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    }
    
    // Add the filename to the path
    char full_real_path[1024];
    join_path(full_real_path, sizeof(full_real_path), real_path, name);
    
    // Write content to real file
    HANDLE hFile = CreateFileA(full_real_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, f->content, strlen(f->content), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    
    gui_println("File written successfully.");
}

static void cmd_writeln(const char* args) {
    char name[MAX_NAME]; 
    const char* text = NULL; 
    split_name_and_text(args ? args : "", name, sizeof(name), &text);
    
    if (name[0] == '\0') { 
        gui_println("Usage: WRITELN <file> <text>"); 
        gui_println("Writes text to file with automatic line breaks."); 
        return; 
    }
    
    if (!text || !*text) { 
        gui_println("Usage: WRITELN <file> <text>"); 
        return; 
    }
    
    // Create or find the file
    File* f = fs_find_file(g_cwd, name);
    if (!f) {
        f = fs_create_file(name);
        if (!f) {
            gui_println("Failed to create file.");
            return;
        }
        fs_add_file(g_cwd, f);
    }
    
    // Process text to add line breaks
    char processed_text[MAX_FILE_SIZE];
    int j = 0;
    for (int i = 0; text[i] && j < MAX_FILE_SIZE - 1; i++) {
        if (text[i] == '\\' && text[i+1] == 'n') {
            processed_text[j++] = '\r';
            processed_text[j++] = '\n';
            i++; // Skip the 'n'
        } else {
            processed_text[j++] = text[i];
        }
    }
    processed_text[j] = '\0';
    
    // Copy processed text to file content
    strncpy(f->content, processed_text, MAX_FILE_SIZE - 1);
    f->content[MAX_FILE_SIZE - 1] = '\0';
    
    // Save to real filesystem
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char virtual_path[1024];
    fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
    
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    }
    
    char full_real_path[1024];
    join_path(full_real_path, sizeof(full_real_path), real_path, name);
    
    HANDLE hFile = CreateFileA(full_real_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, f->content, strlen(f->content), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    
    gui_println("File written with line breaks successfully.");
}

static void cmd_writecode(const char* args) {
    char name[MAX_NAME]; 
    const char* text = NULL; 
    split_name_and_text(args ? args : "", name, sizeof(name), &text);
    
    if (name[0] == '\0') { 
        gui_println("Usage: WRITECODE <file> <code>"); 
        gui_println("Writes code to file with proper formatting and syntax highlighting."); 
        return; 
    }
    
    if (!text || !*text) { 
        gui_println("Usage: WRITECODE <file> <code>"); 
        return; 
    }
    
    // Create or find the file
    File* f = fs_create_file(name);
    if (!f) {
        gui_println("Failed to create file.");
        return;
    }
    fs_add_file(g_cwd, f);
    
    // Process code text
    char processed_text[MAX_FILE_SIZE];
    int j = 0;
    for (int i = 0; text[i] && j < MAX_FILE_SIZE - 1; i++) {
        if (text[i] == '\\' && text[i+1] == 'n') {
            processed_text[j++] = '\r';
            processed_text[j++] = '\n';
            i++; // Skip the 'n'
        } else if (text[i] == '\\' && text[i+1] == 't') {
            processed_text[j++] = '\t';
            i++; // Skip the 't'
        } else {
            processed_text[j++] = text[i];
        }
    }
    processed_text[j] = '\0';
    
    // Copy processed text to file content
    strncpy(f->content, processed_text, MAX_FILE_SIZE - 1);
    f->content[MAX_FILE_SIZE - 1] = '\0';
    
    // Save to real filesystem
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char virtual_path[1024];
    fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
    
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    }
    
    char full_real_path[1024];
    join_path(full_real_path, sizeof(full_real_path), real_path, name);
    
    HANDLE hFile = CreateFileA(full_real_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, f->content, strlen(f->content), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    
    gui_println("Code file written successfully.");
}

static void cmd_editcode(const char* args) {
    char name[MAX_NAME];
    if (sscanf(args, "%s", name) != 1) {
        gui_println("Usage: EDITCODE <file>");
        gui_println("Opens interactive code editor. Press Ctrl+S to save and exit.");
        return;
    }
    
    gui_println("=== INTERACTIVE CODE EDITOR ===");
    char file_msg[256];
    snprintf(file_msg, sizeof(file_msg), "File: %s", name);
    gui_println(file_msg);
    gui_println("Type your code line by line. Press Ctrl+S to save and exit.");
    gui_println("Press Ctrl+C to cancel without saving.");
    gui_println("----------------------------------------");
    
    // Create or find the file
    File* f = fs_find_file(g_cwd, name);
    if (!f) {
        f = fs_create_file(name);
        if (!f) {
            gui_println("Failed to create file.");
            return;
        }
        fs_add_file(g_cwd, f);
    }
    
    // Clear existing content
    f->content[0] = '\0';
    
    // Set up interactive editing mode
    g_editMode = 1;
    g_editFile = f;
    g_editBuffer[0] = '\0';
    g_editBufferPos = 0;
    
    gui_println("Ready to edit. Start typing...");
}

static void cmd_adduser(const char* username) {
    if (!username || !*username) {
        gui_println("Usage: ADDUSER <username>");
        gui_println("Creates a new user with the specified name.");
        gui_println("Example: ADDUSER Developer");
        gui_println("         ADDUSER TestUser");
        return;
    }
    
    // Validate username (alphanumeric and underscore only)
    for (const char* p = username; *p; p++) {
        if (!isalnum(*p) && *p != '_') {
            gui_println("Error: Username can only contain letters, numbers, and underscores.");
            return;
        }
    }
    
    // Check if username already exists
    Directory* existing_user = fs_find_child(g_root, username);
    if (existing_user) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Error: User '%s' already exists.", username);
        gui_println(msg);
        return;
    }
    
    // Create new user directory
    Directory* new_user = fs_create_dir(username);
    if (!new_user) {
        gui_println("Error: Failed to create user directory.");
        return;
    }
    
    // Add user to root directory
    fs_add_child(g_root, new_user);
    
    // Create real filesystem directory
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char user_path[1024];
    snprintf(user_path, sizeof(user_path), "%s\\data\\USERS\\%s", program_dir, username);
    
    // Create directory in real filesystem
    if (!CreateDirectoryA(user_path, NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Could not create real directory for user '%s'.", username);
            gui_println(msg);
        }
    }
    
    // Create README.txt for the new user
    File* readme = fs_create_file("README.txt");
    if (readme) {
        snprintf(readme->content, sizeof(readme->content), 
            "Welcome to %s's directory!\n\n"
            "This is your personal workspace.\n"
            "You can create files and folders here.\n\n"
            "Available commands:\n"
            "- MKDIR <name> - Create directory\n"
            "- TOUCH <name> - Create file\n"
            "- WRITE <file> <text> - Write to file\n"
            "- EDITCODE <file> - Interactive code editor\n"
            "- TYPE <file> - View file contents\n"
            "- And many more! Type HELP for full list.", username);
        
        fs_add_file(new_user, readme);
        
        // Save README.txt to real filesystem
        char readme_path[1024];
        snprintf(readme_path, sizeof(readme_path), "%s\\README.txt", user_path);
        
        HANDLE hFile = CreateFileA(readme_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten;
            WriteFile(hFile, readme->content, strlen(readme->content), &bytesWritten, NULL);
            CloseHandle(hFile);
        }
    }
    
    // Save filesystem
    fs_save_to_disk();
    
    char msg[256];
    snprintf(msg, sizeof(msg), "User '%s' created successfully!", username);
    gui_println(msg);
    gui_println("The new user starts with an empty directory and a README.txt file.");
    gui_println("Use 'USER <username>' to switch to the new user.");
}


static void cmd_append(const char* args) {
    char name[MAX_NAME]; const char* text = NULL; split_name_and_text(args ? args : "", name, sizeof(name), &text);
    if (name[0] == '\0') { gui_println("Usage: APPEND <file> <text>"); return; }
    File* f = fs_find_file(g_cwd, name);
    if (!f) { f = fs_create_file(name); if (f) fs_add_file(g_cwd, f); }
    if (!f) { gui_println("Out of memory creating file."); return; }
    size_t cur = strlen(f->content);
    size_t left = (cur < sizeof(f->content)) ? (sizeof(f->content) - 1 - cur) : 0;
    if (left == 0) { gui_println("File is full."); return; }
    strncat(f->content, text ? text : "", left);
    
    // Also append to real file system
    char virtual_path[1024];
    fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
    
    // Convert virtual path to real path using absolute paths
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    }
    
    // Add the filename to the path
    char full_real_path[1024];
    join_path(full_real_path, sizeof(full_real_path), real_path, name);
    
    // Append content to real file
    HANDLE hFile = CreateFileA(full_real_path, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        SetFilePointer(hFile, 0, NULL, FILE_END);
        DWORD bytesWritten;
        WriteFile(hFile, text ? text : "", strlen(text ? text : ""), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    
    gui_println("Text appended successfully.");
}

static void cmd_type(const char* name) {
    if (!name || !*name) { gui_println("The system cannot find the file specified."); return; }
    File* f = fs_find_file(g_cwd, name);
    if (!f) { gui_println("The system cannot find the file specified."); return; }
    
    // Display file content with proper line break handling
    const char* content = f->content;
    const char* start = content;
    
    while (*content) {
        if (*content == '\r' && *(content + 1) == '\n') {
            // Windows line break - print up to this point
            int len = content - start;
            if (len > 0) {
                char line[1024];
                strncpy(line, start, len);
                line[len] = '\0';
                gui_println(line);
            } else {
                gui_println(""); // Empty line
            }
            content += 2; // Skip \r\n
            start = content;
        } else if (*content == '\n') {
            // Unix line break - print up to this point
            int len = content - start;
            if (len > 0) {
                char line[1024];
                strncpy(line, start, len);
                line[len] = '\0';
                gui_println(line);
            } else {
                gui_println(""); // Empty line
            }
            content++; // Skip \n
            start = content;
        } else {
            content++;
        }
    }
    
    // Print the last line if there's any content
    if (content > start) {
        int len = content - start;
        char line[1024];
        strncpy(line, start, len);
        line[len] = '\0';
        gui_println(line);
    }
}

static void cmd_cd(const char* path) {
    if (!path || !*path || strcmp(path, "~") == 0) { g_cwd = g_home ? g_home : g_root; return; }
    if (strcmp(path, "..") == 0) { if (g_cwd != g_root) g_cwd = g_cwd->parent; return; }
    Directory* d = fs_find_child(g_cwd, path);
    if (d) { g_cwd = d; } else { gui_println("The system cannot find the path specified."); }
}

static void cmd_echo(const char* text) { gui_println(text ? text : ""); }

// -------- File system persistence --------
static void join_path(char* out, size_t out_sz, const char* base, const char* name) {
    size_t bl = strlen(base);
    int needs_sep = (bl > 0 && base[bl-1] != '\\' && base[bl-1] != '/');
    if (needs_sep) {
        snprintf(out, out_sz, "%s\\%s", base, name);
    } else {
        snprintf(out, out_sz, "%s%s", base, name);
    }
}

static BOOL mkdir_p(const char* pathIn) {
    if (!pathIn || !*pathIn) return FALSE;
    char* path = _strdup(pathIn);
    if (!path) return FALSE;
    size_t len = strlen(path);
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '/' || path[i] == '\\') {
            char old = path[i]; path[i] = '\0';
            if (*path) { CreateDirectoryA(path, NULL); }
            path[i] = old;
        }
    }
    BOOL ok = CreateDirectoryA(path, NULL);
    if (!ok) {
        DWORD attrs = GetFileAttributesA(path);
        ok = (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
    }
    free(path);
    return ok;
}

static void save_dir_recursive(Directory* dir, const char* base) {
    // Save files in this directory
    for (int i = 0; i < dir->file_count; ++i) {
        char fpath[MAX_PATH];
        join_path(fpath, sizeof(fpath), base, dir->files[i]->name);
        HANDLE h = CreateFileA(fpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD toWrite = (DWORD)strlen(dir->files[i]->content);
            DWORD written = 0;
            if (toWrite > 0) WriteFile(h, dir->files[i]->content, toWrite, &written, NULL);
            CloseHandle(h);
        }
    }
    // Recurse into subdirectories
    for (int i = 0; i < dir->child_count; ++i) {
        char dpath[MAX_PATH];
        join_path(dpath, sizeof(dpath), base, dir->children[i]->name);
        mkdir_p(dpath);
        save_dir_recursive(dir->children[i], dpath);
    }
}

static void cmd_savefs(const char* base) {
    if (!base || !*base) { gui_println("Usage: SAVEFS <path>"); return; }
    if (!mkdir_p(base)) {
        DWORD attrs = GetFileAttributesA(base);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            gui_println("Failed to create target folder.");
            return;
        }
    }
    save_dir_recursive(g_root, base);
    char msg[1024]; snprintf(msg, sizeof(msg), "Filesystem saved to %s", base);
    gui_println(msg);
}


static void cmd_user(const char* username) {
    if (!username || !*username) { 
        gui_println("Usage: USER <username>"); 
        gui_println("Available users:");
        for (int i = 0; i < g_root->child_count; i++) {
            char msg[256];
            snprintf(msg, sizeof(msg), "  %s", g_root->children[i]->name);
            gui_println(msg);
        }
        return; 
    }
    
    Directory* user_dir = fs_find_child(g_root, username);
    if (!user_dir) { 
        char msg[256]; 
        snprintf(msg, sizeof(msg), "Invalid user. Available users:");
        gui_println(msg);
        for (int i = 0; i < g_root->child_count; i++) {
            char user_msg[256];
            snprintf(user_msg, sizeof(user_msg), "  %s", g_root->children[i]->name);
            gui_println(user_msg);
        }
        return; 
    }
    
    strncpy(g_currentUser, username, sizeof(g_currentUser) - 1);
    g_currentUser[sizeof(g_currentUser) - 1] = '\0';
    g_home = user_dir;
    g_cwd = user_dir;
    
    char msg[256]; 
    snprintf(msg, sizeof(msg), "Switched to user: %s", username);
    gui_println(msg);
}

static void cmd_whoami(void) {
    char msg[256]; 
    snprintf(msg, sizeof(msg), "Current user: %s", g_currentUser);
    gui_println(msg);
}

static void cmd_users(void) {
    gui_println("Available users:");
    for (int i = 0; i < g_root->child_count; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "  %s", g_root->children[i]->name);
        gui_println(msg);
    }
    gui_println("");
    gui_println("User profiles are stored in: C:\\USERS\\");
}

// Filesystem tree display functions
static void print_filesystem_tree_recursive(Directory* dir, int level, int is_last, char* prefix) {
    if (!dir) return;
    
    // Print current directory
    char line[512];
    char type = 'D';
    char connector[16];
    
    if (level == 0) {
        // Root directory
        snprintf(line, sizeof(line), "|%s[%c] %s", prefix, type, dir->name);
    } else {
        // Child directory
        if (is_last) {
            strcpy(connector, "└── ");
        } else {
            strcpy(connector, "├── ");
        }
        snprintf(line, sizeof(line), "|%s%s[%c] %s", prefix, connector, type, dir->name);
    }
    gui_println(line);
    
    // Update prefix for children
    char new_prefix[256];
    if (level == 0) {
        strcpy(new_prefix, "");
    } else {
        if (is_last) {
            snprintf(new_prefix, sizeof(new_prefix), "%s    ", prefix);
        } else {
            snprintf(new_prefix, sizeof(new_prefix), "%s│   ", prefix);
        }
    }
    
    // Print files first
    for (int i = 0; i < dir->file_count; i++) {
        char file_line[512];
        char file_connector[16];
        
        // Check if this is the last item (after all directories)
        int is_last_item = (i == dir->file_count - 1) && (dir->child_count == 0);
        
        if (is_last_item) {
            strcpy(file_connector, "└── ");
        } else {
            strcpy(file_connector, "├── ");
        }
        
        snprintf(file_line, sizeof(file_line), "|%s%s[F] %s", new_prefix, file_connector, dir->files[i]->name);
        gui_println(file_line);
    }
    
    // Print subdirectories
    for (int i = 0; i < dir->child_count; i++) {
        int is_last_dir = (i == dir->child_count - 1);
        print_filesystem_tree_recursive(dir->children[i], level + 1, is_last_dir, new_prefix);
    }
}

static void print_filesystem_visualization(Directory* dir, const char* user_name) {
    gui_println("+-------------------------------------------------------------+");
    gui_println("|                    FILESYSTEM TREE STRUCTURE               |");
    gui_println("+-------------------------------------------------------------+");
    
    char user_line[64];
    snprintf(user_line, sizeof(user_line), "|  User: %-50s |", user_name);
    gui_println(user_line);
    
    gui_println("|                                                         |");
    gui_println("|  Directory Structure:                                   |");
    
    if (dir) {
        print_filesystem_tree_recursive(dir, 0, 1, "");
    } else {
        gui_println("|  (No files or directories found)                        |");
    }
    
    gui_println("|                                                         |");
    gui_println("|  Legend: [D] = Directory, [F] = File                     |");
    gui_println("+-------------------------------------------------------------+");
}

static void cmd_fileview(void) {
    // Display filesystem tree visualization
    print_filesystem_visualization(g_cwd, g_currentUser);
}

static void cmd_sync(void) {
    // Syncing virtual filesystem with real filesystem silently
    
    // Get the main project directory
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    // Get the current working directory path in the real file system
    char current_real_dir[2048];
    char current_path[2048];
    
    // Build the real path based on current virtual directory
    if (g_cwd == g_home) {
        // We're in the user's home directory
        snprintf(current_real_dir, sizeof(current_real_dir), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    } else {
        // We're in a subdirectory, build the full path
        fs_print_path(g_cwd, current_path, sizeof(current_path));
        
        // Find the position after "C:\\USERS\\"
        char* profiles_pos = strstr(current_path, "\\USERS\\");
        if (profiles_pos) {
            // Skip "C:\USERS\" and add the rest to the base path
            char* relative_path = profiles_pos + 7; // Skip "\USERS\"
            snprintf(current_real_dir, sizeof(current_real_dir), "%s\\data\\USERS\\%s", program_dir, relative_path);
        } else {
            // Fallback to current user's directory
            snprintf(current_real_dir, sizeof(current_real_dir), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
        }
    }
    
    
    // Scan the real directory and add missing folders to virtual filesystem
    WIN32_FIND_DATAA findData;
    char search_path[2048];
    snprintf(search_path, sizeof(search_path), "%s\\*", current_real_dir);
    
    HANDLE hFind = FindFirstFileA(search_path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Skip . and .. entries
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                continue;
            }
            
            // Check if it's a directory
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Check if this directory exists in virtual filesystem
                Directory* existing = fs_find_child(g_cwd, findData.cFileName);
                if (!existing) {
                    // Create the directory in virtual filesystem
                    Directory* new_dir = fs_create_dir(findData.cFileName);
                    if (new_dir) {
                        fs_add_child(g_cwd, new_dir);
                        // Directory added silently
                    }
                }
            } else {
                // It's a file - check if this file exists in virtual filesystem
                File* existing_file = fs_find_file(g_cwd, findData.cFileName);
                if (!existing_file) {
                    // Read the file content from real filesystem
                    char full_path[2048];
                    snprintf(full_path, sizeof(full_path), "%s\\%s", current_real_dir, findData.cFileName);
                    
                    HANDLE hFile = CreateFileA(full_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD fileSize = GetFileSize(hFile, NULL);
                        if (fileSize > 0 && fileSize < 10240) { // Only read files smaller than 10KB
                            char* content = (char*)malloc(fileSize + 1);
                            if (content) {
                                DWORD bytesRead;
                                if (ReadFile(hFile, content, fileSize, &bytesRead, NULL)) {
                                    content[bytesRead] = '\0';
                                    
                                    // Create the file in virtual filesystem
                                    File* new_file = fs_create_file(findData.cFileName);
                                    if (new_file) {
                                        // Set the file content
                                        strncpy(new_file->content, content, sizeof(new_file->content) - 1);
                                        new_file->content[sizeof(new_file->content) - 1] = '\0';
                                        
                                        fs_add_file(g_cwd, new_file);
                                        char msg[256];
                                        snprintf(msg, sizeof(msg), "Added missing file: %s", findData.cFileName);
                                        gui_println(msg);
                                    }
                                }
                                free(content);
                            }
                        }
                        CloseHandle(hFile);
                    }
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    } else {
        gui_println("No files found in real directory or directory doesn't exist.");
    }
    
    // Save the updated filesystem
    fs_save_to_disk();
    gui_println("Sync completed successfully!");
}

// Helper function to get the main project directory (not the build directory)
static void get_main_project_dir(char* buffer, size_t size) {
    // Get the executable's full path
    GetModuleFileNameA(NULL, buffer, (DWORD)size);
    
    // Find the last backslash and remove the filename
    char* last_slash = strrchr(buffer, '\\');
    if (last_slash) {
        *last_slash = '\0';  // Remove the executable filename (now in build directory)
    }
    
    // Now we're in the build directory, go up one level to the main project directory
    last_slash = strrchr(buffer, '\\');
    if (last_slash) {
        *last_slash = '\0';  // Go up one level from build/ to main project directory
    }
}

// Simple Git command implementations using system calls
static void execute_git_command(const char* git_args) {
    char command[1024];
    snprintf(command, sizeof(command), "git %s", git_args);
    
    // Get the current working directory path in the real file system
    char storage_path[2048];
    char current_path[2048];
    
    // Build the real path based on current virtual directory
    // Get the main project directory (not the build directory)
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    if (g_cwd == g_home) {
        // We're in the user's home directory
        snprintf(storage_path, sizeof(storage_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    } else {
        // We're in a subdirectory, build the full path
        fs_print_path(g_cwd, current_path, sizeof(current_path));
        
        // Convert virtual path to real path
        // Virtual path: C:\USERS\Public\folder\subfolder
        // Real path: [program_dir]\data\USERS\Public\folder\subfolder
        
        // Find the position after "C:\\USERS\\"
        char* profiles_pos = strstr(current_path, "\\USERS\\");
        if (profiles_pos) {
            // Skip "C:\USERS\" and add the rest to the base path
            char* relative_path = profiles_pos + 7; // Skip "\USERS\"
            snprintf(storage_path, sizeof(storage_path), "%s\\data\\USERS\\%s", program_dir, relative_path);
        } else {
            // Fallback to current user's directory
            snprintf(storage_path, sizeof(storage_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
        }
    }
    
    // Create the directories if they don't exist (hidden) - use absolute paths
    char data_dir[1024];
    snprintf(data_dir, sizeof(data_dir), "%s\\data", program_dir);
    CreateDirectoryA(data_dir, NULL);
    
    char users_dir[1024];
    snprintf(users_dir, sizeof(users_dir), "%s\\data\\USERS", program_dir);
    CreateDirectoryA(users_dir, NULL);
    
    char public_dir[1024];
    snprintf(public_dir, sizeof(public_dir), "%s\\data\\USERS\\Public", program_dir);
    CreateDirectoryA(public_dir, NULL);
    
    char admin_dir[1024];
    snprintf(admin_dir, sizeof(admin_dir), "%s\\data\\USERS\\Admin", program_dir);
    CreateDirectoryA(admin_dir, NULL);
    
    // Create Admin's System folder in real filesystem
    char admin_system_dir[1024];
    snprintf(admin_system_dir, sizeof(admin_system_dir), "%s\\data\\USERS\\Admin\\System", program_dir);
    CreateDirectoryA(admin_system_dir, NULL);
    
    
    
    // Create a temporary batch file to capture output (in temp directory)
    char temp_bat[1024];
    snprintf(temp_bat, sizeof(temp_bat), "%s\\data\\temp_git_output.bat", program_dir);
    
    // Create the batch file content with Git isolation using --git-dir and --work-tree
    char batch_content[4096];
    snprintf(batch_content, sizeof(batch_content), 
        "@echo off\n"
        "cd /d \"%s\"\n"
        "if exist .git (\n"
        "    set GIT_PAGER=cat\n"
        "    set GIT_CONFIG_NOSYSTEM=1\n"
        "    git --git-dir=\"%%CD%%\\.git\" --work-tree=\"%%CD%%\" %s\n"
        ") else (\n"
        "    echo No .git folder found. Use 'GIT INIT' to create a repository.\n"
        ")\n"
        "echo.\n"
        "echo [GIT_EXIT_CODE] %%ERRORLEVEL%%\n", 
        storage_path, git_args);
    
    // Write the batch file
    HANDLE hFile = CreateFileA(temp_bat, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, batch_content, strlen(batch_content), &bytesWritten, NULL);
        CloseHandle(hFile);
        
        // Execute the batch file and capture output using CreateProcess (hidden)
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa;
        HANDLE hReadPipe, hWritePipe;
        
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // Hide the window
        
        ZeroMemory(&pi, sizeof(pi));
        
        // Create pipes for input/output
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        if (CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            si.hStdOutput = hWritePipe;
            si.hStdError = hWritePipe;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            
            // Create the process
            if (CreateProcessA(NULL, temp_bat, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(hWritePipe);
                
                // Read output
                char buffer[1024];
                DWORD bytesRead;
                int exit_code = 0;
                
                while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    
                    // Process each line
                    char* line = buffer;
                    char* next_line;
                    while ((next_line = strchr(line, '\n')) != NULL) {
                        *next_line = '\0';
                        
                        // Remove carriage return if present
                        char* cr = strchr(line, '\r');
                        if (cr) *cr = '\0';
                        
                        // Check for exit code marker
                        if (strstr(line, "[GIT_EXIT_CODE]") != NULL) {
                            sscanf(line, "[GIT_EXIT_CODE] %d", &exit_code);
                        } else if (strlen(line) > 0) {
                            // Display the output line
                            gui_println(line);
                        }
                        
                        line = next_line + 1;
                    }
                }
                
                // Wait for process to complete
                WaitForSingleObject(pi.hProcess, INFINITE);
                GetExitCodeProcess(pi.hProcess, (DWORD*)&exit_code);
                
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                CloseHandle(hReadPipe);
                
                if (exit_code == 0) {
                    gui_println("Git command completed successfully.");
                } else {
                    gui_println("Git command failed. Check your Git installation and try again.");
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), "Exit code: %d", exit_code);
                    gui_println(error_msg);
                }
            } else {
                CloseHandle(hReadPipe);
                CloseHandle(hWritePipe);
                gui_println("Failed to execute Git command.");
            }
        } else {
            gui_println("Failed to create pipes for Git command.");
        }
        
        // Clean up the temporary batch file
        DeleteFileA(temp_bat);
        
        // Create .gitignore to ignore temp files if it doesn't exist
        char gitignore_path[1024];
        snprintf(gitignore_path, sizeof(gitignore_path), "%s\\.gitignore", storage_path);
        
        // Check if .gitignore exists
        HANDLE hGitignore = CreateFileA(gitignore_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hGitignore == INVALID_HANDLE_VALUE) {
            // .gitignore doesn't exist, create it
            hGitignore = CreateFileA(gitignore_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hGitignore != INVALID_HANDLE_VALUE) {
                const char* gitignore_content = "temp_git_output.bat\n*.tmp\n";
                DWORD bytesWritten;
                WriteFile(hGitignore, gitignore_content, strlen(gitignore_content), &bytesWritten, NULL);
                CloseHandle(hGitignore);
            }
        } else {
            CloseHandle(hGitignore);
        }
    } else {
        gui_println("Failed to create temporary file for Git command.");
    }
}

static void cmd_git_init(void) {
    gui_println("Initializing Git repository...");
    execute_git_command("init");
}

static void cmd_git_clone(const char* url) {
    if (!url || !*url) {
        gui_println("Usage: GIT CLONE <url>");
        gui_println("Example: GIT CLONE https://github.com/user/repo.git");
        return;
    }
    
    gui_println("Cloning repository...");
    gui_println("Note: This will clone to your actual file system.");
    
    // Get the main project directory
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    // Get the current working directory path in the real file system
    char storage_path[2048];
    char current_path[2048];
    
    // Always clone to the user's home directory for consistency
    snprintf(storage_path, sizeof(storage_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    
    // Create a temporary batch file for git clone (no .git folder needed)
    char temp_bat[1024];
    snprintf(temp_bat, sizeof(temp_bat), "%s\\data\\temp_git_clone.bat", program_dir);
    
    // Create the batch file content for git clone
    char batch_content[4096];
    snprintf(batch_content, sizeof(batch_content), 
        "@echo off\n"
        "cd /d \"%s\"\n"
        "git clone \"%s\"\n"
        "echo.\n"
        "echo [GIT_EXIT_CODE] %%ERRORLEVEL%%\n", 
        storage_path, url);
    
    
    // Write the batch file
    HANDLE hFile = CreateFileA(temp_bat, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, batch_content, (DWORD)strlen(batch_content), &bytesWritten, NULL);
        CloseHandle(hFile);
        
        // Execute the batch file
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        if (CreateProcessA(temp_bat, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        // Clean up the batch file
        DeleteFileA(temp_bat);
    }
    
    // Extract repository name from URL for virtual filesystem
    char repo_name[256] = {0};
    const char* last_slash = strrchr(url, '/');
    if (last_slash) {
        strcpy(repo_name, last_slash + 1);
        // Remove .git extension if present
        char* dot_git = strstr(repo_name, ".git");
        if (dot_git) {
            *dot_git = '\0';
        }
    }
    
    // Create the repository folder in virtual filesystem
    if (strlen(repo_name) > 0) {
        Directory* repo_dir = fs_create_dir(repo_name);
        if (repo_dir) {
            fs_add_child(g_cwd, repo_dir);
            char msg[256];
            snprintf(msg, sizeof(msg), "Repository '%s' added to virtual filesystem.", repo_name);
            gui_println(msg);
            
            // Auto-save filesystem
            fs_save_to_disk();
        }
    }
    
    // Auto-sync to update virtual filesystem with cloned repository
    cmd_sync();
}

static void cmd_git_add(const char* file) {
    if (!file || !*file) {
        gui_println("Usage: GIT ADD <file> or GIT ADD .");
        gui_println("Example: GIT ADD . (adds all files)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "add %s", file);
    execute_git_command(command);
}

static void cmd_git_commit(const char* message) {
    if (!message || !*message) {
        gui_println("Usage: GIT COMMIT -m <message>");
        gui_println("Example: GIT COMMIT -m \"Initial commit\"");
        return;
    }
    
    // Handle both "GIT COMMIT -m message" and "GIT COMMIT message" formats
    char command[1024];
    if (strncmp(message, "-m ", 3) == 0) {
        // Already has -m flag
        snprintf(command, sizeof(command), "commit %s", message);
    } else {
        // Add -m flag
        snprintf(command, sizeof(command), "commit -m \"%s\"", message);
    }
    execute_git_command(command);
}

static void cmd_git_status(void) {
    gui_println("Git Status:");
    execute_git_command("status");
}

static void cmd_git_log(void) {
    gui_println("Git Log:");
    execute_git_command("log --oneline -10");
    gui_println("Remote commits:");
    execute_git_command("log --oneline origin/main -5");
}

static void cmd_git_diff(void) {
    gui_println("Git Diff:");
    execute_git_command("diff");
}

static void cmd_git_branch(const char* name) {
    if (!name || !*name) {
        gui_println("Git Branches:");
        execute_git_command("branch");
    } else {
        char command[1024];
        snprintf(command, sizeof(command), "branch %s", name);
        execute_git_command(command);
    }
}

static void cmd_git_remote(const char* args) {
    if (!args || !*args) {
        gui_println("Git Remotes:");
        execute_git_command("remote -v");
    } else {
        char command[1024];
        snprintf(command, sizeof(command), "remote %s", args);
        execute_git_command(command);
    }
}

static void cmd_git_checkout(const char* branch) {
    if (!branch || !*branch) {
        gui_println("Usage: GIT CHECKOUT <branch>");
        gui_println("Example: GIT CHECKOUT main");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "checkout %s", branch);
    execute_git_command(command);
    // Auto-sync to update virtual filesystem
    cmd_sync();
}

static void cmd_git_merge(const char* branch) {
    if (!branch || !*branch) {
        gui_println("Usage: GIT MERGE <branch>");
        gui_println("Example: GIT MERGE feature-branch");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "merge %s", branch);
    execute_git_command(command);
    // Auto-sync to update virtual filesystem
    cmd_sync();
}

static void cmd_git_pull(void) {
    gui_println("Pulling changes from remote...");
    gui_println("Checking remote configuration...");
    execute_git_command("remote -v");
    gui_println("Checking current branch...");
    execute_git_command("branch");
    gui_println("Checking status before pull...");
    execute_git_command("status");
    gui_println("Executing git pull...");
    execute_git_command("pull");
    gui_println("Checking status after pull...");
    execute_git_command("status");
    // Auto-sync to update virtual filesystem
    cmd_sync();
}

static void cmd_git_push(void) {
    gui_println("Pushing changes to remote...");
    gui_println("Checking commits ahead of remote...");
    execute_git_command("log --oneline origin/main..HEAD");
    gui_println("Executing git push...");
    execute_git_command("push");
}


static void cmd_git_fetch(void) {
    gui_println("Fetching changes from remote...");
    execute_git_command("fetch");
}

static void cmd_git_reset(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: GIT RESET <file> or GIT RESET --hard");
        gui_println("Example: GIT RESET --hard (reset all changes)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "reset %s", args);
    execute_git_command(command);
}

static void cmd_git_rm(const char* file) {
    if (!file || !*file) {
        gui_println("Usage: GIT RM <file> or GIT RM -r <folder>");
        gui_println("Example: GIT RM file.txt (remove file from Git)");
        gui_println("Example: GIT RM -r folder/ (remove folder from Git)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "rm %s", file);
    execute_git_command(command);
    // Auto-sync to update virtual filesystem
    cmd_sync();
}

static void cmd_git_clean(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: GIT CLEAN -f (remove untracked files)");
        gui_println("Usage: GIT CLEAN -fd (remove untracked files and directories)");
        gui_println("Example: GIT CLEAN -fd (remove all untracked files and folders)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "clean %s", args);
    execute_git_command(command);
    // Auto-sync to update virtual filesystem
    cmd_sync();
}

static void cmd_git_stash(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: GIT STASH or GIT STASH POP");
        gui_println("Example: GIT STASH (stash changes)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "stash %s", args);
    execute_git_command(command);
}

static void cmd_git_tag(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: GIT TAG <name> or GIT TAG -l");
        gui_println("Example: GIT TAG -l (list tags)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "tag %s", args);
    execute_git_command(command);
}

static void cmd_git_config(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: GIT CONFIG --global user.name <name> or GIT CONFIG --list");
        gui_println("Example: GIT CONFIG --list (show all config)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "config %s", args);
    execute_git_command(command);
}

static void cmd_git_help(const char* command) {
    if (!command || !*command) {
        gui_println("Available Git commands:");
        gui_println("  INIT, CLONE, ADD, COMMIT, STATUS, LOG, DIFF");
        gui_println("  BRANCH, CHECKOUT, MERGE, PULL, PUSH, REMOTE");
        gui_println("  FETCH, RESET, RM, CLEAN, STASH, TAG, CONFIG, VERSION");
        gui_println("Type 'HELP' for detailed command descriptions.");
        return;
    }
    
    char git_help_cmd[1024];
    snprintf(git_help_cmd, sizeof(git_help_cmd), "help %s", command);
    execute_git_command(git_help_cmd);
}

static void cmd_git_version(void) {
    gui_println("Git Version:");
    execute_git_command("--version");
}

// Test command to verify Git is working
static void cmd_git_test(void) {
    gui_println("Testing Git installation...");
    gui_println("Running: git --version");
    
    int result = system("git --version");
    
    if (result == 0) {
        gui_println("Git is working correctly!");
    } else {
        gui_println("Git is not installed or not in PATH.");
        gui_println("Please install Git from https://git-scm.com/");
    }
}

// Show where Git operations happen
static void cmd_git_pwd(void) {
    gui_println("Git operations happen in the real Windows file system.");
    gui_println("Current working directory for Git commands:");
    
    // Get current working directory
    char cwd[1024];
    if (GetCurrentDirectoryA(sizeof(cwd), cwd) != 0) {
        gui_println(cwd);
    } else {
        gui_println("Unable to get current directory");
    }
    
    gui_println("");
    gui_println("Terminal virtual filesystem path:");
    char path[1024];
    fs_print_path(g_cwd, path, sizeof(path));
    gui_println(path);
}

static int parse_first_token(char* line, char** arg_out) {
    while (*line && isspace((unsigned char)*line)) line++;
    if (!*line) { *arg_out = line; return 0; }
    char* p = line;
    while (*p && !isspace((unsigned char)*p)) { *p = (char)toupper((unsigned char)*p); p++; }
    int has_cmd = (*line != '\0');
    if (*p) { *p++ = '\0'; }
    while (*p && isspace((unsigned char)*p)) p++;
    *arg_out = p;
    return has_cmd ? 1 : 0;
}

static BOOL process_command(char* input) {
    char* arg = NULL;
    if (!parse_first_token(input, &arg)) return TRUE;
    if (strcmp(input, "HELP") == 0) { cmd_help(); }
    else if (strcmp(input, "PWD") == 0) { cmd_pwd(); }
    else if (strcmp(input, "DIR") == 0 || strcmp(input, "LS") == 0) { cmd_dir(); }
    else if (strcmp(input, "MKDIR") == 0 || strcmp(input, "MD") == 0) { cmd_mkdir(arg); }
    else if (strcmp(input, "TOUCH") == 0) { cmd_touch(arg); }
    else if (strcmp(input, "DEL") == 0 || strcmp(input, "DELETE") == 0) { cmd_del(arg); }
    else if (strcmp(input, "RMDIR") == 0 || strcmp(input, "RD") == 0) { cmd_rmdir(arg); }
    else if (strcmp(input, "SOFTDEL") == 0) { cmd_softdel(arg); }
    else if (strcmp(input, "RESTORE") == 0) { cmd_restore(arg); }
    else if (strcmp(input, "TRASH") == 0) { cmd_trash(); }
    else if (strcmp(input, "EMPTYTRASH") == 0) { cmd_emptytrash(); }
    else if (strcmp(input, "TYPE") == 0 || strcmp(input, "CAT") == 0) { cmd_type(arg); }
    else if (strcmp(input, "WRITE") == 0) { cmd_write(arg); }
    else if (strcmp(input, "WRITELN") == 0) { cmd_writeln(arg); }
    else if (strcmp(input, "WRITECODE") == 0) { cmd_writecode(arg); }
    else if (strcmp(input, "EDITCODE") == 0) { cmd_editcode(arg); }
    else if (strcmp(input, "APPEND") == 0) { cmd_append(arg); }
    else if (strcmp(input, "CD") == 0) { cmd_cd(arg); }
    else if (strcmp(input, "ECHO") == 0) { cmd_echo(arg); }
    else if (strcmp(input, "SAVEFS") == 0) { cmd_savefs(arg); }
    else if (strcmp(input, "USER") == 0) { cmd_user(arg); }
    else if (strcmp(input, "ADDUSER") == 0) { cmd_adduser(arg); }
    else if (strcmp(input, "WHOAMI") == 0) { cmd_whoami(); }
    else if (strcmp(input, "USERS") == 0) { cmd_users(); }
    else if (strcmp(input, "FILEVIEW") == 0) { cmd_fileview(); }
    else if (strcmp(input, "SYNC") == 0) { cmd_sync(); }
    else if (strcmp(input, "SAVE") == 0) { fs_save_to_disk(); }
    else if (strcmp(input, "GIT") == 0) {
        // Simple Git command handling
        if (!arg || !*arg) {
            gui_println("Usage: GIT <command> [options]");
            gui_println("Type 'HELP' to see all available Git commands.");
            return TRUE;
        }
        
        // Parse the Git command and arguments properly
        char git_cmd[64];
        char* git_args = NULL;
        
        // Find the first space to separate command from arguments
        char* space_pos = strchr(arg, ' ');
        if (space_pos) {
            // Copy command part (before first space)
            size_t cmd_len = space_pos - arg;
            if (cmd_len >= sizeof(git_cmd)) cmd_len = sizeof(git_cmd) - 1;
            strncpy(git_cmd, arg, cmd_len);
            git_cmd[cmd_len] = '\0';
            
            // Get arguments part (after first space)
            git_args = space_pos + 1;
            while (*git_args && isspace(*git_args)) git_args++;
        } else {
            // No arguments, just the command
            strncpy(git_cmd, arg, sizeof(git_cmd) - 1);
            git_cmd[sizeof(git_cmd) - 1] = '\0';
        }
        
        // Convert command to uppercase for comparison
        for (char* p = git_cmd; *p; p++) {
            *p = toupper(*p);
        }
        
        if (strcmp(git_cmd, "INIT") == 0) { cmd_git_init(); }
        else if (strcmp(git_cmd, "CLONE") == 0) { cmd_git_clone(git_args); }
        else if (strcmp(git_cmd, "ADD") == 0) { cmd_git_add(git_args); }
        else if (strcmp(git_cmd, "COMMIT") == 0) { cmd_git_commit(git_args); }
        else if (strcmp(git_cmd, "STATUS") == 0) { cmd_git_status(); }
        else if (strcmp(git_cmd, "LOG") == 0) { cmd_git_log(); }
        else if (strcmp(git_cmd, "DIFF") == 0) { cmd_git_diff(); }
        else if (strcmp(git_cmd, "BRANCH") == 0) { cmd_git_branch(git_args); }
        else if (strcmp(git_cmd, "REMOTE") == 0) { cmd_git_remote(git_args); }
        else if (strcmp(git_cmd, "CHECKOUT") == 0) { cmd_git_checkout(git_args); }
        else if (strcmp(git_cmd, "MERGE") == 0) { cmd_git_merge(git_args); }
        else if (strcmp(git_cmd, "PULL") == 0) { cmd_git_pull(); }
        else if (strcmp(git_cmd, "PUSH") == 0) { cmd_git_push(); }
        else if (strcmp(git_cmd, "FETCH") == 0) { cmd_git_fetch(); }
        else if (strcmp(git_cmd, "RESET") == 0) { cmd_git_reset(git_args); }
        else if (strcmp(git_cmd, "RM") == 0) { cmd_git_rm(git_args); }
        else if (strcmp(git_cmd, "CLEAN") == 0) { cmd_git_clean(git_args); }
        else if (strcmp(git_cmd, "STASH") == 0) { cmd_git_stash(git_args); }
        else if (strcmp(git_cmd, "TAG") == 0) { cmd_git_tag(git_args); }
        else if (strcmp(git_cmd, "CONFIG") == 0) { cmd_git_config(git_args); }
        else if (strcmp(git_cmd, "HELP") == 0) { cmd_git_help(git_args); }
        else if (strcmp(git_cmd, "VERSION") == 0) { cmd_git_version(); }
        else if (strcmp(git_cmd, "TEST") == 0) { cmd_git_test(); }
        else if (strcmp(git_cmd, "PWD") == 0) { cmd_git_pwd(); }
        else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Git command '%s' not recognized.", git_cmd);
            gui_println(msg);
            gui_println("Type 'HELP' to see all available Git commands.");
        }
    }
    else if (strcmp(input, "CLS") == 0 || strcmp(input, "CLEAR") == 0) { gui_clear(); }
    else if (strcmp(input, "EXIT") == 0 || strcmp(input, "QUIT") == 0) { return FALSE; }
    else { gui_println("'COMMAND' is not recognized."); }
    return TRUE;
}

// Simplified output edit procedure
static LRESULT CALLBACK OutputEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
        case WM_PAINT: {
            // Let default paint happen first
            LRESULT result = CallWindowProcA(g_OutputPrevProc, hWnd, msg, wParam, lParam);
            
            // Draw blinking cursor after default painting
            if (g_cursorVisible) {
                HDC hdc = GetDC(hWnd);
                
                // Get current selection position
                DWORD start = 0, end = 0;
                SendMessageA(hWnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
                
                // Get character position at selection
                POINT pt;
                pt.x = LOWORD(SendMessageA(hWnd, EM_POSFROMCHAR, (WPARAM)start, 0));
                pt.y = HIWORD(SendMessageA(hWnd, EM_POSFROMCHAR, (WPARAM)start, 0));
                
                // If position is still (0,0), try to calculate it manually
                if (pt.x == 0 && pt.y == 0) {
                    // Get text metrics
                    TEXTMETRICA tm;
                    GetTextMetricsA(hdc, &tm);
                    
                    // Calculate position based on text length
                    int textLen = GetWindowTextLengthA(hWnd);
                    int charsPerLine = 80; // Approximate
                    pt.x = (textLen % charsPerLine) * tm.tmAveCharWidth + 8; // 8px margin
                    pt.y = (textLen / charsPerLine) * tm.tmHeight + 8; // 8px margin
                }
                
                // Set text color to bright white for cursor
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                
                // Draw blinking cursor
                HFONT oldFont = (HFONT)SelectObject(hdc, g_hMono);
                TextOutA(hdc, pt.x, pt.y, "|", 1);
                SelectObject(hdc, oldFont);
                
                ReleaseDC(hWnd, hdc);
            }
            return result;
        }
        case WM_SETFOCUS: {
            int len = GetWindowTextLengthA(hWnd);
            SendMessageA(hWnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            int len = GetWindowTextLengthA(hWnd);
            SendMessageA(hWnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            return 0;
        }
        case WM_KEYDOWN: {
            DWORD start = 0, end = 0;
            SendMessageA(hWnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            
            if ((int)start < g_inputStart || (int)end < g_inputStart) {
                SendMessageA(hWnd, EM_SETSEL, (WPARAM)g_inputStart, (LPARAM)g_inputStart);
                return 0;
            }
            
            if (wParam == VK_BACK) {
                if ((int)start <= g_inputStart) return 0;
            }
            if (wParam == VK_LEFT) {
                if ((int)start <= g_inputStart) {
                    SendMessageA(hWnd, EM_SETSEL, (WPARAM)g_inputStart, (LPARAM)g_inputStart);
                    return 0;
                }
            }
            if (wParam == VK_HOME) {
                SendMessageA(hWnd, EM_SETSEL, (WPARAM)g_inputStart, (LPARAM)g_inputStart);
                return 0;
            }
            return CallWindowProcA(g_OutputPrevProc, hWnd, msg, wParam, lParam);
        }
        case WM_CHAR: {
            // Handle edit mode
            if (g_editMode) {
                if (wParam == 19) { // Ctrl+S (save)
                    // Get the current text from the edit control
                    int totalLen = GetWindowTextLengthA(hWnd);
                    char* all = (char*)malloc((size_t)totalLen + 1);
                    if (all) {
                        GetWindowTextA(hWnd, all, totalLen + 1);
                        
                        // Find the content after the edit mode start
                        char* editStart = strstr(all, "Ready to edit. Start typing...");
                        if (editStart) {
                            editStart = strstr(editStart, "\r\n");
                            if (editStart) {
                                editStart += 2; // Skip \r\n
                                
                                // Copy the edited content
                                strncpy(g_editFile->content, editStart, MAX_FILE_SIZE - 1);
                                g_editFile->content[MAX_FILE_SIZE - 1] = '\0';
                                
                                // Save to real filesystem
                                char program_dir[1024];
                                get_main_project_dir(program_dir, sizeof(program_dir));
                                
                                char virtual_path[1024];
                                fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
                                
                                char real_path[1024];
                                const char* profiles_pos = strstr(virtual_path, "\\USERS\\");
                                if (profiles_pos) {
                                    snprintf(real_path, sizeof(real_path), "%s\\data%s", program_dir, profiles_pos);
                                } else {
                                    snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
                                }
                                
                                char full_real_path[1024];
                                join_path(full_real_path, sizeof(full_real_path), real_path, g_editFile->name);
                                
                                HANDLE hFile = CreateFileA(full_real_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                                if (hFile != INVALID_HANDLE_VALUE) {
                                    DWORD bytesWritten;
                                    WriteFile(hFile, g_editFile->content, strlen(g_editFile->content), &bytesWritten, NULL);
                                    CloseHandle(hFile);
                                }
                                
                                gui_println("File saved successfully!");
                            }
                        }
                        free(all);
                    }
                    
                    g_editMode = 0;
                    g_editFile = NULL;
                    gui_show_prompt_and_arm_input();
                    return 0;
                } else if (wParam == 3) { // Ctrl+C (cancel)
                    gui_println("Edit cancelled.");
                    g_editMode = 0;
                    g_editFile = NULL;
                    gui_show_prompt_and_arm_input();
                    return 0;
                }
                // For all other characters, let the normal display handle them
            }
            
            // Normal command mode
            if (wParam == '\r' || wParam == '\n') {
                int totalLen = GetWindowTextLengthA(hWnd);
                int cmdLen = totalLen - g_inputStart;
                if (cmdLen < 0) cmdLen = 0;
                char buf[1024];
                int copyLen = cmdLen < (int)sizeof(buf) - 1 ? cmdLen : (int)sizeof(buf) - 1;
                if (copyLen > 0) {
                    char* all = (char*)malloc((size_t)totalLen + 1);
                    if (!all) return 0;
                    GetWindowTextA(hWnd, all, totalLen + 1);
                    memcpy(buf, all + g_inputStart, (size_t)copyLen);
                    buf[copyLen] = '\0';
                    free(all);
                } else {
                    buf[0] = '\0';
                }

                gui_append("\r\n");
                if (!process_command(buf)) {
                    PostMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
                    return 0;
                }
                gui_show_prompt_and_arm_input();
                return 0;
            }
            
            DWORD start = 0, end = 0;
            SendMessageA(hWnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            if ((int)start < g_inputStart) {
                SendMessageA(hWnd, EM_SETSEL, (WPARAM)g_inputStart, (LPARAM)g_inputStart);
                return 0;
            }
            
            return CallWindowProcA(g_OutputPrevProc, hWnd, msg, wParam, lParam);
        }
    }
    return CallWindowProcA(g_OutputPrevProc, hWnd, msg, wParam, lParam);
}

static void create_child_controls(HWND hWnd) {
    DWORD outStyle = WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP;
    g_hOut = CreateWindowExA(0, "EDIT", "", outStyle, 8, 8, 800, 500, hWnd, (HMENU)1001, GetModuleHandle(NULL), NULL);

    g_hMono = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, "Consolas");
    SendMessageA(g_hOut, WM_SETFONT, (WPARAM)g_hMono, TRUE);

    g_OutputPrevProc = (WNDPROC)SetWindowLongPtrA(g_hOut, GWLP_WNDPROC, (LONG_PTR)OutputEditProc);
    SetFocus(g_hOut);
}

static void layout_children(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    int pad = 8;
    MoveWindow(g_hOut, pad, pad, rc.right - 2*pad, rc.bottom - 2*pad, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(0,255,0));
            SetBkColor(hdc, RGB(0,0,0));
            if (!g_hbrBlack) g_hbrBlack = CreateSolidBrush(RGB(0,0,0));
            return (LRESULT)g_hbrBlack;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc; GetClientRect(hWnd, &rc);
            if (!g_hbrBlack) g_hbrBlack = CreateSolidBrush(RGB(0,0,0));
            FillRect(hdc, &rc, g_hbrBlack);
            return 1;
        }
        case WM_CREATE:
            create_child_controls(hWnd);
            fs_init();
            layout_children(hWnd);
            gui_show_prompt_and_arm_input();
            // Force initial cursor draw
            InvalidateRect(g_hOut, NULL, FALSE);
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE && g_hOut) SetFocus(g_hOut);
            return 0;
        case WM_SIZE:
            layout_children(hWnd);
            return 0;
        case WM_TIMER:
            if (wParam == 1) { // Cursor blink timer
                g_cursorVisible = !g_cursorVisible;
                InvalidateRect(g_hOut, NULL, FALSE);
            }
            return 0;
        case WM_DESTROY:
            // Auto-save filesystem before closing
            fs_save_to_disk();
            
            if (g_cursorTimer) {
                KillTimer(hWnd, g_cursorTimer);
                g_cursorTimer = 0;
            }
            if (g_hbrBlack) { DeleteObject(g_hbrBlack); g_hbrBlack = NULL; }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (!g_hbrBlack) g_hbrBlack = CreateSolidBrush(RGB(0,0,0));
    wc.hbrBackground = g_hbrBlack;
    wc.lpszClassName = "SimpleGuiTermClass";
    if (!RegisterClassA(&wc)) return 1;

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "C File System Terminal", WS_OVERLAPPEDWINDOW, 100, 100, 800, 500, NULL, NULL, hInst, NULL);
    if (!hWnd) return 1;
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);
    BringWindowToTop(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

