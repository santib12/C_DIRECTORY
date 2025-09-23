#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <time.h>
#include <stdarg.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
// IDE functionality integrated directly

#define MAX_NAME 256
#define MAX_CHILDREN 100
#define MAX_FILES 100
#define MAX_FILE_SIZE 2048

// Security Constants
#define MAX_PASSWORD_LENGTH 128
#define SALT_LENGTH 32
#define HASH_LENGTH 64
#define MAX_FAILED_ATTEMPTS 5
#define LOCKOUT_DURATION 300  // 5 minutes in seconds
#define SESSION_TIMEOUT 1800  // 30 minutes in seconds
#define MAX_SESSIONS 10

// Theme Constants
#define MAX_THEME_NAME 32
#define MAX_FONT_NAME 64
#define MAX_SETTINGS_SIZE 1024

// Project Detection Constants
#define MAX_PROJECT_PATH 512
#define MAX_SCRIPT_NAME 128
#define MAX_DEPENDENCY_NAME 64
#define MAX_DEPENDENCIES 50

// Project Types
typedef enum {
    PROJECT_NONE = 0,
    PROJECT_NODEJS,      // package.json exists
    PROJECT_REACT,       // package.json + react dependency
    PROJECT_VUE,         // package.json + vue dependency
    PROJECT_ANGULAR,     // package.json + @angular/core
    PROJECT_CPP,         // .cpp files present
    PROJECT_PYTHON,      // .py files present
    PROJECT_MIXED        // Multiple project types
} ProjectType;

// Project Information Structure
typedef struct {
    ProjectType type;
    char package_json_path[MAX_PROJECT_PATH];
    char main_file[MAX_PROJECT_PATH];
    char build_command[MAX_SCRIPT_NAME];
    char run_command[MAX_SCRIPT_NAME];
    char install_command[MAX_SCRIPT_NAME];
    char test_command[MAX_SCRIPT_NAME];
    char dev_command[MAX_SCRIPT_NAME];
    int has_node_modules;
    int has_requirements_txt;
    int has_cmake;
    int has_makefile;
} ProjectInfo;

// Global project info
static ProjectInfo g_currentProject = {0};

// Forward declarations
static void get_main_project_dir(char* buffer, size_t size);
static void cmd_sync(void);
static void cmd_writeln(const char* args);
static void cmd_writecode(const char* args);
static void cmd_editcode(const char* args);
static void cmd_adduser(const char* args);
static void gui_println(const char* text);
static void gui_printf(const char* format, ...);

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

// ---------------- Security & Authentication System ----------------
typedef struct {
    char username[64];
    char password_hash[HASH_LENGTH + 1];  // SHA-256 hash
    char salt[SALT_LENGTH + 1];           // Random salt for security
    time_t last_login;
    time_t password_changed;
    int failed_attempts;
    time_t locked_until;                  // Account lockout timestamp
    int privilege_level;                  // 0=Public, 1=Admin, 2=SuperAdmin
    BOOL is_active;                       // Account status
} UserAuth;

typedef struct {
    char session_id[64];
    char username[64];
    BOOL is_authenticated;
    time_t session_start;
    time_t last_activity;
    int privilege_level;
    char ip_address[16];                  // For future network features
} Session;

// ---------------- Theme & Customization System ----------------
typedef struct {
    char name[MAX_THEME_NAME];
    COLORREF text_color;
    COLORREF bg_color;
    COLORREF cursor_color;
    COLORREF selection_color;
    COLORREF error_color;
    COLORREF success_color;
    COLORREF warning_color;
    COLORREF prompt_color;
} ColorTheme;

typedef struct {
    char current_theme[MAX_THEME_NAME];
    int cursor_blink_speed;               // milliseconds
    int font_size;
    char font_name[MAX_FONT_NAME];
    BOOL auto_sync_enabled;
    BOOL show_hidden_files;
    char default_editor[64];
    int max_history_size;
    BOOL sound_enabled;
    int window_width;
    int window_height;
    BOOL require_auth_for_admin;          // Security setting
    int session_timeout;                  // minutes
    COLORREF custom_text_color;           // Custom text color (0 = use theme)
    COLORREF custom_cursor_color;         // Custom cursor color (0 = use theme)
} TerminalSettings;

static Directory* g_root = NULL;
static Directory* g_cwd = NULL;
static Directory* g_home = NULL;
static char g_currentUser[64] = "Public";

// Security & Authentication Globals
static UserAuth g_userAuth[MAX_SESSIONS];
static Session g_currentSession;
static int g_authCount = 0;
static BOOL g_requireAuthForAdmin = TRUE;
static char g_authFilePath[MAX_PATH];

// Theme & Settings Globals
static ColorTheme g_themes[10];
static int g_themeCount = 0;
static TerminalSettings g_settings;
static char g_settingsFilePath[MAX_PATH];
static char g_themesDir[MAX_PATH];

// Forward declarations
static void fs_load_from_disk(void);
static void fs_save_to_disk(void);
static void save_filesystem_recursive(Directory* dir, FILE* f, const char* path);
static void join_path(char* out, size_t out_sz, const char* base, const char* name);
static void load_users_from_realfilesystem(void);
static void sync_all_directories(void);
static void sync_directory_recursive(Directory* virtual_dir, const char* real_path);
static void cmd_ide(const char* args);
static void cmd_ide_help(void);
static void cmd_ide_list(void);

// Security & Authentication Functions
static void init_security_system(void);
static void init_theme_system(void);
static void generate_salt(char* salt, int length);
static void hash_password(const char* password, const char* salt, char* hash);
static BOOL verify_password(const char* password, const char* hash, const char* salt);
static BOOL create_user_account(const char* username, const char* password, int privilege_level);
static BOOL authenticate_user(const char* username, const char* password);
static void logout_user(void);
static BOOL change_password(const char* username, const char* old_password, const char* new_password);
static void save_auth_data(void);
static void load_auth_data(void);
static void load_user_specific_auth(void);
static void log_security_event(const char* event, const char* username, const char* details);
static BOOL is_account_locked(const char* username);
static void lock_account(const char* username);
static void unlock_account(const char* username);
static BOOL has_privilege(int required_level);
static BOOL user_exists(const char* username);

// Theme & Settings Functions
static void init_default_themes(void);
static void load_settings(void);
static void save_settings(void);
static void apply_theme(const char* theme_name);
static void apply_theme_silent(const char* theme_name, BOOL show_message);
static void create_custom_theme(const char* name, COLORREF text, COLORREF bg, COLORREF cursor);
static void cmd_login(const char* args);
static void cmd_logout(void);
static void cmd_chpasswd(const char* args);
static void cmd_whoami(void);
static void cmd_sessions(void);
static void cmd_theme(const char* args);
static void cmd_settings(const char* args);
static void cmd_set(const char* args);
static void cmd_get(const char* args);
static void cmd_setup_auth(const char* args);
static void cmd_renameuser(const char* args);
static void cmd_textcolor(const char* args);
static void cmd_cursorcolor(const char* args);

// Project Detection Functions
static void detect_project_type(void);
static ProjectType scan_directory_for_project_type(Directory* dir);
static int has_package_json(Directory* dir);
static int has_react_dependency(Directory* dir);
static int has_vue_dependency(Directory* dir);
static int has_angular_dependency(Directory* dir);
static int has_cpp_files(Directory* dir);
static int has_python_files(Directory* dir);
static int has_requirements_txt(Directory* dir);
static int has_cmake_files(Directory* dir);
static int has_makefile(Directory* dir);
static void load_package_json_info(void);
static void find_main_files(void);
static const char* project_type_to_string(ProjectType type);
static void cmd_project_type(void);
static void cmd_project_info(void);

// npm/Node.js Commands
static void cmd_npm_install(const char* args);
static void cmd_npm_add(const char* args);
static void cmd_npm_remove(const char* args);
static void cmd_npm_update(const char* args);
static void cmd_npm_audit(const char* args);
static void cmd_npm_outdated(void);
static void cmd_npm_run(const char* args);
static void cmd_npm_start(void);
static void cmd_npm_build(void);
static void cmd_npm_test(void);
static void cmd_npm_dev(void);
static void cmd_npm_init(const char* args);
static void cmd_npm_publish(const char* args);
static void cmd_npm_link(const char* args);
static void cmd_npm_list(void);
static void cmd_npm_help(void);

// React Development Commands
static void cmd_react_create(const char* args);
static void cmd_react_component(const char* args);
static void cmd_react_start(void);
static void cmd_react_build(void);
static void cmd_react_test(void);
static void cmd_react_eject(void);
static void cmd_react_lint(void);

// System Command Execution
static void execute_system_command(const char* command, const char* args);
static void execute_npm_command(const char* npm_args);

// C++ and Python Execution
static void execute_cpp_file(const char* filename);
static void execute_python_file(const char* filename);
static int find_cpp_files(char files[][256], int max_files);
static int find_python_files(char files[][256], int max_files);
static void cmd_run(const char* filename);
static void cmd_compile(const char* filename);

// Command history functions
static void add_to_history(const char* command);
static void navigate_history(int direction);
static void update_input_field(const char* text);

// System maintenance functions for admin operations
static void create_system_maintenance_folder(void);
static BOOL verify_admin_credentials(const char* username, const char* password);
static void cmd_system_ls(const char* path);
static void cmd_system_cd(const char* path);
static void cmd_system_cat(const char* path);
static void cmd_system_copy(const char* args);
static void cmd_system_exit(void);
static void cmd_system_log(void);
static void log_system_access(const char* action, const char* path);
static BOOL is_system_session_valid(void);

// Obfuscated string constants for security
static const char* SYS_MAINT_MSG1 = "Maintenance mode not active. Use CD .system_maintenance first.";
static const char* SYS_MAINT_MSG2 = "Maintenance session expired. Please re-authenticate.";
static const char* SYS_MAINT_MSG3 = "System Maintenance Access";
static const char* SYS_MAINT_MSG4 = "=========================";

// Edit mode variables
static int g_editMode = 0;
static File* g_editFile = NULL;
static char g_editBuffer[2048];
static int g_editBufferPos = 0;

// Command history variables
#define MAX_HISTORY 100
static char g_commandHistory[MAX_HISTORY][256];
static int g_historyCount = 0;
static int g_historyIndex = -1;
static char g_currentInput[256] = {0};

// System maintenance variables for admin operations
static BOOL g_systemMaintenanceMode = FALSE;
static char g_systemMaintenancePath[1024] = "C:\\";
static time_t g_systemMaintenanceStart = 0;
#define SYSTEM_MAINTENANCE_TIMEOUT 1800 // 30 minutes - system maintenance session timeout

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
    
    // Create Public and Admin user profiles directly under root
    Directory* public_user = fs_create_dir("Public");
    Directory* admin_user = fs_create_dir("Admin");
    
    fs_add_child(g_root, public_user);
    fs_add_child(g_root, admin_user);
    
    // Create some default directories for each user
    Directory* public_docs = fs_create_dir("Documents");
    Directory* public_desktop = fs_create_dir("Desktop");
    Directory* public_downloads = fs_create_dir("Downloads");
    Directory* public_settings = fs_create_dir("Settings");
    
    Directory* admin_docs = fs_create_dir("Documents");
    Directory* admin_desktop = fs_create_dir("Desktop");
    Directory* admin_downloads = fs_create_dir("Downloads");
    Directory* admin_system = fs_create_dir("System");
    Directory* admin_settings = fs_create_dir("Settings");
    
    fs_add_child(public_user, public_docs);
    fs_add_child(public_user, public_desktop);
    fs_add_child(public_user, public_downloads);
    fs_add_child(public_user, public_settings);
    
    fs_add_child(admin_user, admin_docs);
    fs_add_child(admin_user, admin_desktop);
    fs_add_child(admin_user, admin_downloads);
    fs_add_child(admin_user, admin_system);
    fs_add_child(admin_user, admin_settings);
    
    // Create Settings folders in real filesystem
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    // Create Public Settings folder
    char public_settings_path[1024];
    snprintf(public_settings_path, sizeof(public_settings_path), "%s\\data\\USERS\\Public\\Settings", program_dir);
    CreateDirectoryA(public_settings_path, NULL);
    
    // Create Admin Settings folder
    char admin_settings_path[1024];
    snprintf(admin_settings_path, sizeof(admin_settings_path), "%s\\data\\USERS\\Admin\\Settings", program_dir);
    CreateDirectoryA(admin_settings_path, NULL);
    
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

// ---------------- Security & Authentication System ----------------
static void generate_salt(char* salt, int length) {
    HCRYPTPROV hProv;
    BYTE binary_salt[32];
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptGenRandom(hProv, length, binary_salt)) {
            // Convert to hex string
            for (int i = 0; i < length; i++) {
                sprintf(salt + i * 2, "%02x", binary_salt[i]);
            }
            salt[length * 2] = '\0';
        }
        CryptReleaseContext(hProv, 0);
    }
}

static void hash_password(const char* password, const char* salt, char* hash) {
    // Simple hash function using a combination of password and salt
    // This is not cryptographically secure but works for basic authentication
    
            char combined[MAX_PASSWORD_LENGTH + SALT_LENGTH + 1];
            snprintf(combined, sizeof(combined), "%s%s", password, salt);
            
    // Simple hash: sum of all characters with some bit shifting
    unsigned long hash_value = 0;
    for (int i = 0; combined[i]; i++) {
        hash_value = hash_value * 31 + (unsigned char)combined[i];
    }
    
    // Convert to hex string (64 characters)
    sprintf(hash, "%016lx%016lx%016lx%016lx", 
            hash_value, 
            hash_value ^ 0x12345678, 
            hash_value ^ 0x87654321, 
            hash_value ^ 0xDEADBEEF);
                    hash[64] = '\0';
}

static BOOL verify_password(const char* password, const char* hash, const char* salt) {
    char computed_hash[HASH_LENGTH + 1];
    hash_password(password, salt, computed_hash);
    return strcmp(computed_hash, hash) == 0;
}

static void init_security_system(void) {
    // Initialize current session
    memset(&g_currentSession, 0, sizeof(g_currentSession));
    g_currentSession.is_authenticated = FALSE;
    g_currentSession.privilege_level = 0; // Public by default
    
    // Set up file paths
    char program_dir[MAX_PATH];
    get_main_project_dir(program_dir, sizeof(program_dir));
    snprintf(g_authFilePath, sizeof(g_authFilePath), "%s\\data\\USERS\\Admin\\System\\auth.dat", program_dir);
    snprintf(g_settingsFilePath, sizeof(g_settingsFilePath), "%s\\data\\USERS\\Admin\\System\\settings.dat", program_dir);
    snprintf(g_themesDir, sizeof(g_themesDir), "%s\\data\\USERS\\Admin\\System\\themes", program_dir);
    
    // Create System directory if it doesn't exist
    char system_dir[MAX_PATH];
    snprintf(system_dir, sizeof(system_dir), "%s\\data\\USERS\\Admin\\System", program_dir);
    CreateDirectoryA(system_dir, NULL);
    CreateDirectoryA(g_themesDir, NULL);
    
    // Load existing authentication data
    load_auth_data();
    
    // Create default Admin user if it doesn't exist
    if (g_authCount == 0 || !user_exists("Admin")) {
        create_user_account("Admin", "admin123", 1); // Admin privilege level
        gui_println("Default Admin user created with password: admin123");
    }
    
    // No default accounts - users must setup authentication manually
}

