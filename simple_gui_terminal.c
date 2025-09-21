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
static void join_path(char* out, size_t out_sz, const char* base, const char* name);

// Cursor blinking
static UINT_PTR g_cursorTimer = 0;
static BOOL g_cursorVisible = TRUE;

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
        snprintf(out, out_sz, "C:\\");
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
    snprintf(out, out_sz, "C:\\%s", tmp);
}



static void fs_init(void) {
    g_root = fs_create_dir("");
    Directory* profiles = fs_create_dir("PROFILES");
    Directory* windows = fs_create_dir("Windows");
    Directory* temp = fs_create_dir("Temp");
    fs_add_child(g_root, profiles);
    fs_add_child(g_root, windows);
    fs_add_child(g_root, temp);
    
    // Create Public and Admin user profiles
    Directory* public_user = fs_create_dir("Public");
    Directory* admin_user = fs_create_dir("Admin");
    
    fs_add_child(profiles, public_user);
    fs_add_child(profiles, admin_user);
    
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
    
    // Add welcome files to each user
    File* public_readme = fs_create_file("README.txt");
    if (public_readme) {
        strcpy(public_readme->content, "Welcome to Public Profile!\n\nThis is your personal workspace.\n\nCommon commands:\n- DIR: List files\n- CD: Change directory\n- MKDIR: Create folder\n- TOUCH: Create file\n- WRITE: Write to file\n- TYPE: Read file\n- USER: Switch users\n- WHOAMI: Show current user");
        fs_add_file(public_user, public_readme);
    }
    
    File* admin_readme = fs_create_file("README.txt");
    if (admin_readme) {
        strcpy(admin_readme->content, "Welcome to Admin Profile!\n\nYou have administrative access.\n\nSystem directories:\n- System: System files and configurations\n- Documents: Admin documents\n- Desktop: Admin desktop files\n- Downloads: Downloaded files");
        fs_add_file(admin_user, admin_readme);
    }
    
    // Set default user to Public
    g_home = public_user;
    g_cwd = g_home;
    
    // Try to load existing filesystem
    fs_load_from_disk();
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
    snprintf(fs_file, sizeof(fs_file), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\filesystem.dat");
    
    FILE* f = fopen(fs_file, "rb");
    if (!f) {
        gui_println("No saved filesystem found, creating new one...");
        return;
    }
    
    // Simple loading: just check if file exists
    fclose(f);
    gui_println("Loading saved filesystem...");
    
    // For now, we'll just create the basic structure
    // In a full implementation, you'd deserialize the filesystem here
}

// Save filesystem to disk
static void fs_save_to_disk(void) {
    char fs_file[1024];
    snprintf(fs_file, sizeof(fs_file), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\filesystem.dat");
    
    FILE* f = fopen(fs_file, "wb");
    if (!f) {
        gui_println("Failed to save filesystem");
        return;
    }
    
    // Simple saving: just create a marker file
    fprintf(f, "Filesystem saved at %s\n", fs_file);
    fclose(f);
    
    gui_println("Filesystem saved successfully");
}

// ---------------- Command processing ----------------
static void cmd_help(void) {
    gui_println("Commands:");
    gui_println("  DIR, LS               List directory contents");
    gui_println("  CD <dir>|..|~         Change directory");
    gui_println("  MKDIR <name>          Create directory");
    gui_println("  TOUCH <name>          Create empty file");
    gui_println("  TYPE <file>           Show file contents");
    gui_println("  WRITE <file> <text>   Replace file content with text");
    gui_println("  APPEND <file> <text>  Append text to file");
    gui_println("  ECHO <text>           Print text");
    gui_println("  PWD                   Print working directory");
    gui_println("  SAVEFS <path>         Save entire filesystem to disk");
    gui_println("  USER <name>           Switch to user");
    gui_println("  WHOAMI                Show current user");
    gui_println("  USERS                 List all users");
    gui_println("  FILEVIEW              Show files in filesystem tree structure");
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
        
        // Also create the directory on the real file system
        char real_path[1024];
        snprintf(real_path, sizeof(real_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\%s\\%s", g_currentUser, name);
        
        // Create the directory on disk
        char mkdir_cmd[1024];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir \"%s\" 2>nul", real_path);
        system(mkdir_cmd);
        
        char msg[256];
        snprintf(msg, sizeof(msg), "Directory '%s' created successfully.", name);
        gui_println(msg);
        gui_println("Real path:");
        gui_println(real_path);
        
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
        
        // Convert virtual path to real path
        char real_path[1024];
        const char* profiles_pos = strstr(virtual_path, "\\PROFILES\\");
        if (profiles_pos) {
            snprintf(real_path, sizeof(real_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE%s", profiles_pos);
        } else {
            snprintf(real_path, sizeof(real_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\%s", g_currentUser);
        }
        
        // Add the filename to the path
        char full_real_path[1024];
        join_path(full_real_path, sizeof(full_real_path), real_path, name);
        
        // Create empty file on disk
        char touch_cmd[1024];
        snprintf(touch_cmd, sizeof(touch_cmd), "echo. > \"%s\" 2>nul", full_real_path);
        system(touch_cmd);
        
        char msg[256];
        snprintf(msg, sizeof(msg), "File '%s' created successfully.", name);
        gui_println(msg);
        gui_println("Real path:");
        gui_println(full_real_path);
        
        // Auto-save filesystem
        fs_save_to_disk();
    }
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
    
    // Convert virtual path to real path
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\PROFILES\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE%s", profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\%s", g_currentUser);
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
    gui_println("Real path:");
    gui_println(full_real_path);
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
    
    // Convert virtual path to real path
    char real_path[1024];
    const char* profiles_pos = strstr(virtual_path, "\\PROFILES\\");
    if (profiles_pos) {
        snprintf(real_path, sizeof(real_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE%s", profiles_pos);
    } else {
        snprintf(real_path, sizeof(real_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\%s", g_currentUser);
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
    gui_println("Real path:");
    gui_println(full_real_path);
}

static void cmd_type(const char* name) {
    if (!name || !*name) { gui_println("The system cannot find the file specified."); return; }
    File* f = fs_find_file(g_cwd, name);
    if (!f) { gui_println("The system cannot find the file specified."); return; }
    gui_println(f->content);
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
        gui_println("Available users: Public, Admin");
        return; 
    }
    
    // Check if username is valid
    if (strcmp(username, "Public") != 0 && strcmp(username, "Admin") != 0) {
        gui_println("Invalid user. Available users: Public, Admin");
        return;
    }
    
    Directory* profiles = fs_find_child(g_root, "PROFILES");
    if (!profiles) { 
        gui_println("PROFILES directory not found"); 
        return; 
    }
    
    Directory* user_dir = fs_find_child(profiles, username);
    if (!user_dir) { 
        char msg[256]; 
        snprintf(msg, sizeof(msg), "User profile '%s' not found", username);
        gui_println(msg);
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
    gui_println("  Public");
    gui_println("  Admin");
    gui_println("");
    gui_println("User profiles are stored in: PROFILES/");
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

// Simple Git command implementations using system calls
static void execute_git_command(const char* git_args) {
    char command[1024];
    snprintf(command, sizeof(command), "git %s", git_args);
    
    // Get the current working directory path in the real file system
    char storage_path[1024];
    char current_path[1024];
    
    // Build the real path based on current virtual directory
    if (g_cwd == g_home) {
        // We're in the user's home directory
        snprintf(storage_path, sizeof(storage_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\%s", g_currentUser);
    } else {
        // We're in a subdirectory, build the full path
        fs_print_path(g_cwd, current_path, sizeof(current_path));
        
        // Convert virtual path to real path
        // Virtual path: C:\PROFILES\Public\folder\subfolder
        // Real path: C:\Users\santi\OneDrive\Desktop\DIRECTORY_STORAGE\PROFILES\Public\folder\subfolder
        
        // Find the position after "C:\PROFILES\"
        char* profiles_pos = strstr(current_path, "\\PROFILES\\");
        if (profiles_pos) {
            // Skip "C:\PROFILES\" and add the rest to the base path
            char* relative_path = profiles_pos + 10; // Skip "\PROFILES\"
            snprintf(storage_path, sizeof(storage_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\%s", relative_path);
        } else {
            // Fallback to home directory
            snprintf(storage_path, sizeof(storage_path), "C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\%s", g_currentUser);
        }
    }
    
    // Create the directories if they don't exist
    system("mkdir \"C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\" 2>nul");
    system("mkdir \"C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\" 2>nul");
    system("mkdir \"C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\Public\" 2>nul");
    system("mkdir \"C:\\Users\\santi\\OneDrive\\Desktop\\DIRECTORY_STORAGE\\PROFILES\\Admin\" 2>nul");
    
    // Create a temporary batch file to capture output (in temp directory)
    char temp_bat[1024];
    snprintf(temp_bat, sizeof(temp_bat), "%s\\..\\..\\temp_git_output.bat", storage_path);
    
    // Create the batch file content
    char batch_content[2048];
    snprintf(batch_content, sizeof(batch_content), 
        "@echo off\n"
        "cd /d \"%s\"\n"
        "%s\n"
        "echo.\n"
        "echo [GIT_EXIT_CODE] %%ERRORLEVEL%%\n", 
        storage_path, command);
    
    // Write the batch file
    HANDLE hFile = CreateFileA(temp_bat, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, batch_content, strlen(batch_content), &bytesWritten, NULL);
        CloseHandle(hFile);
        
        // Execute the batch file and capture output
        char full_command[2048];
        snprintf(full_command, sizeof(full_command), "\"%s\" 2>&1", temp_bat);
        
        FILE* pipe = _popen(full_command, "r");
        if (pipe) {
            char buffer[1024];
            int exit_code = 0;
            
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                // Remove newline
                buffer[strcspn(buffer, "\r\n")] = 0;
                
                // Check for exit code marker
                if (strstr(buffer, "[GIT_EXIT_CODE]") != NULL) {
                    sscanf(buffer, "[GIT_EXIT_CODE] %d", &exit_code);
                    break;
                }
                
                // Display the output line
                gui_println(buffer);
            }
            
            _pclose(pipe);
            
            if (exit_code == 0) {
                gui_println("Git command completed successfully.");
            } else {
                gui_println("Git command failed. Check your Git installation and try again.");
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "Exit code: %d", exit_code);
                gui_println(error_msg);
            }
        } else {
            gui_println("Failed to execute Git command.");
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
    
    char command[1024];
    snprintf(command, sizeof(command), "clone %s", url);
    execute_git_command(command);
    
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

static void cmd_git_checkout(const char* branch) {
    if (!branch || !*branch) {
        gui_println("Usage: GIT CHECKOUT <branch>");
        gui_println("Example: GIT CHECKOUT main");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "checkout %s", branch);
    execute_git_command(command);
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
}

static void cmd_git_pull(void) {
    gui_println("Pulling changes from remote...");
    execute_git_command("pull");
}

static void cmd_git_push(void) {
    gui_println("Pushing changes to remote...");
    execute_git_command("push");
}

static void cmd_git_remote(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: GIT REMOTE -v or GIT REMOTE ADD <name> <url>");
        gui_println("Example: GIT REMOTE -v (list remotes)");
        return;
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "remote %s", args);
    execute_git_command(command);
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
        gui_println("  FETCH, RESET, STASH, TAG, CONFIG, VERSION");
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
    else if (strcmp(input, "TYPE") == 0 || strcmp(input, "CAT") == 0) { cmd_type(arg); }
    else if (strcmp(input, "WRITE") == 0) { cmd_write(arg); }
    else if (strcmp(input, "APPEND") == 0) { cmd_append(arg); }
    else if (strcmp(input, "CD") == 0) { cmd_cd(arg); }
    else if (strcmp(input, "ECHO") == 0) { cmd_echo(arg); }
    else if (strcmp(input, "SAVEFS") == 0) { cmd_savefs(arg); }
    else if (strcmp(input, "USER") == 0) { cmd_user(arg); }
    else if (strcmp(input, "WHOAMI") == 0) { cmd_whoami(); }
    else if (strcmp(input, "USERS") == 0) { cmd_users(); }
    else if (strcmp(input, "FILEVIEW") == 0) { cmd_fileview(); }
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
        else if (strcmp(git_cmd, "CHECKOUT") == 0) { cmd_git_checkout(git_args); }
        else if (strcmp(git_cmd, "MERGE") == 0) { cmd_git_merge(git_args); }
        else if (strcmp(git_cmd, "PULL") == 0) { cmd_git_pull(); }
        else if (strcmp(git_cmd, "PUSH") == 0) { cmd_git_push(); }
        else if (strcmp(git_cmd, "REMOTE") == 0) { cmd_git_remote(git_args); }
        else if (strcmp(git_cmd, "FETCH") == 0) { cmd_git_fetch(); }
        else if (strcmp(git_cmd, "RESET") == 0) { cmd_git_reset(git_args); }
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

