#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NAME 256
#define MAX_CHILDREN 100
#define MAX_FILES 100
#define MAX_FILE_SIZE 2048

#define ID_OUTPUT 1001
#define ID_INPUT  1002

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
    // Build path backwards
    char tmp[1024] = {0};
    Directory* cur = dir;
    while (cur && cur != g_root) {
        char seg[300];
        snprintf(seg, sizeof(seg), "%s%s", cur == dir ? cur->name : cur->name, "");
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
    g_cwd = g_root;
    // Seed with a few dirs
    Directory* users = fs_create_dir("Users");
    Directory* windows = fs_create_dir("Windows");
    Directory* temp = fs_create_dir("Temp");
    fs_add_child(g_root, users);
    fs_add_child(g_root, windows);
    fs_add_child(g_root, temp);
    Directory* admin = fs_create_dir("Administrator");
    fs_add_child(users, admin);
}

// ---------------- GUI helpers ----------------
static HWND g_hOut = NULL;
static HWND g_hIn = NULL;
static HFONT g_hMono = NULL;
static HBRUSH g_hbrBlack = NULL;

static void gui_append(const char* text) {
    int len = GetWindowTextLengthA(g_hOut);
    SendMessageA(g_hOut, EM_SETREADONLY, (WPARAM)FALSE, 0);
    SendMessageA(g_hOut, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(g_hOut, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(g_hOut, EM_SETREADONLY, (WPARAM)TRUE, 0);
}

static void gui_println(const char* text) {
    gui_append(text);
    gui_append("\r\n");
}

static void gui_print_prompt(void) {
    char path[1024];
    fs_print_path(g_cwd, path, sizeof(path));
    char line[1100];
    snprintf(line, sizeof(line), "%s> ", path);
    gui_append(line);
}

static void gui_clear(void) {
    SendMessageA(g_hOut, EM_SETREADONLY, (WPARAM)FALSE, 0);
    SetWindowTextA(g_hOut, "");
    SendMessageA(g_hOut, EM_SETREADONLY, (WPARAM)TRUE, 0);
}

// ---------------- Command processing ----------------
static void cmd_help(void) {
    gui_println("Commands:");
    gui_println("  DIR, LS               List directory contents");
    gui_println("  CD <dir>|..|~         Change directory");
    gui_println("  MKDIR <name>          Create directory");
    gui_println("  TOUCH <name>          Create empty file");
    gui_println("  TYPE <file>           Show file contents");
    gui_println("  ECHO <text>           Print text");
    gui_println("  PWD                   Print working directory");
    gui_println("  CLS, CLEAR            Clear screen");
    gui_println("  EXIT                  Quit");
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
    if (!name || !*name) { gui_println("The syntax of the command is incorrect."); return; }
    if (fs_find_child(g_cwd, name)) { gui_println("A subdirectory or file already exists."); return; }
    Directory* d = fs_create_dir(name);
    if (d) fs_add_child(g_cwd, d);
}

static void cmd_touch(const char* name) {
    if (!name || !*name) { gui_println("The syntax of the command is incorrect."); return; }
    if (fs_find_file(g_cwd, name)) { gui_println("File already exists."); return; }
    File* f = fs_create_file(name);
    if (f) fs_add_file(g_cwd, f);
}

static void cmd_type(const char* name) {
    if (!name || !*name) { gui_println("The system cannot find the file specified."); return; }
    File* f = fs_find_file(g_cwd, name);
    if (!f) { gui_println("The system cannot find the file specified."); return; }
    gui_println(f->content);
}

static void cmd_cd(const char* path) {
    if (!path || !*path || strcmp(path, "~") == 0) { g_cwd = g_root; return; }
    if (strcmp(path, "..") == 0) { if (g_cwd != g_root) g_cwd = g_cwd->parent; return; }
    Directory* d = fs_find_child(g_cwd, path);
    if (d) { g_cwd = d; } else { gui_println("The system cannot find the path specified."); }
}

static void cmd_echo(const char* text) { gui_println(text ? text : ""); }

static int parse_first_token(char* line, char** arg_out) {
    // trim leading spaces
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
    if (!parse_first_token(input, &arg)) return TRUE; // empty line
    if (strcmp(input, "HELP") == 0) { cmd_help(); }
    else if (strcmp(input, "PWD") == 0) { cmd_pwd(); }
    else if (strcmp(input, "DIR") == 0 || strcmp(input, "LS") == 0) { cmd_dir(); }
    else if (strcmp(input, "MKDIR") == 0 || strcmp(input, "MD") == 0) { cmd_mkdir(arg); }
    else if (strcmp(input, "TOUCH") == 0) { cmd_touch(arg); }
    else if (strcmp(input, "TYPE") == 0 || strcmp(input, "CAT") == 0) { cmd_type(arg); }
    else if (strcmp(input, "CD") == 0) { cmd_cd(arg); }
    else if (strcmp(input, "ECHO") == 0) { cmd_echo(arg); }
    else if (strcmp(input, "CLS") == 0 || strcmp(input, "CLEAR") == 0) { gui_clear(); }
    else if (strcmp(input, "EXIT") == 0 || strcmp(input, "QUIT") == 0) { return FALSE; }
    else { gui_println("'COMMAND' is not recognized."); }
    return TRUE;
}

// Subclass input edit to capture Enter
static WNDPROC g_InputPrevProc = NULL;
static LRESULT CALLBACK InputEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
    }
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        char buf[1024];
        GetWindowTextA(hWnd, buf, (int)sizeof(buf));
        gui_append(buf);
        gui_append("\r\n");
        if (!process_command(buf)) {
            PostMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
            return 0;
        }
        SetWindowTextA(hWnd, "");
        gui_print_prompt();
        return 0; // eat Enter
    }
    return CallWindowProcA(g_InputPrevProc, hWnd, msg, wParam, lParam);
}