static BOOL create_user_account(const char* username, const char* password, int privilege_level) {
    if (g_authCount >= MAX_SESSIONS) {
        gui_println("Maximum number of user accounts reached");
        return FALSE;
    }
    
    // Check if user already exists
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            gui_println("User account already exists");
            return FALSE;
        }
    }
    
    // Create new user account
    UserAuth* user = &g_userAuth[g_authCount];
    strncpy(user->username, username, sizeof(user->username) - 1);
    user->username[sizeof(user->username) - 1] = '\0';
    
    // Generate salt and hash password
    generate_salt(user->salt, SALT_LENGTH / 2);
    hash_password(password, user->salt, user->password_hash);
    
    user->privilege_level = privilege_level;
    user->is_active = TRUE;
    user->failed_attempts = 0;
    user->locked_until = 0;
    user->last_login = 0;
    user->password_changed = time(NULL);
    
    g_authCount++;
    
    // Save authentication data
    save_auth_data();
    
    log_security_event("ACCOUNT_CREATED", username, "New user account created");
    
    return TRUE;
}

static BOOL authenticate_user(const char* username, const char* password) {
    // Check if account is locked
    if (is_account_locked(username)) {
        gui_println("Account is locked due to too many failed attempts");
        return FALSE;
    }
    
    // Find user account
    UserAuth* user = NULL;
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            user = &g_userAuth[i];
            break;
        }
    }
    
    if (!user || !user->is_active) {
        gui_println("Invalid username or account disabled");
        return FALSE;
    }
    
    // Verify password
    if (verify_password(password, user->password_hash, user->salt)) {
        // Successful authentication
        user->failed_attempts = 0;
        user->last_login = time(NULL);
        
        // Update session
        strncpy(g_currentSession.username, username, sizeof(g_currentSession.username) - 1);
        g_currentSession.is_authenticated = TRUE;
        g_currentSession.session_start = time(NULL);
        g_currentSession.last_activity = time(NULL);
        g_currentSession.privilege_level = user->privilege_level;
        
        // Generate session ID
        char session_id[65];
        generate_salt(session_id, 32);
        strncpy(g_currentSession.session_id, session_id, sizeof(g_currentSession.session_id) - 1);
        
        log_security_event("LOGIN_SUCCESS", username, "User successfully authenticated");
        
        return TRUE;
    } else {
        // Failed authentication
        user->failed_attempts++;
        if (user->failed_attempts >= MAX_FAILED_ATTEMPTS) {
            lock_account(username);
            gui_println("Account locked due to too many failed attempts");
        }
        
        log_security_event("LOGIN_FAILED", username, "Failed authentication attempt");
        
        return FALSE;
    }
}

static void logout_user(void) {
    if (g_currentSession.is_authenticated) {
        log_security_event("LOGOUT", g_currentSession.username, "User logged out");
    }
    
    memset(&g_currentSession, 0, sizeof(g_currentSession));
    g_currentSession.is_authenticated = FALSE;
    g_currentSession.privilege_level = 0;
}

static BOOL change_password(const char* username, const char* old_password, const char* new_password) {
    // Find user account
    UserAuth* user = NULL;
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            user = &g_userAuth[i];
            break;
        }
    }
    
    if (!user) {
        gui_println("User account not found");
        return FALSE;
    }
    
    // Verify old password
    if (!verify_password(old_password, user->password_hash, user->salt)) {
        gui_println("Current password is incorrect");
        return FALSE;
    }
    
    // Update password
    generate_salt(user->salt, SALT_LENGTH / 2);
    hash_password(new_password, user->salt, user->password_hash);
    user->password_changed = time(NULL);
    
    // Save authentication data
    save_auth_data();
    
    log_security_event("PASSWORD_CHANGED", username, "Password successfully changed");
    
    return TRUE;
}

static void save_auth_data(void) {
    FILE* f = fopen(g_authFilePath, "w");
    if (!f) return;
    
    for (int i = 0; i < g_authCount; i++) {
        UserAuth* user = &g_userAuth[i];
        fprintf(f, "USER:%s|%s|%s|%ld|%ld|%d|%ld|%d|%d\n",
                user->username,
                user->password_hash,
                user->salt,
                user->last_login,
                user->password_changed,
                user->failed_attempts,
                user->locked_until,
                user->privilege_level,
                user->is_active ? 1 : 0);
    }
    
    fclose(f);
}

static void load_auth_data(void) {
    FILE* f = fopen(g_authFilePath, "r");
    if (!f) return;
    
    char line[1024];
    g_authCount = 0;
    
    while (fgets(line, sizeof(line), f) && g_authCount < MAX_SESSIONS) {
        if (strncmp(line, "USER:", 5) == 0) {
            char* data = line + 5;
            char* tokens[9];
            int token_count = 0;
            
            char* token = strtok(data, "|");
            while (token && token_count < 9) {
                tokens[token_count++] = token;
                token = strtok(NULL, "|");
            }
            
            if (token_count >= 9) {
                UserAuth* user = &g_userAuth[g_authCount];
                strncpy(user->username, tokens[0], sizeof(user->username) - 1);
                strncpy(user->password_hash, tokens[1], sizeof(user->password_hash) - 1);
                strncpy(user->salt, tokens[2], sizeof(user->salt) - 1);
                user->last_login = atol(tokens[3]);
                user->password_changed = atol(tokens[4]);
                user->failed_attempts = atoi(tokens[5]);
                user->locked_until = atol(tokens[6]);
                user->privilege_level = atoi(tokens[7]);
                user->is_active = atoi(tokens[8]) != 0;
                
                g_authCount++;
            }
        }
    }
    
    fclose(f);
    
    // Load user-specific authentication from each user's Settings folder
    load_user_specific_auth();
}

static void load_user_specific_auth(void) {
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    char users_dir[1024];
    snprintf(users_dir, sizeof(users_dir), "%s\\data\\USERS", program_dir);
    
    // Find all user directories
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
        
        // Skip Admin (already loaded from System folder)
        if (strcmp(findData.cFileName, "Admin") == 0) {
            continue;
        }
        
        // Load authentication from user's Settings folder
        char user_auth_file[1024];
        snprintf(user_auth_file, sizeof(user_auth_file), "%s\\%s\\Settings\\auth.dat", users_dir, findData.cFileName);
        
        FILE* f = fopen(user_auth_file, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f) && g_authCount < MAX_SESSIONS) {
                if (strncmp(line, "USER:", 5) == 0) {
                    char* data = line + 5;
                    char* tokens[9];
                    int token_count = 0;
                    
                    char* token = strtok(data, "|");
                    while (token && token_count < 9) {
                        tokens[token_count++] = token;
                        token = strtok(NULL, "|");
                    }
                    
                    if (token_count >= 9) {
                        UserAuth* user = &g_userAuth[g_authCount];
                        strncpy(user->username, tokens[0], sizeof(user->username) - 1);
                        strncpy(user->password_hash, tokens[1], sizeof(user->password_hash) - 1);
                        strncpy(user->salt, tokens[2], sizeof(user->salt) - 1);
                        user->last_login = atol(tokens[3]);
                        user->password_changed = atol(tokens[4]);
                        user->failed_attempts = atoi(tokens[5]);
                        user->locked_until = atol(tokens[6]);
                        user->privilege_level = atoi(tokens[7]);
                        user->is_active = atoi(tokens[8]) != 0;
                        
                        g_authCount++;
                    }
                }
            }
            fclose(f);
        }
        
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}

static void log_security_event(const char* event, const char* username, const char* details) {
    char log_file[MAX_PATH];
    char program_dir[MAX_PATH];
    get_main_project_dir(program_dir, sizeof(program_dir));
    snprintf(log_file, sizeof(log_file), "%s\\data\\USERS\\Admin\\System\\logs\\security.log", program_dir);
    
    // Create logs directory if it doesn't exist
    char logs_dir[MAX_PATH];
    snprintf(logs_dir, sizeof(logs_dir), "%s\\data\\USERS\\Admin\\System\\logs", program_dir);
    CreateDirectoryA(logs_dir, NULL);
    
    FILE* f = fopen(log_file, "a");
    if (f) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s - %s\n",
                tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                event, username, details);
        fclose(f);
    }
}

static BOOL is_account_locked(const char* username) {
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            time_t now = time(NULL);
            return g_userAuth[i].locked_until > now;
        }
    }
    return FALSE;
}

static void lock_account(const char* username) {
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            g_userAuth[i].locked_until = time(NULL) + LOCKOUT_DURATION;
            save_auth_data();
            log_security_event("ACCOUNT_LOCKED", username, "Account locked due to failed attempts");
            break;
        }
    }
}

static void unlock_account(const char* username) {
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            g_userAuth[i].locked_until = 0;
            g_userAuth[i].failed_attempts = 0;
            save_auth_data();
            log_security_event("ACCOUNT_UNLOCKED", username, "Account unlocked by administrator");
            break;
        }
    }
}

static BOOL has_privilege(int required_level) {
    // All users (including Admin) must be authenticated to access features
    if (g_currentSession.is_authenticated) {
        return g_currentSession.privilege_level >= required_level;
    }
    
    return required_level == 0; // Only public access for unauthenticated users
}

static BOOL user_exists(const char* username) {
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

// ---------------- Theme & Customization System ----------------
static void init_default_themes(void) {
    // Classic Green Theme
    strcpy(g_themes[0].name, "classic");
    g_themes[0].text_color = RGB(0, 255, 0);
    g_themes[0].bg_color = RGB(0, 0, 0);
    g_themes[0].cursor_color = RGB(255, 255, 255);
    g_themes[0].selection_color = RGB(0, 100, 200);
    g_themes[0].error_color = RGB(255, 0, 0);
    g_themes[0].success_color = RGB(0, 255, 0);
    g_themes[0].warning_color = RGB(255, 255, 0);
    g_themes[0].prompt_color = RGB(0, 255, 0);
    
    // White Theme
    strcpy(g_themes[1].name, "white");
    g_themes[1].text_color = RGB(0, 0, 0);
    g_themes[1].bg_color = RGB(255, 255, 255);
    g_themes[1].cursor_color = RGB(0, 0, 0);
    g_themes[1].selection_color = RGB(0, 100, 200);
    g_themes[1].error_color = RGB(255, 0, 0);
    g_themes[1].success_color = RGB(0, 128, 0);
    g_themes[1].warning_color = RGB(255, 165, 0);
    g_themes[1].prompt_color = RGB(0, 0, 255);
    
    // Dark Theme
    strcpy(g_themes[2].name, "dark");
    g_themes[2].text_color = RGB(200, 200, 200);
    g_themes[2].bg_color = RGB(30, 30, 30);
    g_themes[2].cursor_color = RGB(255, 255, 255);
    g_themes[2].selection_color = RGB(0, 100, 200);
    g_themes[2].error_color = RGB(255, 100, 100);
    g_themes[2].success_color = RGB(100, 255, 100);
    g_themes[2].warning_color = RGB(255, 200, 100);
    g_themes[2].prompt_color = RGB(100, 200, 255);
    
    g_themeCount = 3;
}

static void init_theme_system(void) {
    // Initialize default themes
    init_default_themes();
    
    // Initialize default settings
    strcpy(g_settings.current_theme, "classic");
    g_settings.cursor_blink_speed = 500;
    g_settings.font_size = 14;
    strcpy(g_settings.font_name, "Consolas");
    g_settings.auto_sync_enabled = TRUE;
    g_settings.show_hidden_files = FALSE;
    strcpy(g_settings.default_editor, "notepad");
    g_settings.max_history_size = 100;
    g_settings.sound_enabled = TRUE;
    g_settings.window_width = 800;
    g_settings.window_height = 600;
    g_settings.require_auth_for_admin = TRUE;
    g_settings.session_timeout = 30;
    
    // Load saved settings
    load_settings();
    
    // Note: Theme will be applied after GUI is initialized
}

static void load_settings(void) {
    FILE* f = fopen(g_settingsFilePath, "r");
    if (!f) return;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64], value[128];
        if (sscanf(line, "%63[^=]=%127s", key, value) == 2) {
            if (strcmp(key, "current_theme") == 0) {
                strncpy(g_settings.current_theme, value, sizeof(g_settings.current_theme) - 1);
            } else if (strcmp(key, "cursor_blink_speed") == 0) {
                g_settings.cursor_blink_speed = atoi(value);
            } else if (strcmp(key, "font_size") == 0) {
                g_settings.font_size = atoi(value);
            } else if (strcmp(key, "font_name") == 0) {
                strncpy(g_settings.font_name, value, sizeof(g_settings.font_name) - 1);
            } else if (strcmp(key, "auto_sync_enabled") == 0) {
                g_settings.auto_sync_enabled = atoi(value) != 0;
            } else if (strcmp(key, "show_hidden_files") == 0) {
                g_settings.show_hidden_files = atoi(value) != 0;
            } else if (strcmp(key, "default_editor") == 0) {
                strncpy(g_settings.default_editor, value, sizeof(g_settings.default_editor) - 1);
            } else if (strcmp(key, "max_history_size") == 0) {
                g_settings.max_history_size = atoi(value);
            } else if (strcmp(key, "sound_enabled") == 0) {
                g_settings.sound_enabled = atoi(value) != 0;
            } else if (strcmp(key, "window_width") == 0) {
                g_settings.window_width = atoi(value);
            } else if (strcmp(key, "window_height") == 0) {
                g_settings.window_height = atoi(value);
            } else if (strcmp(key, "require_auth_for_admin") == 0) {
                g_settings.require_auth_for_admin = atoi(value) != 0;
            } else if (strcmp(key, "session_timeout") == 0) {
                g_settings.session_timeout = atoi(value);
            } else if (strcmp(key, "custom_text_color") == 0) {
                g_settings.custom_text_color = (COLORREF)strtoul(value, NULL, 16);
            } else if (strcmp(key, "custom_cursor_color") == 0) {
                g_settings.custom_cursor_color = (COLORREF)strtoul(value, NULL, 16);
            }
        }
    }
    
    fclose(f);
}

static void save_settings(void) {
    FILE* f = fopen(g_settingsFilePath, "w");
    if (!f) return;
    
    fprintf(f, "current_theme=%s\n", g_settings.current_theme);
    fprintf(f, "cursor_blink_speed=%d\n", g_settings.cursor_blink_speed);
    fprintf(f, "font_size=%d\n", g_settings.font_size);
    fprintf(f, "font_name=%s\n", g_settings.font_name);
    fprintf(f, "auto_sync_enabled=%d\n", g_settings.auto_sync_enabled ? 1 : 0);
    fprintf(f, "show_hidden_files=%d\n", g_settings.show_hidden_files ? 1 : 0);
    fprintf(f, "default_editor=%s\n", g_settings.default_editor);
    fprintf(f, "max_history_size=%d\n", g_settings.max_history_size);
    fprintf(f, "sound_enabled=%d\n", g_settings.sound_enabled ? 1 : 0);
    fprintf(f, "window_width=%d\n", g_settings.window_width);
    fprintf(f, "window_height=%d\n", g_settings.window_height);
    fprintf(f, "require_auth_for_admin=%d\n", g_settings.require_auth_for_admin ? 1 : 0);
    fprintf(f, "session_timeout=%d\n", g_settings.session_timeout);
    fprintf(f, "custom_text_color=%08X\n", g_settings.custom_text_color);
    fprintf(f, "custom_cursor_color=%08X\n", g_settings.custom_cursor_color);
    
    fclose(f);
}

static void apply_theme(const char* theme_name) {
    apply_theme_silent(theme_name, FALSE);
}

static void apply_theme_silent(const char* theme_name, BOOL show_message) {
    for (int i = 0; i < g_themeCount; i++) {
        if (strcmp(g_themes[i].name, theme_name) == 0) {
            // Apply theme colors to GUI
            strncpy(g_settings.current_theme, theme_name, sizeof(g_settings.current_theme) - 1);
            save_settings();
            
            // Theme colors will be applied on next window redraw
            
            if (show_message) {
                gui_println("Theme applied successfully");
            }
            return;
        }
    }
    if (show_message) {
        gui_println("Theme not found");
    }
}

// ---------------- GUI helpers ----------------
static HWND g_hOut = NULL;

// Scroll functionality (using built-in edit control scrolling)
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

static void gui_printf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    gui_println(buffer);
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
        
        // Create system maintenance folder for Admin user
        create_system_maintenance_folder();
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
    
    // Ensure system maintenance folder exists for Admin user
    create_system_maintenance_folder();
    
    // Filesystem loaded silently
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
        
        // Create default directories for the new user
        Directory* user_docs = fs_create_dir("Documents");
        Directory* user_desktop = fs_create_dir("Desktop");
        Directory* user_downloads = fs_create_dir("Downloads");
        Directory* user_settings = fs_create_dir("Settings");
        
        fs_add_child(new_user, user_docs);
        fs_add_child(new_user, user_desktop);
        fs_add_child(new_user, user_downloads);
        fs_add_child(new_user, user_settings);
        
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
        
        // Ensure preset directories exist in real filesystem
        char docs_path[1024], desktop_path[1024], downloads_path[1024], settings_path[1024];
        snprintf(docs_path, sizeof(docs_path), "%s\\Documents", user_real_path);
        snprintf(desktop_path, sizeof(desktop_path), "%s\\Desktop", user_real_path);
        snprintf(downloads_path, sizeof(downloads_path), "%s\\Downloads", user_real_path);
        snprintf(settings_path, sizeof(settings_path), "%s\\Settings", user_real_path);
        
        CreateDirectoryA(docs_path, NULL);
        CreateDirectoryA(desktop_path, NULL);
        CreateDirectoryA(downloads_path, NULL);
        CreateDirectoryA(settings_path, NULL);
        
        // Check if the real user directory exists
        DWORD user_attrs = GetFileAttributesA(user_real_path);
        if (user_attrs != INVALID_FILE_ATTRIBUTES && (user_attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Sync this user's directory recursively
            sync_directory_recursive(user_dir, user_real_path);
        }
    }
    
    // Auto-sync completed silently
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
                    if (fileSize >= 0 && fileSize < 10240) { // Read files including empty ones, but smaller than 10KB
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
    gui_println("  RENAMEUSER <old> <new> Rename a user account");
    gui_println("  TEXTCOLOR <color>       Change text color");
    gui_println("  CURSORCOLOR <color>     Change cursor color");
    gui_println("  WHOAMI                Show current user");
    gui_println("  USERS                 List all users");
    gui_println("  FILEVIEW              Show files in filesystem tree structure");
    gui_println("  SYNC                  Sync virtual filesystem with real filesystem");
    gui_println("  SAVE                  Save filesystem to disk");
    gui_println("");
    gui_println("=== AUTHENTICATION ===");
    gui_println("  LOGIN <user> <pass>   Login to user account (required for Admin)");
    gui_println("  USER <username>       Switch to user (Admin requires login first)");
    gui_println("  LOGOUT                Logout from current session");
    gui_println("  CHPASSWD <old> <new>  Change password");
    gui_println("  SETUP_AUTH <user> <pass>  Setup authentication for user");
    gui_println("");
    gui_println("=== CUSTOMIZATION ===");
    gui_println("  THEME <name>          Switch theme (classic, white, dark)");
    gui_println("  THEME LIST            List available themes");
    gui_println("  SETTINGS              Show current settings");
    gui_println("  SET <setting> <value> Set configuration value");
    gui_println("  GET <setting>         Get configuration value");
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
    gui_println("");
    gui_println("=== PROJECT DETECTION ===");
    gui_println("  PROJECT_TYPE           Show current project type");
    gui_println("  PROJECT_INFO           Show detailed project information");
    gui_println("");
    gui_println("=== C++ & PYTHON EXECUTION ===");
    gui_println("  RUN <file>             Run C++ or Python file");
    gui_println("  COMPILE <file>         Compile C++ file");
    gui_println("");
    gui_println("=== SYSTEM MAINTENANCE (Admin Only) ===");
    gui_println("  CD .system_maintenance Access system maintenance tools");
    gui_println("  SYSTEM_LS <path>       List directory contents");
    gui_println("  SYSTEM_CD <path>       Change working directory");
    gui_println("  SYSTEM_CAT <path>      Display file contents");
    gui_println("  SYSTEM_COPY <src> <dest> Copy files between systems");
    gui_println("  SYSTEM_EXIT            Exit maintenance mode");
    gui_println("  SYSTEM_LOG             View maintenance activity log");
    gui_println("");
    gui_println("=== NPM/NODE.JS COMMANDS ===");
    gui_println("  NPM INSTALL            Install all dependencies");
    gui_println("  NPM ADD <package>      Install a package");
    gui_println("  NPM REMOVE <package>   Uninstall a package");
    gui_println("  NPM UPDATE             Update all packages");
    gui_println("  NPM AUDIT              Check for vulnerabilities");
    gui_println("  NPM OUTDATED           Show outdated packages");
    gui_println("  NPM RUN <script>       Run a package script");
    gui_println("  NPM START              Run start script");
    gui_println("  NPM BUILD              Run build script");
    gui_println("  NPM TEST               Run test script");
    gui_println("  NPM DEV                Run dev script");
    gui_println("  NPM INIT               Initialize package.json");
    gui_println("  NPM LIST               List installed packages");
    gui_println("  NPM HELP               Show npm help");
    gui_println("");
    gui_println("=== REACT DEVELOPMENT ===");
    gui_println("  REACT CREATE <name>    Create new React app");
    gui_println("  REACT COMPONENT <name> Create React component");
    gui_println("  REACT START            Start React dev server");
    gui_println("  REACT BUILD            Build React app");
    gui_println("  REACT TEST             Run React tests");
    gui_println("  REACT EJECT            Eject from Create React App");
    gui_println("  REACT LINT             Run ESLint");
    gui_println("");
    gui_println("IDE Commands (open external editors):");
    gui_println("  IDE vscode             Open VS Code in current directory");
    gui_println("  IDE code               Alternative VS Code command");
    gui_println("  IDE cursor             Open Cursor in current directory");
    gui_println("  IDE notepad            Open Notepad");
    gui_println("  IDE notepad++          Open Notepad++");
    gui_println("  IDE sublime            Open Sublime Text");
    gui_println("  IDE atom               Open Atom");
    gui_println("  IDE vim                Open Vim");
    gui_println("  IDE LIST               List available editors");
    gui_println("  IDE HELP               Show IDE command help");
}

// ---------------- IDE Commands ----------------

// IDE Configuration
#define MAX_IDES 10
#define MAX_EDITOR_NAME 32
#define MAX_EDITOR_PATH 512

typedef struct {
    char name[MAX_EDITOR_NAME];
    char path[MAX_EDITOR_PATH];
    int enabled;
} IDEConfig;

static IDEConfig g_ide_configs[MAX_IDES];
static int g_ide_count = 0;

// Initialize IDE configurations
static void init_ide_configs(void) {
    g_ide_count = 0;
    
    // VS Code detection
    const char* vscode_paths[] = {
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe",
        "C:\\Program Files\\Microsoft VS Code\\Code.exe",
        "C:\\Program Files (x86)\\Microsoft VS Code\\Code.exe"
    };
    
    for (int i = 0; i < 3; i++) {
        char expanded_path[MAX_EDITOR_PATH];
        ExpandEnvironmentStringsA(vscode_paths[i], expanded_path, sizeof(expanded_path));
        
        if (GetFileAttributesA(expanded_path) != INVALID_FILE_ATTRIBUTES) {
            strcpy(g_ide_configs[g_ide_count].name, "vscode");
            strcpy(g_ide_configs[g_ide_count].path, expanded_path);
            g_ide_configs[g_ide_count].enabled = 1;
            g_ide_count++;
            
            // Add alternative "code" command
            strcpy(g_ide_configs[g_ide_count].name, "code");
            strcpy(g_ide_configs[g_ide_count].path, expanded_path);
            g_ide_configs[g_ide_count].enabled = 1;
            g_ide_count++;
            break;
        }
    }
    
    // Notepad detection
    const char* notepad_path = "C:\\Windows\\System32\\notepad.exe";
    if (GetFileAttributesA(notepad_path) != INVALID_FILE_ATTRIBUTES) {
        strcpy(g_ide_configs[g_ide_count].name, "notepad");
        strcpy(g_ide_configs[g_ide_count].path, notepad_path);
        g_ide_configs[g_ide_count].enabled = 1;
        g_ide_count++;
    }
    
    // Notepad++ detection
    const char* npp_paths[] = {
        "C:\\Program Files\\Notepad++\\notepad++.exe",
        "C:\\Program Files (x86)\\Notepad++\\notepad++.exe"
    };
    
    for (int i = 0; i < 2; i++) {
        if (GetFileAttributesA(npp_paths[i]) != INVALID_FILE_ATTRIBUTES) {
            strcpy(g_ide_configs[g_ide_count].name, "notepad++");
            strcpy(g_ide_configs[g_ide_count].path, npp_paths[i]);
            g_ide_configs[g_ide_count].enabled = 1;
            g_ide_count++;
            break;
        }
    }
    
    // Cursor detection
    const char* cursor_paths[] = {
        "C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\cursor\\Cursor.exe",
        "C:\\Program Files\\Cursor\\Cursor.exe",
        "C:\\Program Files (x86)\\Cursor\\Cursor.exe"
    };
    
    for (int i = 0; i < 3; i++) {
        char expanded_path[MAX_EDITOR_PATH];
        ExpandEnvironmentStringsA(cursor_paths[i], expanded_path, sizeof(expanded_path));
        
        if (GetFileAttributesA(expanded_path) != INVALID_FILE_ATTRIBUTES) {
            strcpy(g_ide_configs[g_ide_count].name, "cursor");
            strcpy(g_ide_configs[g_ide_count].path, expanded_path);
            g_ide_configs[g_ide_count].enabled = 1;
            g_ide_count++;
            break;
        }
    }
}

// Find IDE by name
static int find_ide_by_name(const char* name) {
    for (int i = 0; i < g_ide_count; i++) {
        if (strcmp(g_ide_configs[i].name, name) == 0 && g_ide_configs[i].enabled) {
            return i;
        }
    }
    return -1;
}

// Get current working directory for IDE
static void get_current_working_directory(char* path, size_t path_size) {
    // Get the main project directory
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    // Build the real path based on current virtual directory
    if (g_cwd) {
        // Convert virtual path to real path
        char virtual_path[1024];
        fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
        
        // Find the position after "C:\\USERS\\"
        char* profiles_pos = strstr(virtual_path, "\\USERS\\");
        if (profiles_pos) {
            // Skip "C:\USERS\" and add the rest to the base path
            char* relative_path = profiles_pos + 7; // Skip "\USERS\"
            snprintf(path, path_size, "%s\\data\\USERS\\%s", program_dir, relative_path);
        } else {
            // Fallback to current user's directory
            snprintf(path, path_size, "%s\\data\\USERS\\%s", program_dir, g_currentUser);
        }
    } else {
        strcpy(path, "C:\\");
    }
}

// Execute IDE
static int execute_ide(const char* ide_path, const char* working_dir) {
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_NORMAL;
    
    // Build command line with new window flags
    char command_line[1024];
    
    // Check if it's VS Code or Cursor (they use similar arguments)
    const char* exe_name = strrchr(ide_path, '\\');
    if (exe_name) exe_name++; else exe_name = ide_path;
    
    if (strstr(exe_name, "Code.exe") || strstr(exe_name, "Cursor.exe")) {
        // VS Code and Cursor: use -n for new window
        snprintf(command_line, sizeof(command_line), "\"%s\" -n \"%s\"", ide_path, working_dir);
    } else if (strstr(exe_name, "notepad++.exe")) {
        // Notepad++: use -multiInst for new instance
        snprintf(command_line, sizeof(command_line), "\"%s\" -multiInst \"%s\"", ide_path, working_dir);
    } else {
        // Default: just open the directory
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

// IDE command implementation
static void cmd_ide(const char* args) {
    if (!args || !*args) {
        cmd_ide_help();
        return;
    }
    
    // Parse arguments
    char editor_name[64];
    if (sscanf(args, "%63s", editor_name) != 1) {
        cmd_ide_help();
        return;
    }
    
    // Find the IDE
    int ide_index = find_ide_by_name(editor_name);
    if (ide_index == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "IDE '%s' not found. Use 'IDE LIST' to see available editors.", editor_name);
        gui_println(msg);
        return;
    }
    
    // Get current working directory
    char working_dir[1024];
    get_current_working_directory(working_dir, sizeof(working_dir));
    
    // Execute the IDE
    if (execute_ide(g_ide_configs[ide_index].path, working_dir)) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Opening %s in %s...", g_ide_configs[ide_index].name, working_dir);
        gui_println(msg);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to open %s. Please check if it's installed.", g_ide_configs[ide_index].name);
        gui_println(msg);
    }
}

// IDE help command
static void cmd_ide_help(void) {
    gui_println("IDE Command Help:");
    gui_println("================");
    gui_println("Usage: IDE <editor>");
    gui_println("");
    gui_println("Available Editors:");
    cmd_ide_list();
    gui_println("");
    gui_println("Examples:");
    gui_println("  IDE vscode              - Open VS Code in new window");
    gui_println("  IDE code                - Alternative VS Code command");
    gui_println("  IDE cursor              - Open Cursor in new window");
    gui_println("  IDE notepad             - Open Notepad");
    gui_println("  IDE notepad++           - Open Notepad++ in new instance");
    gui_println("");
    gui_println("Editors will open in new windows in the current terminal directory.");
}