static void create_child_controls(HWND hWnd) {
    DWORD outStyle = WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_READONLY | WS_BORDER | WS_TABSTOP;
    DWORD inStyle  = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP;
    g_hOut = CreateWindowExA(0, "EDIT", "", outStyle, 8, 8, 800, 500, hWnd, (HMENU)ID_OUTPUT, GetModuleHandle(NULL), NULL);
    g_hIn  = CreateWindowExA(0, "EDIT", "", inStyle, 8, 516, 800, 28, hWnd, (HMENU)ID_INPUT, GetModuleHandle(NULL), NULL);

    // Set monospaced font
    g_hMono = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, "Consolas");
    SendMessageA(g_hOut, WM_SETFONT, (WPARAM)g_hMono, TRUE);
    SendMessageA(g_hIn, WM_SETFONT, (WPARAM)g_hMono, TRUE);

    // Subclass input for Enter key
    g_InputPrevProc = (WNDPROC)SetWindowLongPtrA(g_hIn, GWLP_WNDPROC, (LONG_PTR)InputEditProc);
    // Focus input and set a reasonable text limit
    SetFocus(g_hIn);
    SendMessageA(g_hIn, EM_SETLIMITTEXT, (WPARAM)1000, 0);
}

static void layout_children(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    int pad = 8;
    int inputH = 28;
    MoveWindow(g_hOut, pad, pad, rc.right - 2*pad, rc.bottom - 3*pad - inputH, TRUE);
    MoveWindow(g_hIn,  pad, rc.bottom - pad - inputH, rc.right - 2*pad, inputH, TRUE);
}

// ---------------- Win32 window ----------------
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(255,255,255));
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
            gui_println("C File System GUI Terminal");
            gui_println("Type 'help' for commands, 'exit' to quit\r\n");
            gui_print_prompt();
            SetFocus(g_hIn);
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE && g_hIn) SetFocus(g_hIn);
            return 0;
        case WM_SIZE:
            layout_children(hWnd);
            return 0;
        case WM_DESTROY:
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
    // Use black background for the window class
    if (!g_hbrBlack) g_hbrBlack = CreateSolidBrush(RGB(0,0,0));
    wc.hbrBackground = g_hbrBlack;
    wc.lpszClassName = "FsGuiTermClass";
    if (!RegisterClassA(&wc)) return 1;

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "C File System Terminal", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 640, NULL, NULL, hInst, NULL);
    if (!hWnd) return 1;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}