// IDE list command
static void cmd_ide_list(void) {
    gui_println("Available IDEs:");
    gui_println("===============");
    
    for (int i = 0; i < g_ide_count; i++) {
        if (g_ide_configs[i].enabled) {
            char msg[256];
            snprintf(msg, sizeof(msg), "  %-12s - %s", g_ide_configs[i].name, g_ide_configs[i].path);
            gui_println(msg);
        }
    }
    
    if (g_ide_count == 0) {
        gui_println("  No IDEs detected. Please install an editor.");
    }
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
    
    // Create default directories for the new user
    Directory* user_docs = fs_create_dir("Documents");
    Directory* user_desktop = fs_create_dir("Desktop");
    Directory* user_downloads = fs_create_dir("Downloads");
    Directory* user_settings = fs_create_dir("Settings");
    
    fs_add_child(new_user, user_docs);
    fs_add_child(new_user, user_desktop);
    fs_add_child(new_user, user_downloads);
    fs_add_child(new_user, user_settings);
    
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
    
    // Create all preset directories in real filesystem
    char settings_path[1024];
    snprintf(settings_path, sizeof(settings_path), "%s\\Settings", user_path);
    CreateDirectoryA(settings_path, NULL);
    
    char docs_path[1024];
    snprintf(docs_path, sizeof(docs_path), "%s\\Documents", user_path);
    CreateDirectoryA(docs_path, NULL);
    
    char desktop_path[1024];
    snprintf(desktop_path, sizeof(desktop_path), "%s\\Desktop", user_path);
    CreateDirectoryA(desktop_path, NULL);
    
    char downloads_path[1024];
    snprintf(downloads_path, sizeof(downloads_path), "%s\\Downloads", user_path);
    CreateDirectoryA(downloads_path, NULL);
    
    // All preset directories created in both virtual and real filesystem
    
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
    
    // Create system maintenance folder for Admin user
    if (strcmp(username, "Admin") == 0) {
        create_system_maintenance_folder();
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
    
    // Check if file has content
    if (!f->content || strlen(f->content) == 0) {
        gui_println("File is empty.");
        return;
    }
    
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
    
        // Check for system maintenance folder access (admin only)
        if (strcmp(path, ".system_maintenance") == 0) {
            
            // Verify admin privileges for system maintenance
            if (strcmp(g_currentUser, "Admin") != 0) {
                gui_println("Access denied. System maintenance requires Admin privileges.");
                return;
            }
            
            // Check if already in maintenance mode
            if (g_systemMaintenanceMode) {
                gui_println("Already in maintenance mode. Use SYSTEM_EXIT to exit.");
                return;
            }
            
            // Check if user is authenticated
            if (!g_currentSession.is_authenticated) {
                gui_println("System Maintenance Access");
                gui_println("=========================");
                gui_println("Enter Admin credentials:");
                gui_println("Username: Admin");
                gui_println("Password: [Enter your Admin password]");
                gui_println("");
                gui_println("Use: LOGIN Admin <password> to authenticate");
                gui_println("Then use: CD .system_maintenance again");
                return;
            }
            
            // Activate system maintenance mode
            g_systemMaintenanceMode = TRUE;
            strcpy(g_systemMaintenancePath, "C:\\");
            g_systemMaintenanceStart = time(NULL);
            
            gui_println("System maintenance mode activated.");
            gui_println("Accessing real filesystem...");
            gui_println("Use SYSTEM_* commands to interact with the real filesystem.");
            gui_println("Type SYSTEM_EXIT to return to normal mode.");
            return;
        }
    
    Directory* d = fs_find_child(g_cwd, path);
    if (d) { 
        g_cwd = d; 
        // Auto-detect project type when changing directories
        detect_project_type();
    } else { 
        gui_println("The system cannot find the path specified."); 
    }
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
    
    // Check if user has authentication credentials
    BOOL user_has_auth = FALSE;
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            user_has_auth = TRUE;
            break;
        }
    }
    
    // If user has authentication credentials, require login
    if (user_has_auth) {
        if (!g_currentSession.is_authenticated || strcmp(g_currentSession.username, username) != 0) {
            gui_println("User access requires authentication.");
            gui_printf("Use: LOGIN %s <password>", username);
            return;
        }
    } else {
        // User doesn't have authentication setup
        gui_printf("User '%s' does not have authentication setup.", username);
        gui_printf("Use: SETUP_AUTH %s <password>", username);
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
    
    // Build the real path based on current virtual directory
    if (g_cwd == g_home) {
        // We're in the user's home directory
        snprintf(storage_path, sizeof(storage_path), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    } else {
        // We're in a subdirectory, build the full path
        fs_print_path(g_cwd, current_path, sizeof(current_path));
        
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
    while (*p && !isspace((unsigned char)*p)) { p++; }
    int has_cmd = (*line != '\0');
    if (*p) { *p++ = '\0'; }
    while (*p && isspace((unsigned char)*p)) p++;
    *arg_out = p;
    return has_cmd ? 1 : 0;
}

// Case-insensitive string comparison
static int str_icmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// ---------------- Command Implementations ----------------
static void cmd_login(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: LOGIN <username> <password>");
        return;
    }
    
    char username[64], password[128];
    if (sscanf(args, "%63s %127s", username, password) != 2) {
        gui_println("Usage: LOGIN <username> <password>");
        return;
    }
    
    if (authenticate_user(username, password)) {
        gui_printf("Successfully logged in as %s", username);
        
        
        // Update current user
        strncpy(g_currentUser, username, sizeof(g_currentUser) - 1);
        
        // Switch to user's home directory
        Directory* user_dir = fs_find_child(g_root, username);
        if (user_dir) {
            g_home = user_dir;
            g_cwd = user_dir;
        }
        
        // Check if this was a system maintenance authentication attempt
        if (strcmp(username, "Admin") == 0) {
            gui_println("");
            gui_println("Admin authentication successful.");
            gui_println("System maintenance tools available: CD .system_maintenance");
        }
    } else {
        gui_println("Login failed. Invalid username or password.");
    }
}

static void cmd_logout(void) {
    if (!g_currentSession.is_authenticated) {
        gui_println("No active session to logout from.");
        return;
    }
    
    gui_printf("Logged out from %s", g_currentSession.username);
    
    logout_user();
    
    // Switch back to Public user
    strncpy(g_currentUser, "Public", sizeof(g_currentUser) - 1);
    Directory* public_dir = fs_find_child(g_root, "Public");
    if (public_dir) {
        g_home = public_dir;
        g_cwd = public_dir;
    }
}

static void cmd_chpasswd(const char* args) {
    if (!g_currentSession.is_authenticated) {
        gui_println("You must be logged in to change password.");
        return;
    }
    
    if (!args || !*args) {
        gui_println("Usage: CHPASSWD <old_password> <new_password>");
        return;
    }
    
    char old_password[128], new_password[128];
    if (sscanf(args, "%127s %127s", old_password, new_password) != 2) {
        gui_println("Usage: CHPASSWD <old_password> <new_password>");
        return;
    }
    
    if (strlen(new_password) < 6) {
        gui_println("Password must be at least 6 characters long.");
        return;
    }
    
    if (change_password(g_currentSession.username, old_password, new_password)) {
        gui_println("Password changed successfully.");
    } else {
        gui_println("Failed to change password. Check your current password.");
    }
}

static void cmd_sessions(void) {
    if (!has_privilege(1)) {
        gui_println("Insufficient privileges. Admin access required.");
        return;
    }
    
    gui_println("Active Sessions:");
    gui_println("===============");
    
    if (g_currentSession.is_authenticated) {
        time_t now = time(NULL);
        int session_duration = (int)(now - g_currentSession.session_start);
        
        gui_printf("User: %s | Session ID: %.8s... | Duration: %d seconds | Level: %d",
                g_currentSession.username,
                g_currentSession.session_id,
                session_duration,
                g_currentSession.privilege_level);
    } else {
        gui_println("No active sessions.");
    }
}

static void cmd_theme(const char* args) {
    if (!args || !*args) {
        gui_println("Available themes:");
        for (int i = 0; i < g_themeCount; i++) {
            gui_printf("  %s", g_themes[i].name);
        }
        gui_println("Usage: THEME <theme_name>");
        return;
    }
    
    if (strcmp(args, "LIST") == 0) {
        gui_println("Available themes:");
        for (int i = 0; i < g_themeCount; i++) {
            gui_printf("  %s", g_themes[i].name);
        }
        return;
    }
    
    apply_theme_silent(args, TRUE);
}

static void cmd_settings(const char* args) {
    if (!args || !*args) {
        gui_println("Current Settings:");
        gui_println("=================");
        gui_printf("Theme: %s", g_settings.current_theme);
        gui_printf("Font: %s (%dpt)", g_settings.font_name, g_settings.font_size);
        gui_printf("Auto Sync: %s", g_settings.auto_sync_enabled ? "Enabled" : "Disabled");
        gui_println("");
        gui_println("Quick Commands:");
        gui_println("  THEME <name>     - Change theme (classic, white, dark)");
        gui_println("  SETTINGS RESET   - Reset to defaults");
        return;
    }
    
    if (strcmp(args, "RESET") == 0) {
        // Reset to defaults
        strcpy(g_settings.current_theme, "classic");
        g_settings.cursor_blink_speed = 500;
        g_settings.font_size = 14;
        strcpy(g_settings.font_name, "Consolas");
        g_settings.auto_sync_enabled = TRUE;
        g_settings.show_hidden_files = FALSE;
        strcpy(g_settings.default_editor, "notepad");
        g_settings.max_history_size = 100;
        g_settings.sound_enabled = TRUE;
        g_settings.window_width = 800;
        g_settings.window_height = 600;
        g_settings.require_auth_for_admin = TRUE;
        g_settings.session_timeout = 30;
        
        save_settings();
        apply_theme("classic");
        gui_println("Settings reset to defaults.");
    } else {
        gui_println("Usage: SETTINGS [RESET]");
    }
}

static void cmd_set(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: SET <setting> <value>");
        gui_println("Available settings: current_theme, font_size, font_name, cursor_blink_speed, auto_sync_enabled, show_hidden_files, default_editor, window_width, window_height, session_timeout, require_auth_for_admin");
        return;
    }
    
    char setting[64], value[128];
    if (sscanf(args, "%63s %127s", setting, value) != 2) {
        gui_println("Usage: SET <setting> <value>");
        return;
    }
    
    BOOL changed = FALSE;
    
    if (strcmp(setting, "current_theme") == 0) {
        strncpy(g_settings.current_theme, value, sizeof(g_settings.current_theme) - 1);
        apply_theme(value);
        changed = TRUE;
    } else if (strcmp(setting, "font_size") == 0) {
        int size = atoi(value);
        if (size >= 8 && size <= 72) {
            g_settings.font_size = size;
            changed = TRUE;
        } else {
            gui_println("Font size must be between 8 and 72.");
            return;
        }
    } else if (strcmp(setting, "font_name") == 0) {
        strncpy(g_settings.font_name, value, sizeof(g_settings.font_name) - 1);
        changed = TRUE;
    } else if (strcmp(setting, "cursor_blink_speed") == 0) {
        int speed = atoi(value);
        if (speed >= 100 && speed <= 2000) {
            g_settings.cursor_blink_speed = speed;
            changed = TRUE;
        } else {
            gui_println("Cursor speed must be between 100 and 2000 milliseconds.");
            return;
        }
    } else if (strcmp(setting, "auto_sync_enabled") == 0) {
        g_settings.auto_sync_enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        changed = TRUE;
    } else if (strcmp(setting, "show_hidden_files") == 0) {
        g_settings.show_hidden_files = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        changed = TRUE;
    } else if (strcmp(setting, "default_editor") == 0) {
        strncpy(g_settings.default_editor, value, sizeof(g_settings.default_editor) - 1);
        changed = TRUE;
    } else if (strcmp(setting, "window_width") == 0) {
        int width = atoi(value);
        if (width >= 400 && width <= 2000) {
            g_settings.window_width = width;
            changed = TRUE;
        } else {
            gui_println("Window width must be between 400 and 2000 pixels.");
            return;
        }
    } else if (strcmp(setting, "window_height") == 0) {
        int height = atoi(value);
        if (height >= 300 && height <= 1500) {
            g_settings.window_height = height;
            changed = TRUE;
        } else {
            gui_println("Window height must be between 300 and 1500 pixels.");
            return;
        }
    } else if (strcmp(setting, "session_timeout") == 0) {
        int timeout = atoi(value);
        if (timeout >= 5 && timeout <= 480) {
            g_settings.session_timeout = timeout;
            changed = TRUE;
        } else {
            gui_println("Session timeout must be between 5 and 480 minutes.");
            return;
        }
    } else if (strcmp(setting, "require_auth_for_admin") == 0) {
        g_settings.require_auth_for_admin = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        changed = TRUE;
    } else {
        gui_println("Unknown setting. Use SETTINGS to see available options.");
        return;
    }
    
    if (changed) {
        save_settings();
        gui_printf("Setting '%s' updated to '%s'", setting, value);
    }
}

static void cmd_get(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: GET <setting>");
        return;
    }
    
    if (strcmp(args, "current_theme") == 0) {
        gui_println(g_settings.current_theme);
    } else if (strcmp(args, "font_size") == 0) {
        gui_printf("%d", g_settings.font_size);
    } else if (strcmp(args, "font_name") == 0) {
        gui_println(g_settings.font_name);
    } else if (strcmp(args, "cursor_blink_speed") == 0) {
        gui_printf("%d", g_settings.cursor_blink_speed);
    } else if (strcmp(args, "auto_sync_enabled") == 0) {
        gui_println(g_settings.auto_sync_enabled ? "true" : "false");
    } else if (strcmp(args, "show_hidden_files") == 0) {
        gui_println(g_settings.show_hidden_files ? "true" : "false");
    } else if (strcmp(args, "default_editor") == 0) {
        gui_println(g_settings.default_editor);
    } else if (strcmp(args, "window_width") == 0) {
        gui_printf("%d", g_settings.window_width);
    } else if (strcmp(args, "window_height") == 0) {
        gui_printf("%d", g_settings.window_height);
    } else if (strcmp(args, "session_timeout") == 0) {
        gui_printf("%d", g_settings.session_timeout);
    } else if (strcmp(args, "require_auth_for_admin") == 0) {
        gui_println(g_settings.require_auth_for_admin ? "true" : "false");
    } else {
        gui_println("Unknown setting. Use SETTINGS to see available options.");
    }
}

static void cmd_setup_auth(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: SETUP_AUTH <username> <password>");
        gui_println("Sets up authentication for a user account.");
        gui_println("Example: SETUP_AUTH Public mypassword123");
        gui_println("         SETUP_AUTH Admin admin123");
        return;
    }
    
    char username[64], password[64];
    if (sscanf(args, "%63s %63s", username, password) != 2) {
        gui_println("Usage: SETUP_AUTH <username> <password>");
        return;
    }
    
    // Check if user exists in filesystem
    Directory* user_dir = fs_find_child(g_root, username);
    if (!user_dir) {
        gui_printf("Error: User '%s' does not exist.", username);
        gui_println("Use ADDUSER to create the user first.");
        return;
    }
    
    // Check if user already has authentication
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, username) == 0) {
            gui_printf("User '%s' already has authentication setup.", username);
            gui_println("Use CHPASSWD to change password.");
            return;
        }
    }
    
    // Create user account with authentication
    if (create_user_account(username, password, 0)) { // Default privilege level 0
        // Save authentication to user's Settings folder
        char program_dir[1024];
        get_main_project_dir(program_dir, sizeof(program_dir));
        
        char user_auth_file[1024];
        snprintf(user_auth_file, sizeof(user_auth_file), "%s\\data\\USERS\\%s\\Settings\\auth.dat", program_dir, username);
        
        // Create Settings directory if it doesn't exist
        char settings_dir[1024];
        snprintf(settings_dir, sizeof(settings_dir), "%s\\data\\USERS\\%s\\Settings", program_dir, username);
        CreateDirectoryA(settings_dir, NULL);
        
        // Save authentication data
        FILE* f = fopen(user_auth_file, "w");
        if (f) {
            // Find the user in our auth array
            for (int i = 0; i < g_authCount; i++) {
                if (strcmp(g_userAuth[i].username, username) == 0) {
                    fprintf(f, "USER:%s|%s|%s|%ld|%ld|%d|%ld|%d|%d\n",
                        g_userAuth[i].username,
                        g_userAuth[i].password_hash,
                        g_userAuth[i].salt,
                        g_userAuth[i].last_login,
                        g_userAuth[i].password_changed,
                        g_userAuth[i].failed_attempts,
                        g_userAuth[i].locked_until,
                        g_userAuth[i].privilege_level,
                        g_userAuth[i].is_active ? 1 : 0);
                    break;
                }
            }
            fclose(f);
        }
        
        // Also create a virtual file in the Settings folder
        Directory* settings_dir_virtual = fs_find_child(user_dir, "Settings");
        if (settings_dir_virtual) {
            File* auth_file_virtual = fs_create_file("auth.dat");
            if (auth_file_virtual) {
                // Find the user in our auth array
                for (int i = 0; i < g_authCount; i++) {
                    if (strcmp(g_userAuth[i].username, username) == 0) {
                        snprintf(auth_file_virtual->content, sizeof(auth_file_virtual->content),
                            "USER:%s|%s|%s|%ld|%ld|%d|%ld|%d|%d",
                            g_userAuth[i].username,
                            g_userAuth[i].password_hash,
                            g_userAuth[i].salt,
                            g_userAuth[i].last_login,
                            g_userAuth[i].password_changed,
                            g_userAuth[i].failed_attempts,
                            g_userAuth[i].locked_until,
                            g_userAuth[i].privilege_level,
                            g_userAuth[i].is_active ? 1 : 0);
                        break;
                    }
                }
                fs_add_file(settings_dir_virtual, auth_file_virtual);
            }
        }
        
        gui_printf("Authentication setup complete for user '%s'", username);
        gui_println("You can now use LOGIN to authenticate before switching users.");
    } else {
        gui_println("Failed to setup authentication. Maximum accounts reached.");
    }
}

static BOOL process_command(char* input) {
    char* arg = NULL;
    if (!parse_first_token(input, &arg)) return TRUE;
    if (str_icmp(input, "help") == 0) { cmd_help(); }
    else if (str_icmp(input, "pwd") == 0) { cmd_pwd(); }
    else if (str_icmp(input, "dir") == 0 || str_icmp(input, "ls") == 0) { cmd_dir(); }
    else if (str_icmp(input, "mkdir") == 0 || str_icmp(input, "md") == 0) { cmd_mkdir(arg); }
    else if (str_icmp(input, "touch") == 0) { cmd_touch(arg); }
    else if (str_icmp(input, "del") == 0 || str_icmp(input, "delete") == 0) { cmd_del(arg); }
    else if (str_icmp(input, "rmdir") == 0 || str_icmp(input, "rd") == 0) { cmd_rmdir(arg); }
    else if (str_icmp(input, "softdel") == 0) { cmd_softdel(arg); }
    else if (str_icmp(input, "restore") == 0) { cmd_restore(arg); }
    else if (str_icmp(input, "trash") == 0) { cmd_trash(); }
    else if (str_icmp(input, "emptytrash") == 0) { cmd_emptytrash(); }
    else if (str_icmp(input, "type") == 0 || str_icmp(input, "cat") == 0) { cmd_type(arg); }
    else if (str_icmp(input, "write") == 0) { cmd_write(arg); }
    else if (str_icmp(input, "writeln") == 0) { cmd_writeln(arg); }
    else if (str_icmp(input, "writecode") == 0) { cmd_writecode(arg); }
    else if (str_icmp(input, "editcode") == 0) { cmd_editcode(arg); }
    else if (str_icmp(input, "append") == 0) { cmd_append(arg); }
    else if (str_icmp(input, "cd") == 0) { cmd_cd(arg); }
    else if (str_icmp(input, "echo") == 0) { cmd_echo(arg); }
    else if (str_icmp(input, "savefs") == 0) { cmd_savefs(arg); }
    else if (str_icmp(input, "user") == 0) { cmd_user(arg); }
    else if (str_icmp(input, "adduser") == 0) { cmd_adduser(arg); }
    else if (str_icmp(input, "renameuser") == 0) { cmd_renameuser(arg); }
    else if (str_icmp(input, "textcolor") == 0) { cmd_textcolor(arg); }
    else if (str_icmp(input, "cursorcolor") == 0) { cmd_cursorcolor(arg); }
    else if (str_icmp(input, "whoami") == 0) { cmd_whoami(); }
    else if (str_icmp(input, "users") == 0) { cmd_users(); }
    else if (str_icmp(input, "fileview") == 0) { cmd_fileview(); }
    else if (str_icmp(input, "sync") == 0) { cmd_sync(); }
    else if (str_icmp(input, "save") == 0) { fs_save_to_disk(); }
    else if (str_icmp(input, "ide") == 0) { 
        if (arg && str_icmp(arg, "list") == 0) { cmd_ide_list(); }
        else if (arg && str_icmp(arg, "help") == 0) { cmd_ide_help(); }
        else { cmd_ide(arg); }
    }
    else if (str_icmp(input, "git") == 0) {
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
    // Project Detection Commands
    else if (str_icmp(input, "project_type") == 0) { cmd_project_type(); }
    else if (str_icmp(input, "project_info") == 0) { cmd_project_info(); }
    // Test command
    else if (str_icmp(input, "test") == 0) { gui_println("Test command works!"); }
    // npm/Node.js Commands
    else if (str_icmp(input, "npm") == 0) {
        if (!arg) { cmd_npm_help(); }
        else if (str_icmp(arg, "install") == 0) { 
            // Get remaining arguments after install
            char* remaining = strstr(input, "install");
            if (!remaining) remaining = strstr(input, "INSTALL");
            if (remaining) {
                remaining += 7; // Skip "install"
                while (*remaining == ' ') remaining++; // Skip spaces
                cmd_npm_install(remaining);
            } else {
                cmd_npm_install("");
            }
        }
        else if (str_icmp(arg, "add") == 0) { 
            char* remaining = strstr(input, "add");
            if (!remaining) remaining = strstr(input, "ADD");
            if (remaining) {
                remaining += 3;
                while (*remaining == ' ') remaining++;
                cmd_npm_add(remaining);
            } else {
                cmd_npm_add("");
            }
        }
        else if (str_icmp(arg, "remove") == 0) { 
            char* remaining = strstr(input, "remove");
            if (!remaining) remaining = strstr(input, "REMOVE");
            if (remaining) {
                remaining += 6;
                while (*remaining == ' ') remaining++;
                cmd_npm_remove(remaining);
            } else {
                cmd_npm_remove("");
            }
        }
        else if (str_icmp(arg, "update") == 0) { 
            char* remaining = strstr(input, "update");
            if (!remaining) remaining = strstr(input, "UPDATE");
            if (remaining) {
                remaining += 6;
                while (*remaining == ' ') remaining++;
                cmd_npm_update(remaining);
            } else {
                cmd_npm_update("");
            }
        }
        else if (str_icmp(arg, "audit") == 0) { 
            char* remaining = strstr(input, "audit");
            if (!remaining) remaining = strstr(input, "AUDIT");
            if (remaining) {
                remaining += 5;
                while (*remaining == ' ') remaining++;
                cmd_npm_audit(remaining);
            } else {
                cmd_npm_audit("");
            }
        }
        else if (str_icmp(arg, "outdated") == 0) { cmd_npm_outdated(); }
        else if (str_icmp(arg, "run") == 0) { 
            char* remaining = strstr(input, "run");
            if (!remaining) remaining = strstr(input, "RUN");
            if (remaining) {
                remaining += 3;
                while (*remaining == ' ') remaining++;
                cmd_npm_run(remaining);
            } else {
                cmd_npm_run("");
            }
        }
        else if (str_icmp(arg, "start") == 0) { cmd_npm_start(); }
        else if (str_icmp(arg, "build") == 0) { cmd_npm_build(); }
        else if (str_icmp(arg, "test") == 0) { cmd_npm_test(); }
        else if (str_icmp(arg, "dev") == 0) { cmd_npm_dev(); }
        else if (str_icmp(arg, "init") == 0) { 
            char* remaining = strstr(input, "init");
            if (!remaining) remaining = strstr(input, "INIT");
            if (remaining) {
                remaining += 4;
                while (*remaining == ' ') remaining++;
                cmd_npm_init(remaining);
            } else {
                cmd_npm_init("");
            }
        }
        else if (str_icmp(arg, "publish") == 0) { 
            char* remaining = strstr(input, "publish");
            if (!remaining) remaining = strstr(input, "PUBLISH");
            if (remaining) {
                remaining += 7;
                while (*remaining == ' ') remaining++;
                cmd_npm_publish(remaining);
            } else {
                cmd_npm_publish("");
            }
        }
        else if (str_icmp(arg, "link") == 0) { 
            char* remaining = strstr(input, "link");
            if (!remaining) remaining = strstr(input, "LINK");
            if (remaining) {
                remaining += 4;
                while (*remaining == ' ') remaining++;
                cmd_npm_link(remaining);
            } else {
                cmd_npm_link("");
            }
        }
        else if (str_icmp(arg, "list") == 0) { cmd_npm_list(); }
        else if (str_icmp(arg, "help") == 0) { cmd_npm_help(); }
        else { cmd_npm_help(); }
    }
    // React Development Commands
    else if (str_icmp(input, "react") == 0) {
        if (!arg) { gui_println("Usage: REACT <command> [args]"); }
        else if (str_icmp(arg, "create") == 0) { 
            char* remaining = strstr(input, "create");
            if (!remaining) remaining = strstr(input, "CREATE");
            if (remaining) {
                remaining += 6;
                while (*remaining == ' ') remaining++;
                cmd_react_create(remaining);
            } else {
                cmd_react_create("");
            }
        }
        else if (str_icmp(arg, "component") == 0) { 
            char* remaining = strstr(input, "component");
            if (!remaining) remaining = strstr(input, "COMPONENT");
            if (remaining) {
                remaining += 9;
                while (*remaining == ' ') remaining++;
                cmd_react_component(remaining);
            } else {
                cmd_react_component("");
            }
        }
        else if (str_icmp(arg, "start") == 0) { cmd_react_start(); }
        else if (str_icmp(arg, "build") == 0) { cmd_react_build(); }
        else if (str_icmp(arg, "test") == 0) { cmd_react_test(); }
        else if (str_icmp(arg, "eject") == 0) { cmd_react_eject(); }
        else if (str_icmp(arg, "lint") == 0) { cmd_react_lint(); }
        else { gui_println("Unknown React command. Use REACT HELP for available commands."); }
    }
    // C++ and Python Execution Commands
    else if (str_icmp(input, "run") == 0) { cmd_run(arg); }
    else if (str_icmp(input, "compile") == 0) { cmd_compile(arg); }
    // System Maintenance Commands
    else if (str_icmp(input, "system_ls") == 0) { cmd_system_ls(arg); }
    else if (str_icmp(input, "system_cd") == 0) { cmd_system_cd(arg); }
    else if (str_icmp(input, "system_cat") == 0) { cmd_system_cat(arg); }
    else if (str_icmp(input, "system_copy") == 0) { cmd_system_copy(arg); }
    else if (str_icmp(input, "system_exit") == 0) { cmd_system_exit(); }
    else if (str_icmp(input, "system_log") == 0) { cmd_system_log(); }
    // Authentication Commands
    else if (str_icmp(input, "login") == 0) { cmd_login(arg); }
    else if (str_icmp(input, "logout") == 0) { cmd_logout(); }
    else if (str_icmp(input, "chpasswd") == 0) { cmd_chpasswd(arg); }
    else if (str_icmp(input, "sessions") == 0) { cmd_sessions(); }
    // Theme & Settings Commands
    else if (str_icmp(input, "theme") == 0) { cmd_theme(arg); }
    else if (str_icmp(input, "settings") == 0) { cmd_settings(arg); }
    else if (str_icmp(input, "set") == 0) { cmd_set(arg); }
    else if (str_icmp(input, "get") == 0) { cmd_get(arg); }
    else if (str_icmp(input, "setup_auth") == 0) { cmd_setup_auth(arg); }
    else if (str_icmp(input, "cls") == 0 || str_icmp(input, "clear") == 0) { gui_clear(); }
    else if (str_icmp(input, "exit") == 0 || str_icmp(input, "quit") == 0) { return FALSE; }
    else { gui_println("'COMMAND' is not recognized."); }
    return TRUE;
}

// Scroll handling functions (using built-in edit control)

// User management functions
static void cmd_renameuser(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: RENAMEUSER <old_name> <new_name>");
        gui_println("Example: RENAMEUSER Developer Coder");
        return;
    }
    
    char oldName[64], newName[64];
    if (sscanf(args, "%63s %63s", oldName, newName) != 2) {
        gui_println("Usage: RENAMEUSER <old_name> <new_name>");
        return;
    }
    
    // Validate new name (letters, numbers, underscores only)
    for (int i = 0; newName[i]; i++) {
        if (!isalnum(newName[i]) && newName[i] != '_') {
            gui_println("Error: Username can only contain letters, numbers, and underscores");
            return;
        }
    }
    
    // Check if old user exists
    BOOL oldUserFound = FALSE;
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, oldName) == 0) {
            oldUserFound = TRUE;
            break;
        }
    }
    
    if (!oldUserFound) {
        gui_println("Error: User not found");
        return;
    }
    
    // Check if new name already exists
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, newName) == 0) {
            gui_println("Error: Username already exists");
            return;
        }
    }
    
    // Find and update the user
    for (int i = 0; i < g_authCount; i++) {
        if (strcmp(g_userAuth[i].username, oldName) == 0) {
            strcpy(g_userAuth[i].username, newName);
            
            // Update current user if it's the one being renamed
            if (strcmp(g_currentUser, oldName) == 0) {
                strcpy(g_currentUser, newName);
            }
            
            // Update session username if it's the current session
            if (strcmp(g_currentSession.username, oldName) == 0) {
                strcpy(g_currentSession.username, newName);
            }
            
            // Update virtual filesystem - rename the user directory
            Directory* old_user_dir = fs_find_child(g_root, oldName);
            if (old_user_dir) {
                strcpy(old_user_dir->name, newName);
            }
            
            // Update home directory if it's the current user
            if (g_home == old_user_dir) {
                g_home = old_user_dir;
            }
            
            // Update current working directory if it's the current user
            if (g_cwd == old_user_dir) {
                g_cwd = old_user_dir;
            }
            
            // Rename the user directory in the real filesystem
            char program_dir[1024];
            get_main_project_dir(program_dir, sizeof(program_dir));
            
            char old_dir[1024], new_dir[1024];
            snprintf(old_dir, sizeof(old_dir), "%s\\data\\USERS\\%s", program_dir, oldName);
            snprintf(new_dir, sizeof(new_dir), "%s\\data\\USERS\\%s", program_dir, newName);
            
            // Rename the directory
            if (MoveFileA(old_dir, new_dir)) {
                gui_printf("User directory renamed from %s to %s\n", oldName, newName);
            } else {
                gui_printf("Warning: Could not rename user directory, but user data updated\n");
            }
            
            // Save the changes
            save_auth_data();
            
            // Update the prompt display
            gui_show_prompt_and_arm_input();
            
            gui_printf("User '%s' renamed to '%s' successfully\n", oldName, newName);
            return;
        }
    }
    
    gui_println("Error: User not found");
}

// Color customization commands
static void cmd_textcolor(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: TEXTCOLOR <color>");
        gui_println("Colors: red, green, blue, yellow, cyan, magenta, white, black, orange, purple");
        gui_println("Or use hex: TEXTCOLOR #FF0000 (for red)");
        gui_println("Use 'default' to use theme color");
        return;
    }
    
    COLORREF color = 0;
    
    if (strcmp(args, "default") == 0) {
        g_settings.custom_text_color = 0;
        gui_println("Text color reset to theme default");
        save_settings();
        InvalidateRect(g_hOut, NULL, TRUE);
        return;
    }
    
    // Parse hex color
    if (args[0] == '#') {
        color = (COLORREF)strtoul(args + 1, NULL, 16);
    } else {
        // Parse named colors
        if (strcmp(args, "red") == 0) color = RGB(255, 0, 0);
        else if (strcmp(args, "green") == 0) color = RGB(0, 255, 0);
        else if (strcmp(args, "blue") == 0) color = RGB(0, 0, 255);
        else if (strcmp(args, "yellow") == 0) color = RGB(255, 255, 0);
        else if (strcmp(args, "cyan") == 0) color = RGB(0, 255, 255);
        else if (strcmp(args, "magenta") == 0) color = RGB(255, 0, 255);
        else if (strcmp(args, "white") == 0) color = RGB(255, 255, 255);
        else if (strcmp(args, "black") == 0) color = RGB(0, 0, 0);
        else if (strcmp(args, "orange") == 0) color = RGB(255, 165, 0);
        else if (strcmp(args, "purple") == 0) color = RGB(128, 0, 128);
        else {
            gui_println("Invalid color. Use: red, green, blue, yellow, cyan, magenta, white, black, orange, purple");
            return;
        }
    }
    
    g_settings.custom_text_color = color;
    save_settings();
    gui_printf("Text color changed to %s\n", args);
    InvalidateRect(g_hOut, NULL, TRUE);
}

static void cmd_cursorcolor(const char* args) {
    if (!args || !*args) {
        gui_println("Usage: CURSORCOLOR <color>");
        gui_println("Colors: red, green, blue, yellow, cyan, magenta, white, black, orange, purple");
        gui_println("Or use hex: CURSORCOLOR #FF0000 (for red)");
        gui_println("Use 'default' to use theme color");
        return;
    }
    
    COLORREF color = 0;
    
    if (strcmp(args, "default") == 0) {
        g_settings.custom_cursor_color = 0;
        gui_println("Cursor color reset to theme default");
        save_settings();
        InvalidateRect(g_hOut, NULL, TRUE);
        return;
    }
    
    // Parse hex color
    if (args[0] == '#') {
        color = (COLORREF)strtoul(args + 1, NULL, 16);
    } else {
        // Parse named colors
        if (strcmp(args, "red") == 0) color = RGB(255, 0, 0);
        else if (strcmp(args, "green") == 0) color = RGB(0, 255, 0);
        else if (strcmp(args, "blue") == 0) color = RGB(0, 0, 255);
        else if (strcmp(args, "yellow") == 0) color = RGB(255, 255, 0);
        else if (strcmp(args, "cyan") == 0) color = RGB(0, 255, 255);
        else if (strcmp(args, "magenta") == 0) color = RGB(255, 0, 255);
        else if (strcmp(args, "white") == 0) color = RGB(255, 255, 255);
        else if (strcmp(args, "black") == 0) color = RGB(0, 0, 0);
        else if (strcmp(args, "orange") == 0) color = RGB(255, 165, 0);
        else if (strcmp(args, "purple") == 0) color = RGB(128, 0, 128);
        else {
            gui_println("Invalid color. Use: red, green, blue, yellow, cyan, magenta, white, black, orange, purple");
            return;
        }
    }
    
    g_settings.custom_cursor_color = color;
    save_settings();
    gui_printf("Cursor color changed to %s\n", args);
    InvalidateRect(g_hOut, NULL, TRUE);
}

// Simplified output edit procedure
static LRESULT CALLBACK OutputEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int lines = 3; // Scroll 3 lines at a time
            
            if (delta > 0) {
                // Scroll up
                for (int i = 0; i < lines; i++) {
                    SendMessageA(hWnd, EM_SCROLL, SB_LINEUP, 0);
                }
            } else if (delta < 0) {
                // Scroll down
                for (int i = 0; i < lines; i++) {
                    SendMessageA(hWnd, EM_SCROLL, SB_LINEDOWN, 0);
                }
            }
                    return 0;
                }
        case WM_PAINT: {
            // Let default paint happen first
            LRESULT result = CallWindowProcA(g_OutputPrevProc, hWnd, msg, wParam, lParam);
            return result;
            }
        case WM_SETFOCUS: {
            int len = GetWindowTextLengthA(hWnd);
            SendMessageA(hWnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                return 0;
            }
        case WM_KEYDOWN: {
            // Handle arrow keys for command history
            if (wParam == VK_UP) {
                navigate_history(-1); // Go up in history
                return 0;
            } else if (wParam == VK_DOWN) {
                navigate_history(1); // Go down in history
                return 0;
            }
            
            // Ensure cursor stays at end when typing
            int len = GetWindowTextLengthA(hWnd);
            SendMessageA(hWnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            return CallWindowProcA(g_OutputPrevProc, hWnd, msg, wParam, lParam);
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            int len = GetWindowTextLengthA(hWnd);
            SendMessageA(hWnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            return 0;
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
                
                // Add command to history if it's not empty
                if (strlen(buf) > 0) {
                    add_to_history(buf);
                }
                
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
    DWORD outStyle = WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_TABSTOP;
    g_hOut = CreateWindowExA(0, "EDIT", "", outStyle, 0, 0, 800, 500, hWnd, (HMENU)1001, GetModuleHandle(NULL), NULL);

    g_hMono = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, "Consolas");
    SendMessageA(g_hOut, WM_SETFONT, (WPARAM)g_hMono, TRUE);

    g_OutputPrevProc = (WNDPROC)SetWindowLongPtrA(g_hOut, GWLP_WNDPROC, (LONG_PTR)OutputEditProc);
    SetFocus(g_hOut);
}

static void layout_children(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    MoveWindow(g_hOut, 0, 0, rc.right, rc.bottom, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            
            // Get current theme colors
            COLORREF text_color = RGB(0,255,0);  // Default green
            COLORREF bg_color = RGB(0,0,0);      // Default black
            
            for (int i = 0; i < g_themeCount; i++) {
                if (strcmp(g_themes[i].name, g_settings.current_theme) == 0) {
                    text_color = g_themes[i].text_color;
                    bg_color = g_themes[i].bg_color;
                    break;
                }
            }
            
            // Use custom text color if set
            if (g_settings.custom_text_color != 0) {
                text_color = g_settings.custom_text_color;
            }
            
            SetTextColor(hdc, text_color);
            SetBkColor(hdc, bg_color);
            
            // Recreate brush with current theme color
            if (g_hbrBlack) DeleteObject(g_hbrBlack);
            g_hbrBlack = CreateSolidBrush(bg_color);
            return (LRESULT)g_hbrBlack;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc; GetClientRect(hWnd, &rc);
            
            // Get current theme background color
            COLORREF bg_color = RGB(0,0,0);  // Default black
            for (int i = 0; i < g_themeCount; i++) {
                if (strcmp(g_themes[i].name, g_settings.current_theme) == 0) {
                    bg_color = g_themes[i].bg_color;
                    break;
                }
            }
            
            // Recreate brush with current theme color
            if (g_hbrBlack) DeleteObject(g_hbrBlack);
            g_hbrBlack = CreateSolidBrush(bg_color);
            FillRect(hdc, &rc, g_hbrBlack);
            return 1;
        }
        case WM_CREATE:
            create_child_controls(hWnd);
            fs_init();
            init_ide_configs();
            layout_children(hWnd);
            // Apply theme after GUI is fully initialized
            apply_theme(g_settings.current_theme);
            
            // Show title on first line
            gui_append("NEXUS TERMINAL v5.0");
            gui_append("\r\n");
            
            // Show command prompt on second line, right below title
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
            return 0;
        case WM_DESTROY:
            // Auto-save filesystem before closing
            fs_save_to_disk();
            
            if (g_hbrBlack) { DeleteObject(g_hbrBlack); g_hbrBlack = NULL; }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ================ NPM/NODE.JS FUNCTIONS ================

static void execute_system_command(const char* command, const char* args) {
    char full_command[1024];
    if (args && strlen(args) > 0) {
        snprintf(full_command, sizeof(full_command), "%s %s", command, args);
    } else {
        strncpy(full_command, command, sizeof(full_command) - 1);
    }
    
    // Get the current working directory for the command
    char current_dir[MAX_PATH];
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    // Build the real path based on current virtual directory
    if (g_cwd == g_home) {
        // We're in the user's home directory
        snprintf(current_dir, sizeof(current_dir), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
    } else {
        // We're in a subdirectory, build the full path
        char virtual_path[2048];
        fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
        
        // Find the user name in the virtual path and build the real path
        char* user_start = strstr(virtual_path, g_currentUser);
        if (user_start) {
            // Skip the user name and the leading slash
            char* path_after_user = user_start + strlen(g_currentUser);
            if (*path_after_user == '/') path_after_user++;
            
            snprintf(current_dir, sizeof(current_dir), "%s\\data\\USERS\\%s\\%s", 
                    program_dir, g_currentUser, path_after_user);
        } else {
            // Fallback to user home directory
            snprintf(current_dir, sizeof(current_dir), "%s\\data\\USERS\\%s", program_dir, g_currentUser);
        }
    }
    
    // Execute the command using CreateProcessA to hide the console window
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide the window
    
    char cmd_line[1024];
    snprintf(cmd_line, sizeof(cmd_line), "cmd.exe /c %s", full_command);
    
    BOOL success = CreateProcessA(
        NULL,           // Application name
        cmd_line,       // Command line
        NULL,           // Process security attributes
        NULL,           // Thread security attributes
        FALSE,          // Inherit handles
        CREATE_NO_WINDOW, // Creation flags - no window
        NULL,           // Environment
        current_dir,    // Current directory - set to virtual directory location
        &si,            // Startup info
        &pi             // Process information
    );
    
    if (success) {
        // Check if this is a long-running command (like npm dev, npm start)
        int is_long_running = 0;
        if (strstr(full_command, "npm dev") || strstr(full_command, "npm start") || 
            strstr(full_command, "npm run dev") || strstr(full_command, "npm run start")) {
            is_long_running = 1;
        }
        
        if (is_long_running) {
            // For long-running commands, don't wait - just start and return
            gui_println("Development server started successfully.");
            gui_println("Note: Server is running in background. Use Ctrl+C in the server window to stop.");
            
            // Close handles immediately for long-running processes
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            // For regular commands, wait for completion
            WaitForSingleObject(pi.hProcess, INFINITE);
            
            DWORD exit_code;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            
            // Close handles
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            if (exit_code == 0) {
                gui_println("Command completed successfully.");
            } else {
                gui_printf("Command failed with exit code: %d\n", exit_code);
            }
        }
    } else {
        gui_println("Failed to execute command.");
    }
}

static void execute_npm_command(const char* npm_args) {
    char command[1024];
    snprintf(command, sizeof(command), "npm %s", npm_args);
    execute_system_command(command, "");
}

// npm Package Management Commands
static void cmd_npm_install(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Installing all dependencies...");
        execute_npm_command("install");
    } else {
        gui_printf("Installing package: %s\n", args);
        char npm_cmd[512];
        snprintf(npm_cmd, sizeof(npm_cmd), "install %s", args);
        execute_npm_command(npm_cmd);
    }
}

static void cmd_npm_add(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Usage: NPM ADD <package> [--save-dev]");
        gui_println("Example: NPM ADD react");
        gui_println("Example: NPM ADD eslint --save-dev");
        return;
    }
    gui_printf("Adding package: %s\n", args);
    char npm_cmd[512];
    snprintf(npm_cmd, sizeof(npm_cmd), "install %s", args);
    execute_npm_command(npm_cmd);
}

static void cmd_npm_remove(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Usage: NPM REMOVE <package>");
        gui_println("Example: NPM REMOVE react");
        return;
    }
    gui_printf("Removing package: %s\n", args);
    char npm_cmd[512];
    snprintf(npm_cmd, sizeof(npm_cmd), "uninstall %s", args);
    execute_npm_command(npm_cmd);
}

static void cmd_npm_update(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Updating all packages...");
        execute_npm_command("update");
    } else {
        gui_printf("Updating package: %s\n", args);
        char npm_cmd[512];
        snprintf(npm_cmd, sizeof(npm_cmd), "update %s", args);
        execute_npm_command(npm_cmd);
    }
}

static void cmd_npm_audit(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Running security audit...");
        execute_npm_command("audit");
    } else {
        gui_printf("Running audit with args: %s\n", args);
        char npm_cmd[512];
        snprintf(npm_cmd, sizeof(npm_cmd), "audit %s", args);
        execute_npm_command(npm_cmd);
    }
}

static void cmd_npm_outdated(void) {
    gui_println("Checking for outdated packages...");
    execute_npm_command("outdated");
}

static void cmd_npm_run(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Usage: NPM RUN <script>");
        gui_println("Example: NPM RUN start");
        gui_println("Example: NPM RUN build");
        return;
    }
    gui_printf("Running script: %s\n", args);
    char npm_cmd[512];
    snprintf(npm_cmd, sizeof(npm_cmd), "run %s", args);
    execute_npm_command(npm_cmd);
}

static void cmd_npm_start(void) {
    gui_println("Starting application...");
    execute_npm_command("start");
}

static void cmd_npm_build(void) {
    gui_println("Building application...");
    execute_npm_command("run build");
}

static void cmd_npm_test(void) {
    gui_println("Running tests...");
    execute_npm_command("test");
}

static void cmd_npm_dev(void) {
    gui_println("Starting development server...");
    execute_npm_command("run dev");
}

static void cmd_npm_init(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Initializing package.json...");
        execute_npm_command("init -y");
    } else {
        gui_printf("Initializing with args: %s\n", args);
        char npm_cmd[512];
        snprintf(npm_cmd, sizeof(npm_cmd), "init %s", args);
        execute_npm_command(npm_cmd);
    }
}

static void cmd_npm_publish(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Publishing package...");
        execute_npm_command("publish");
    } else {
        gui_printf("Publishing with args: %s\n", args);
        char npm_cmd[512];
        snprintf(npm_cmd, sizeof(npm_cmd), "publish %s", args);
        execute_npm_command(npm_cmd);
    }
}

static void cmd_npm_link(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Linking package...");
        execute_npm_command("link");
    } else {
        gui_printf("Linking with args: %s\n", args);
        char npm_cmd[512];
        snprintf(npm_cmd, sizeof(npm_cmd), "link %s", args);
        execute_npm_command(npm_cmd);
    }
}

static void cmd_npm_list(void) {
    // Check if we're in a directory with package.json
    File* package_json = fs_find_file(g_cwd, "package.json");
    if (!package_json) {
        gui_println("No package.json found in current directory.");
        gui_println("Showing globally installed packages instead...");
        execute_npm_command("list -g --depth=0");
        return;
    }
    
    gui_println("Listing installed packages...");
    execute_npm_command("list --depth=0");
}

static void cmd_npm_help(void) {
    gui_println("npm Commands Help:");
    gui_println("==================");
    gui_println("NPM INSTALL            Install all dependencies");
    gui_println("NPM ADD <package>      Install a package");
    gui_println("NPM REMOVE <package>   Uninstall a package");
    gui_println("NPM UPDATE             Update all packages");
    gui_println("NPM AUDIT              Check for vulnerabilities");
    gui_println("NPM OUTDATED           Show outdated packages");
    gui_println("NPM RUN <script>       Run a package script");
    gui_println("NPM START              Run start script");
    gui_println("NPM BUILD              Run build script");
    gui_println("NPM TEST               Run test script");
    gui_println("NPM DEV                Run dev script");
    gui_println("NPM INIT               Initialize package.json");
    gui_println("NPM LIST               List installed packages");
    gui_println("NPM HELP               Show this help");
}

// ================ REACT DEVELOPMENT FUNCTIONS ================

static void cmd_react_create(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Usage: REACT CREATE <app-name>");
        gui_println("Example: REACT CREATE my-react-app");
        return;
    }
    gui_printf("Creating React app: %s\n", args);
    char command[512];
    snprintf(command, sizeof(command), "npx create-react-app %s", args);
    execute_system_command(command, "");
}

static void cmd_react_component(const char* args) {
    if (!args || strlen(args) == 0) {
        gui_println("Usage: REACT COMPONENT <component-name>");
        gui_println("Example: REACT COMPONENT Button");
        return;
    }
    gui_printf("Creating React component: %s\n", args);
    
    // Create component file
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.js", args);
    
    char component_code[1024];
    snprintf(component_code, sizeof(component_code),
        "import React from 'react';\n\n"
        "const %s = () => {\n"
        "  return (\n"
        "    <div>\n"
        "      <h2>%s Component</h2>\n"
        "    </div>\n"
        "  );\n"
        "};\n\n"
        "export default %s;",
        args, args, args);
    
    // Write component to virtual filesystem
    char write_args[1536];
    snprintf(write_args, sizeof(write_args), "%s %s", filename, component_code);
    cmd_writecode(write_args);
    gui_printf("Component %s created successfully!\n", args);
}

static void cmd_react_start(void) {
    gui_println("Starting React development server...");
    execute_npm_command("start");
}

static void cmd_react_build(void) {
    gui_println("Building React application...");
    execute_npm_command("run build");
}

static void cmd_react_test(void) {
    gui_println("Running React tests...");
    execute_npm_command("test");
}

static void cmd_react_eject(void) {
    gui_println("Ejecting from Create React App...");
    gui_println("WARNING: This action is irreversible!");
    execute_npm_command("run eject");
}

static void cmd_react_lint(void) {
    gui_println("Running ESLint...");
    execute_npm_command("run lint");
}

// ================ C++ & PYTHON EXECUTION FUNCTIONS ================

static void cmd_run(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        gui_println("Usage: RUN <filename>");
        gui_println("Example: RUN main.cpp");
        gui_println("Example: RUN script.py");
        return;
    }
    
    // Check if file exists
    File* file = fs_find_file(g_cwd, filename);
    if (!file) {
        gui_printf("File '%s' not found in current directory.\n", filename);
        return;
    }
    
    // Determine file type and execute
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        gui_println("File has no extension. Cannot determine type.");
        return;
    }
    
    if (str_icmp(ext, ".cpp") == 0 || str_icmp(ext, ".c") == 0) {
        execute_cpp_file(filename);
    } else if (str_icmp(ext, ".py") == 0) {
        execute_python_file(filename);
    } else {
        gui_printf("Unsupported file type: %s\n", ext);
        gui_println("Supported types: .cpp, .c, .py");
    }
}

static void cmd_compile(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        gui_println("Usage: COMPILE <filename>");
        gui_println("Example: COMPILE main.cpp");
        return;
    }
    
    // Check if file exists
    File* file = fs_find_file(g_cwd, filename);
    if (!file) {
        gui_printf("File '%s' not found in current directory.\n", filename);
        return;
    }
    
    // Check if it's a C++ file
    const char* ext = strrchr(filename, '.');
    if (!ext || (str_icmp(ext, ".cpp") != 0 && str_icmp(ext, ".c") != 0)) {
        gui_println("COMPILE command only works with .cpp and .c files.");
        return;
    }
    
    execute_cpp_file(filename);
}

static void execute_cpp_file(const char* filename) {
    gui_printf("Compiling and running: %s\n", filename);
    
    // Get the real file path
    char real_path[1024];
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    if (g_cwd == g_home) {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s\\%s", 
                program_dir, g_currentUser, filename);
    } else {
        char virtual_path[2048];
        fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
        
        char* user_start = strstr(virtual_path, g_currentUser);
        if (user_start) {
            char* path_after_user = user_start + strlen(g_currentUser);
            if (*path_after_user == '/') path_after_user++;
            
            snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s\\%s\\%s", 
                    program_dir, g_currentUser, path_after_user, filename);
        } else {
            snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s\\%s", 
                    program_dir, g_currentUser, filename);
        }
    }
    
    // Create executable name
    char exe_name[512];
    const char* ext = strrchr(filename, '.');
    if (ext) {
        strncpy(exe_name, filename, ext - filename);
        exe_name[ext - filename] = '\0';
    } else {
        strncpy(exe_name, filename, sizeof(exe_name) - 1);
    }
    
    // Compile command
    char compile_cmd[1024];
    snprintf(compile_cmd, sizeof(compile_cmd), "g++ -o %s.exe %s", exe_name, real_path);
    
    // Execute compilation
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    char cmd_line[1024];
    snprintf(cmd_line, sizeof(cmd_line), "g++ -o %s.exe %s", exe_name, real_path);
    
    // Get the directory where the file is located
    char working_dir[1024];
    strncpy(working_dir, real_path, sizeof(working_dir) - 1);
    working_dir[sizeof(working_dir) - 1] = '\0';
    char* last_slash = strrchr(working_dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    
    BOOL success = CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE, CREATE_NO_WINDOW, 
                                 NULL, working_dir, &si, &pi);
    
    if (success) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        if (exit_code == 0) {
            gui_println("Compilation successful!");
            
            // Run the executable
            char run_cmd[1024];
            snprintf(run_cmd, sizeof(run_cmd), "%s.exe", exe_name);
            
            STARTUPINFOA si2 = {0};
            PROCESS_INFORMATION pi2 = {0};
            si2.cb = sizeof(si2);
            si2.dwFlags = STARTF_USESHOWWINDOW;
            si2.wShowWindow = SW_SHOW; // Show the console for output
            
            char run_line[1024];
            snprintf(run_line, sizeof(run_line), "%s.exe", exe_name);
            
            BOOL run_success = CreateProcessA(NULL, run_line, NULL, NULL, FALSE, 0, 
                                            NULL, working_dir, &si2, &pi2);
            
            if (run_success) {
                WaitForSingleObject(pi2.hProcess, INFINITE);
                CloseHandle(pi2.hProcess);
                CloseHandle(pi2.hThread);
                gui_println("Program execution completed.");
            } else {
                gui_println("Failed to run executable.");
            }
        } else {
            gui_printf("Compilation failed with exit code: %d\n", exit_code);
        }
    } else {
        gui_println("Failed to start compilation process.");
    }
}

static void execute_python_file(const char* filename) {
    gui_printf("Running Python script: %s\n", filename);
    
    // Get the real file path
    char real_path[1024];
    char program_dir[1024];
    get_main_project_dir(program_dir, sizeof(program_dir));
    
    if (g_cwd == g_home) {
        snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s\\%s", 
                program_dir, g_currentUser, filename);
    } else {
        char virtual_path[2048];
        fs_print_path(g_cwd, virtual_path, sizeof(virtual_path));
        
        char* user_start = strstr(virtual_path, g_currentUser);
        if (user_start) {
            char* path_after_user = user_start + strlen(g_currentUser);
            if (*path_after_user == '/') path_after_user++;
            
            snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s\\%s\\%s", 
                    program_dir, g_currentUser, path_after_user, filename);
        } else {
            snprintf(real_path, sizeof(real_path), "%s\\data\\USERS\\%s\\%s", 
                    program_dir, g_currentUser, filename);
        }
    }
    
    // Try python3 first, then python
    char python_cmd[1024];
    snprintf(python_cmd, sizeof(python_cmd), "python3 %s", real_path);
    
    // Get the directory where the file is located
    char working_dir[1024];
    strncpy(working_dir, real_path, sizeof(working_dir) - 1);
    working_dir[sizeof(working_dir) - 1] = '\0';
    char* last_slash = strrchr(working_dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW; // Show the console for output
    
    char cmd_line[1024];
    snprintf(cmd_line, sizeof(cmd_line), "python3 %s", real_path);
    
    BOOL success = CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE, 0, 
                                 NULL, working_dir, &si, &pi);
    
    if (success) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        if (exit_code == 0) {
            gui_println("Python script executed successfully.");
        } else {
            gui_printf("Python script failed with exit code: %d\n", exit_code);
            gui_println("Trying with 'python' instead of 'python3'...");
            
            // Try with just 'python'
            snprintf(python_cmd, sizeof(python_cmd), "python %s", real_path);
            snprintf(cmd_line, sizeof(cmd_line), "python %s", real_path);
            
            STARTUPINFOA si2 = {0};
            PROCESS_INFORMATION pi2 = {0};
            si2.cb = sizeof(si2);
            si2.dwFlags = STARTF_USESHOWWINDOW;
            si2.wShowWindow = SW_SHOW;
            
            BOOL success2 = CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE, 0, 
                                         NULL, working_dir, &si2, &pi2);
            
            if (success2) {
                WaitForSingleObject(pi2.hProcess, INFINITE);
                GetExitCodeProcess(pi2.hProcess, &exit_code);
                CloseHandle(pi2.hProcess);
                CloseHandle(pi2.hThread);
                
                if (exit_code == 0) {
                    gui_println("Python script executed successfully with 'python'.");
                } else {
                    gui_printf("Python script failed with both 'python3' and 'python'.\n");
                }
            } else {
                gui_println("Failed to run Python script with both 'python3' and 'python'.");
            }
        }
    } else {
        gui_println("Failed to start Python process.");
    }
}

static int find_cpp_files(char files[][256], int max_files) {
    int count = 0;
    if (!g_cwd || !g_cwd->files) return 0;
    
    for (int i = 0; i < g_cwd->file_count && count < max_files; i++) {
        if (g_cwd->files[i]) {
            const char* name = g_cwd->files[i]->name;
            const char* ext = strrchr(name, '.');
            if (ext && (str_icmp(ext, ".cpp") == 0 || str_icmp(ext, ".c") == 0)) {
                strncpy(files[count], name, 255);
                files[count][255] = '\0';
                count++;
            }
        }
    }
    return count;
}

static int find_python_files(char files[][256], int max_files) {
    int count = 0;
    if (!g_cwd || !g_cwd->files) return 0;
    
    for (int i = 0; i < g_cwd->file_count && count < max_files; i++) {
        if (g_cwd->files[i]) {
            const char* name = g_cwd->files[i]->name;
            const char* ext = strrchr(name, '.');
            if (ext && str_icmp(ext, ".py") == 0) {
                strncpy(files[count], name, 255);
                files[count][255] = '\0';
                count++;
            }
        }
    }
    return count;
}

// ================ COMMAND HISTORY FUNCTIONS ================

static void add_to_history(const char* command) {
    // Don't add empty commands or duplicate of last command
    if (strlen(command) == 0) return;
    if (g_historyCount > 0 && strcmp(g_commandHistory[g_historyCount - 1], command) == 0) return;
    
    // Shift history if we're at max capacity
    if (g_historyCount >= MAX_HISTORY) {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(g_commandHistory[i], g_commandHistory[i + 1]);
        }
        g_historyCount = MAX_HISTORY - 1;
    }
    
    // Add new command
    strncpy(g_commandHistory[g_historyCount], command, 255);
    g_commandHistory[g_historyCount][255] = '\0';
    g_historyCount++;
    g_historyIndex = g_historyCount; // Reset to "new command" position
}

static void navigate_history(int direction) {
    if (g_historyCount == 0) return;
    
    // Save current input if we're at the "new command" position
    if (g_historyIndex == g_historyCount) {
        int totalLen = GetWindowTextLengthA(g_hOut);
        int cmdLen = totalLen - g_inputStart;
        if (cmdLen > 0 && cmdLen < 256) {
            char* all = (char*)malloc((size_t)totalLen + 1);
            if (all) {
                GetWindowTextA(g_hOut, all, totalLen + 1);
                memcpy(g_currentInput, all + g_inputStart, (size_t)cmdLen);
                g_currentInput[cmdLen] = '\0';
                free(all);
            }
        }
    }
    
    // Navigate through history
    if (direction < 0) { // Up arrow - go to older commands
        if (g_historyIndex > 0) {
            g_historyIndex--;
        }
    } else { // Down arrow - go to newer commands
        if (g_historyIndex < g_historyCount) {
            g_historyIndex++;
        }
    }
    
    // Update the input field
    if (g_historyIndex < g_historyCount) {
        update_input_field(g_commandHistory[g_historyIndex]);
    } else {
        update_input_field(g_currentInput);
    }
}

static void update_input_field(const char* text) {
    if (!g_hOut) return;
    
    // Get current text up to input start
    int totalLen = GetWindowTextLengthA(g_hOut);
    char* all = (char*)malloc((size_t)totalLen + 1);
    if (!all) return;
    
    GetWindowTextA(g_hOut, all, totalLen + 1);
    
    // Replace the input part with the new text
    int newLen = g_inputStart + (int)strlen(text);
    char* newText = (char*)malloc((size_t)newLen + 1);
    if (!newText) {
        free(all);
        return;
    }
    
    // Copy text up to input start
    memcpy(newText, all, (size_t)g_inputStart);
    // Copy the new command
    strcpy(newText + g_inputStart, text);
    
    // Update the edit control
    SetWindowTextA(g_hOut, newText);
    
    // Set cursor to end
    SendMessageA(g_hOut, EM_SETSEL, (WPARAM)newLen, (LPARAM)newLen);
    SendMessageA(g_hOut, EM_SCROLLCARET, 0, 0);
    
    free(all);
    free(newText);
}

// ================ SYSTEM MAINTENANCE FUNCTIONS ================
// These functions provide administrative access to system maintenance operations
// Used for debugging, file system access, and administrative tasks

static void create_system_maintenance_folder(void) {
    // Initialize system maintenance folder for admin operations
    Directory* admin_system = fs_find_child(g_root, "Admin");
    if (admin_system) {
        Directory* system_dir = fs_find_child(admin_system, "System");
        if (system_dir) {
            // Check if maintenance folder already exists
            Directory* existing_maintenance = fs_find_child(system_dir, ".system_maintenance");
            if (existing_maintenance) return;
            
            Directory* maintenance = fs_create_dir(".system_maintenance");
            if (maintenance) {
                fs_add_child(system_dir, maintenance);
                
                // Create maintenance configuration files
                File* access_log = fs_create_file(".access_log");
                File* permissions = fs_create_file(".permissions");
                File* config = fs_create_file(".config");
                
                if (access_log) {
                    strcpy(access_log->content, "System Maintenance Log\n=====================\n");
                    fs_add_file(maintenance, access_log);
                }
                
                if (permissions) {
                    strcpy(permissions->content, "Admin:full_access\n");
                    fs_add_file(maintenance, permissions);
                }
                
                if (config) {
                    strcpy(config->content, "session_timeout=1800\nlog_access=true\n");
                    fs_add_file(maintenance, config);
                }
            }
        }
    }
}

static BOOL verify_admin_credentials(const char* username, const char* password) {
    // Check if user is Admin and authenticated
    if (strcmp(username, "Admin") != 0) return FALSE;
    if (strcmp(g_currentUser, "Admin") != 0) return FALSE;
    
    // Verify password
    return authenticate_user(username, password);
}

static void cmd_system_ls(const char* path) {
    // System maintenance directory listing function
    if (!g_systemMaintenanceMode) {
        gui_println("Maintenance mode not active. Use CD .system_maintenance first.");
        return;
    }
    
    if (!is_system_session_valid()) {
        gui_println("Maintenance session expired. Please re-authenticate.");
        g_systemMaintenanceMode = FALSE;
        return;
    }
    
    char target_path[1024];
    if (!path || !*path) {
        strcpy(target_path, g_systemMaintenancePath);
    } else {
        strncpy(target_path, path, sizeof(target_path) - 1);
        target_path[sizeof(target_path) - 1] = '\0';
    }
    
    log_system_access("LS", target_path);
    
    WIN32_FIND_DATAA findData;
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", target_path);
    
    HANDLE hFind = FindFirstFileA(search_path, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        gui_println("Directory not found or access denied.");
        return;
    }
    
    gui_printf("Contents of %s:\n", target_path);
    gui_println("==================");
    
    do {
        if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                gui_printf("[DIR]  %s\n", findData.cFileName);
            } else {
                gui_printf("[FILE] %s\n", findData.cFileName);
            }
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}

static void cmd_system_cd(const char* path) {
    // System maintenance directory change function
    if (!g_systemMaintenanceMode) {
        gui_println("Maintenance mode not active. Use CD .system_maintenance first.");
        return;
    }
    
    if (!is_system_session_valid()) {
        gui_println("Maintenance session expired. Please re-authenticate.");
        g_systemMaintenanceMode = FALSE;
        return;
    }
    
    if (!path || !*path) {
        strcpy(g_systemMaintenancePath, "C:\\");
        gui_println("Changed to C:\\");
        return;
    }
    
    char new_path[1024];
    if (path[1] == ':') {
        // Absolute path
        strncpy(new_path, path, sizeof(new_path) - 1);
        new_path[sizeof(new_path) - 1] = '\0';
    } else {
        // Relative path
        snprintf(new_path, sizeof(new_path), "%s\\%s", g_systemMaintenancePath, path);
    }
    
    // Normalize path separators
    for (char* p = new_path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
    
    // Verify directory exists
    WIN32_FIND_DATAA findData;
    char test_path[1024];
    snprintf(test_path, sizeof(test_path), "%s\\*", new_path);
    
    HANDLE hFind = FindFirstFileA(test_path, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        gui_println("Directory not found or access denied.");
        return;
    }
    FindClose(hFind);
    
    strcpy(g_systemMaintenancePath, new_path);
    log_system_access("CD", new_path);
    gui_printf("Changed to %s\n", new_path);
}

static void cmd_system_cat(const char* path) {
    // System maintenance file display function
    if (!g_systemMaintenanceMode) {
        gui_println("Maintenance mode not active. Use CD .system_maintenance first.");
        return;
    }
    
    if (!is_system_session_valid()) {
        gui_println("Maintenance session expired. Please re-authenticate.");
        g_systemMaintenanceMode = FALSE;
        return;
    }
    
    if (!path || !*path) {
        gui_println("Usage: SYSTEM_CAT <file_path>");
        return;
    }
    
    char target_path[1024];
    if (path[1] == ':') {
        strncpy(target_path, path, sizeof(target_path) - 1);
        target_path[sizeof(target_path) - 1] = '\0';
    } else {
        snprintf(target_path, sizeof(target_path), "%s\\%s", g_systemMaintenancePath, path);
    }
    
    log_system_access("CAT", target_path);
    
    HANDLE hFile = CreateFileA(target_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        gui_println("File not found or access denied.");
        return;
    }
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize > 10240) { // 10KB limit for maintenance operations
        gui_println("File too large to display (over 10KB).");
        CloseHandle(hFile);
        return;
    }
    
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        gui_println("Memory allocation failed.");
        CloseHandle(hFile);
        return;
    }
    
    DWORD bytesRead;
    if (ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
        buffer[bytesRead] = '\0';
        gui_printf("Contents of %s:\n", target_path);
        gui_println("==================");
        gui_println(buffer);
    } else {
        gui_println("Failed to read file.");
    }
    
    free(buffer);
    CloseHandle(hFile);
}

static void cmd_system_copy(const char* args) {
    if (!g_systemMaintenanceMode) {
        gui_println("Maintenance mode not active. Use CD .system_maintenance first.");
        return;
    }
    
    if (!is_system_session_valid()) {
        gui_println("Maintenance session expired. Please re-authenticate.");
        g_systemMaintenanceMode = FALSE;
        return;
    }
    
    if (!args || !*args) {
        gui_println("Usage: BACKDOOR_COPY <source_path> <dest_name>");
        return;
    }
    
    char src_path[512], dest_name[256];
    if (sscanf(args, "%511s %255s", src_path, dest_name) != 2) {
        gui_println("Usage: BACKDOOR_COPY <source_path> <dest_name>");
        return;
    }
    
    char full_src_path[1024];
    if (src_path[1] == ':') {
        strncpy(full_src_path, src_path, sizeof(full_src_path) - 1);
        full_src_path[sizeof(full_src_path) - 1] = '\0';
    } else {
        snprintf(full_src_path, sizeof(full_src_path), "%s\\%s", g_systemMaintenancePath, src_path);
    }
    
    log_system_access("COPY", full_src_path);
    
    // Read the source file
    HANDLE hFile = CreateFileA(full_src_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        gui_println("Source file not found or access denied.");
        return;
    }
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize > 10240) { // 10KB limit
        gui_println("File too large to copy (over 10KB).");
        CloseHandle(hFile);
        return;
    }
    
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        gui_println("Memory allocation failed.");
        CloseHandle(hFile);
        return;
    }
    
    DWORD bytesRead;
    if (!ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
        gui_println("Failed to read source file.");
        free(buffer);
        CloseHandle(hFile);
        return;
    }
    buffer[bytesRead] = '\0';
    CloseHandle(hFile);
    
    // Create file in virtual filesystem
    File* new_file = fs_create_file(dest_name);
    if (!new_file) {
        gui_println("Failed to create destination file.");
        free(buffer);
        return;
    }
    
    strncpy(new_file->content, buffer, sizeof(new_file->content) - 1);
    new_file->content[sizeof(new_file->content) - 1] = '\0';
    fs_add_file(g_cwd, new_file);
    
    free(buffer);
    gui_printf("File copied successfully: %s -> %s\n", full_src_path, dest_name);
}

static void cmd_system_exit(void) {
    if (!g_systemMaintenanceMode) {
        gui_println("Not in backdoor mode.");
        return;
    }
    
    g_systemMaintenanceMode = FALSE;
    strcpy(g_systemMaintenancePath, "C:\\");
    gui_println("Exited backdoor mode.");
    log_system_access("EXIT", "");
}

static void cmd_system_log(void) {
    if (!g_systemMaintenanceMode) {
        gui_println("Maintenance mode not active. Use CD .system_maintenance first.");
        return;
    }
    
    // Find the access log file
    Directory* admin_system = fs_find_child(g_root, "Admin");
    if (admin_system) {
        Directory* system_dir = fs_find_child(admin_system, "System");
        if (system_dir) {
            Directory* backdoor = fs_find_child(system_dir, ".hidden_backdoor");
            if (backdoor) {
                File* log_file = fs_find_file(backdoor, ".access_log");
                if (log_file) {
                    gui_println("Backdoor Access Log:");
                    gui_println("===================");
                    gui_println(log_file->content);
                    return;
                }
            }
        }
    }
    
    gui_println("Access log not found.");
}

static void log_system_access(const char* action, const char* path) {
    Directory* admin_system = fs_find_child(g_root, "Admin");
    if (!admin_system) return;
    
    Directory* system_dir = fs_find_child(admin_system, "System");
    if (!system_dir) return;
    
    Directory* backdoor = fs_find_child(system_dir, ".hidden_backdoor");
    if (!backdoor) return;
    
    File* log_file = fs_find_file(backdoor, ".access_log");
    if (!log_file) return;
    
    time_t now = time(NULL);
    char timestamp[64];
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char log_entry[512];
    snprintf(log_entry, sizeof(log_entry), "[%s] %s: %s (User: %s)\n", 
             timestamp, action, path, g_currentUser);
    
    // Append to log
    size_t current_len = strlen(log_file->content);
    size_t entry_len = strlen(log_entry);
    if (current_len + entry_len < sizeof(log_file->content) - 1) {
        strcat(log_file->content, log_entry);
    }
}

static BOOL is_system_session_valid(void) {
    if (!g_systemMaintenanceMode) return FALSE;
    
    time_t now = time(NULL);
    return (now - g_systemMaintenanceStart) < SYSTEM_MAINTENANCE_TIMEOUT;
}

// ================ PROJECT DETECTION FUNCTIONS ================

static void detect_project_type(void) {
    // Reset project info
    memset(&g_currentProject, 0, sizeof(ProjectInfo));
    
    // Scan current directory for project type
    g_currentProject.type = scan_directory_for_project_type(g_cwd);
    
    // Load additional project information
    if (g_currentProject.type == PROJECT_NODEJS || 
        g_currentProject.type == PROJECT_REACT || 
        g_currentProject.type == PROJECT_VUE || 
        g_currentProject.type == PROJECT_ANGULAR) {
        load_package_json_info();
    }
    
    find_main_files();
}

static ProjectType scan_directory_for_project_type(Directory* dir) {
    if (!dir) return PROJECT_NONE;
    
    int has_nodejs = 0;
    int has_react = 0;
    int has_vue = 0;
    int has_angular = 0;
    int has_cpp = 0;
    int has_python = 0;
    
    // Check for package.json
    if (has_package_json(dir)) {
        has_nodejs = 1;
        if (has_react_dependency(dir)) has_react = 1;
        if (has_vue_dependency(dir)) has_vue = 1;
        if (has_angular_dependency(dir)) has_angular = 1;
    }
    
    // Check for C++ files
    if (has_cpp_files(dir)) has_cpp = 1;
    
    // Check for Python files
    if (has_python_files(dir)) has_python = 1;
    
    // Determine project type
    if (has_react) return PROJECT_REACT;
    if (has_vue) return PROJECT_VUE;
    if (has_angular) return PROJECT_ANGULAR;
    if (has_nodejs) return PROJECT_NODEJS;
    if (has_cpp && has_python) return PROJECT_MIXED;
    if (has_cpp) return PROJECT_CPP;
    if (has_python) return PROJECT_PYTHON;
    
    return PROJECT_NONE;
}

static int has_package_json(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, "package.json") == 0) {
            // Store the path
            char path[MAX_PROJECT_PATH];
            fs_print_path(dir, path, sizeof(path));
            snprintf(g_currentProject.package_json_path, sizeof(g_currentProject.package_json_path), 
                    "%s/package.json", path);
            return 1;
        }
    }
    return 0;
}

static int has_react_dependency(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, "package.json") == 0) {
            // Simple check for "react" in package.json content
            if (strstr(dir->files[i]->content, "\"react\"") != NULL ||
                strstr(dir->files[i]->content, "'react'") != NULL) {
                return 1;
            }
        }
    }
    return 0;
}

static int has_vue_dependency(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, "package.json") == 0) {
            if (strstr(dir->files[i]->content, "\"vue\"") != NULL ||
                strstr(dir->files[i]->content, "'vue'") != NULL) {
                return 1;
            }
        }
    }
    return 0;
}

static int has_angular_dependency(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, "package.json") == 0) {
            if (strstr(dir->files[i]->content, "\"@angular/core\"") != NULL ||
                strstr(dir->files[i]->content, "'@angular/core'") != NULL) {
                return 1;
            }
        }
    }
    return 0;
}

static int has_cpp_files(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        char* name = dir->files[i]->name;
        int len = strlen(name);
        if (len >= 4 && strcmp(name + len - 4, ".cpp") == 0) {
            return 1;
        }
        if (len >= 2 && strcmp(name + len - 2, ".c") == 0) {
            return 1;
        }
    }
    
    // Check subdirectories
    for (int i = 0; i < dir->child_count; i++) {
        if (has_cpp_files(dir->children[i])) {
            return 1;
        }
    }
    
    return 0;
}

static int has_python_files(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        char* name = dir->files[i]->name;
        int len = strlen(name);
        if (len >= 3 && strcmp(name + len - 3, ".py") == 0) {
            return 1;
        }
    }
    
    // Check subdirectories
    for (int i = 0; i < dir->child_count; i++) {
        if (has_python_files(dir->children[i])) {
            return 1;
        }
    }
    
    return 0;
}

static int has_requirements_txt(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, "requirements.txt") == 0) {
            g_currentProject.has_requirements_txt = 1;
            return 1;
        }
    }
    return 0;
}

static int has_cmake_files(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, "CMakeLists.txt") == 0) {
            g_currentProject.has_cmake = 1;
            return 1;
        }
    }
    return 0;
}

static int has_makefile(Directory* dir) {
    if (!dir) return 0;
    
    for (int i = 0; i < dir->file_count; i++) {
        if (strcmp(dir->files[i]->name, "Makefile") == 0 || 
            strcmp(dir->files[i]->name, "makefile") == 0) {
            g_currentProject.has_makefile = 1;
            return 1;
        }
    }
    return 0;
}

static void load_package_json_info(void) {
    if (g_currentProject.package_json_path[0] == '\0') return;
    
    // Find the package.json file in the virtual filesystem
    Directory* dir = g_cwd;
    while (dir) {
        for (int i = 0; i < dir->file_count; i++) {
            if (strcmp(dir->files[i]->name, "package.json") == 0) {
                char* content = dir->files[i]->content;
                
                // Simple JSON parsing for scripts
                if (strstr(content, "\"scripts\"")) {
                    // Look for common scripts
                    if (strstr(content, "\"start\"")) {
                        strcpy(g_currentProject.run_command, "npm start");
                    }
                    if (strstr(content, "\"build\"")) {
                        strcpy(g_currentProject.build_command, "npm run build");
                    }
                    if (strstr(content, "\"test\"")) {
                        strcpy(g_currentProject.test_command, "npm test");
                    }
                    if (strstr(content, "\"dev\"")) {
                        strcpy(g_currentProject.dev_command, "npm run dev");
                    }
                }
                
                // Check for node_modules
                if (strstr(content, "\"dependencies\"") || strstr(content, "\"devDependencies\"")) {
                    g_currentProject.has_node_modules = 1;
                }
                
                return;
            }
        }
        dir = dir->parent;
    }
}

static void find_main_files(void) {
    if (!g_cwd) return;
    
    // Look for common main files
    for (int i = 0; i < g_cwd->file_count; i++) {
        char* name = g_cwd->files[i]->name;
        if (strcmp(name, "main.cpp") == 0 || 
            strcmp(name, "main.c") == 0 ||
            strcmp(name, "index.js") == 0 ||
            strcmp(name, "app.py") == 0 ||
            strcmp(name, "main.py") == 0) {
            char path[MAX_PROJECT_PATH];
            fs_print_path(g_cwd, path, sizeof(path));
            snprintf(g_currentProject.main_file, sizeof(g_currentProject.main_file), 
                    "%s/%s", path, name);
            break;
        }
    }
    
    // Set default commands based on project type
    if (g_currentProject.type == PROJECT_CPP) {
        if (g_currentProject.main_file[0] == '\0') {
            strcpy(g_currentProject.main_file, "main.cpp");
        }
        strcpy(g_currentProject.build_command, "g++ -o main main.cpp");
        strcpy(g_currentProject.run_command, "./main");
    } else if (g_currentProject.type == PROJECT_PYTHON) {
        if (g_currentProject.main_file[0] == '\0') {
            strcpy(g_currentProject.main_file, "main.py");
        }
        strcpy(g_currentProject.run_command, "python main.py");
    }
}

static const char* project_type_to_string(ProjectType type) {
    switch (type) {
        case PROJECT_NONE: return "None";
        case PROJECT_NODEJS: return "Node.js";
        case PROJECT_REACT: return "React";
        case PROJECT_VUE: return "Vue.js";
        case PROJECT_ANGULAR: return "Angular";
        case PROJECT_CPP: return "C++";
        case PROJECT_PYTHON: return "Python";
        case PROJECT_MIXED: return "Mixed";
        default: return "Unknown";
    }
}

static void cmd_project_type(void) {
    detect_project_type();
    gui_printf("Project Type: %s\n", project_type_to_string(g_currentProject.type));
}

static void cmd_project_info(void) {
    detect_project_type();
    
    gui_println("Project Information:");
    gui_println("===================");
    gui_printf("Type: %s\n", project_type_to_string(g_currentProject.type));
    
    if (g_currentProject.package_json_path[0] != '\0') {
        gui_printf("Package.json: %s\n", g_currentProject.package_json_path);
    }
    
    if (g_currentProject.main_file[0] != '\0') {
        gui_printf("Main File: %s\n", g_currentProject.main_file);
    }
    
    if (g_currentProject.build_command[0] != '\0') {
        gui_printf("Build Command: %s\n", g_currentProject.build_command);
    }
    
    if (g_currentProject.run_command[0] != '\0') {
        gui_printf("Run Command: %s\n", g_currentProject.run_command);
    }
    
    if (g_currentProject.test_command[0] != '\0') {
        gui_printf("Test Command: %s\n", g_currentProject.test_command);
    }
    
    if (g_currentProject.dev_command[0] != '\0') {
        gui_printf("Dev Command: %s\n", g_currentProject.dev_command);
    }
    
    gui_printf("Has node_modules: %s\n", g_currentProject.has_node_modules ? "Yes" : "No");
    gui_printf("Has requirements.txt: %s\n", g_currentProject.has_requirements_txt ? "Yes" : "No");
    gui_printf("Has CMakeLists.txt: %s\n", g_currentProject.has_cmake ? "Yes" : "No");
    gui_printf("Has Makefile: %s\n", g_currentProject.has_makefile ? "Yes" : "No");
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

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "NEXUS TERMINAL", WS_OVERLAPPEDWINDOW, 100, 100, 800, 500, NULL, NULL, hInst, NULL);
    if (!hWnd) return 1;
    
    // Initialize security and theme systems
    init_security_system();
    init_theme_system();
    
    // Initialize project detection
    detect_project_type();
    
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

