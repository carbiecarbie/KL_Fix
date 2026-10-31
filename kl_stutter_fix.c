/*
 * Kane & Lynch: Dead Men - Stutter Fix installer.
 *
 * Installs a dinput8.dll proxy into the game folder that applies the patches
 * from kl_patches.h in memory at startup. kaneandlynch.exe itself is not
 * modified: Games for Windows - LIVE (Steam / retail) validates the exe's
 * signature and refuses to start a modified one.
 *
 * An existing third-party dinput8.dll (e.g. DirectInputFpsFix) is renamed to
 * dinput8_next.dll and chain-loaded by the proxy. Version 1.0 patched the exe on
 * disk; installing or uninstalling restores it from that version's backup.
 *
 * The proxy also scales the HUD, subtitles and menus for high resolutions; its
 * settings file KL_StutterFix.ini is created with defaults on install (an existing
 * one is kept) and removed on uninstall.
 *
 * Usage: KL_StutterFix.exe [--install | --uninstall] [game folder or kaneandlynch.exe]
 */
#define _CRT_SECURE_NO_WARNINGS  /* buffers are explicitly bounded and terminated */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kl_patches.h"

#define VERSION "2.1"
#define EXE_NAME L"kaneandlynch.exe"
#define DLL_NAME L"dinput8.dll"
#define CHAIN_NAME L"dinput8_next.dll"
#define LOG_NAME L"KL_StutterFix.log"
#define INI_NAME L"KL_StutterFix.ini"
#define BACKUP_SUFFIX L".stutterfix-backup"
#define LEGACY_BACKUP_SUFFIX L".orig"
#define DLL_MARKER L"KL_StutterFix_dinput8"
#define IDR_PROXY_DLL 101
#define PATH_CAP (MAX_PATH + 32)

/* Prints a label followed by a wide path, converted to UTF-8 for the console. */
static void print_path(const char *label, const wchar_t *path) {
    char utf8[MAX_PATH * 4];
    if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), NULL, NULL)) utf8[0] = 0;
    printf("%s %s\n", label, utf8);
}

/* Reads one line of input; uses ReadConsoleW so non-ASCII paths survive. */
static BOOL read_line(wchar_t *buf, DWORD cap) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, got = 0;
    if (GetConsoleMode(in, &mode)) {
        if (!ReadConsoleW(in, buf, cap - 1, &got, NULL)) return FALSE;
        buf[got] = 0;
        return TRUE;
    }
    return fgetws(buf, cap, stdin) != NULL;
}

/* Reads a menu choice: the first digit on the line, or 0. */
static wchar_t read_choice(void) {
    wchar_t line[64];
    if (!read_line(line, 64)) return 0;
    for (const wchar_t *c = line; *c && *c != L'\n'; c++)
        if (*c >= L'0' && *c <= L'9') return *c;
    return 0;
}

/* ---------------------------------------------------------------------------- */

static unsigned char *read_file(const wchar_t *path, size_t *size) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER sz;
    unsigned char *buf = NULL;
    DWORD got = 0;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < 256 * 1024 * 1024) {
        buf = (unsigned char *)malloc((size_t)sz.QuadPart);
        if (buf && (!ReadFile(h, buf, (DWORD)sz.QuadPart, &got, NULL) || got != sz.QuadPart)) {
            free(buf);
            buf = NULL;
        }
    }
    CloseHandle(h);
    if (buf) *size = (size_t)sz.QuadPart;
    return buf;
}

static void print_error(DWORD err) {
    if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION)
        printf("The game seems to be running. Close it and try again.\n");
    else if (err == ERROR_ACCESS_DENIED)
        printf("Access denied. Right-click the fix and choose \"Run as administrator\".\n");
    else
        printf("Windows error %lu\n", err);
}

/* Write via a temporary file so a failure never leaves a half-written file. */
static BOOL write_file_atomic(const wchar_t *path, const void *data, size_t size) {
    wchar_t tmp[PATH_CAP];
    _snwprintf(tmp, PATH_CAP, L"%s.tmp", path);
    tmp[PATH_CAP - 1] = 0;
    HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { print_error(GetLastError()); return FALSE; }
    DWORD wrote = 0;
    BOOL ok = WriteFile(h, data, (DWORD)size, &wrote, NULL) && wrote == size && FlushFileBuffers(h);
    DWORD err = GetLastError();
    CloseHandle(h);
    if (ok && !MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        ok = FALSE;
        err = GetLastError();
    }
    if (!ok) {
        DeleteFileW(tmp);
        print_error(err);
    }
    return ok;
}

static BOOL file_exists(const wchar_t *path) {
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* Fails (and explains why) if the file is in use or not writable. */
static BOOL check_writable(const wchar_t *path) {
    if (!file_exists(path)) return TRUE;
    HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        print_error(GetLastError());
        return FALSE;
    }
    CloseHandle(h);
    return TRUE;
}

static BOOL move_file(const wchar_t *from, const wchar_t *to) {
    if (MoveFileExW(from, to, MOVEFILE_WRITE_THROUGH)) return TRUE;
    print_error(GetLastError());
    return FALSE;
}

static BOOL delete_file(const wchar_t *path) {
    if (!file_exists(path) || DeleteFileW(path)) return TRUE;
    print_error(GetLastError());
    return FALSE;
}

/* ---- Game discovery -------------------------------------------------------- */

/* Accepts a folder or a path to kaneandlynch.exe; fills exe with the exe path. */
static BOOL resolve_game(const wchar_t *input, wchar_t *exe) {
    wchar_t buf[MAX_PATH];
    size_t n;
    if (!input || !*input) return FALSE;
    wcsncpy(buf, input, MAX_PATH - 1);
    buf[MAX_PATH - 1] = 0;
    n = wcslen(buf);
    while (n && (buf[n - 1] == L'"' || buf[n - 1] == L' ' || buf[n - 1] == L'\\' ||
                 buf[n - 1] == L'\r' || buf[n - 1] == L'\n')) buf[--n] = 0;
    wchar_t *start = buf;
    while (*start == L'"' || *start == L' ') start++;
    if (file_exists(start)) {
        const wchar_t *base = wcsrchr(start, L'\\');
        base = base ? base + 1 : start;
        if (_wcsicmp(base, EXE_NAME) != 0) return FALSE;
        wcsncpy(exe, start, MAX_PATH - 1);
        exe[MAX_PATH - 1] = 0;
        return TRUE;
    }
    _snwprintf(exe, MAX_PATH, L"%s\\%s", start, EXE_NAME);
    exe[MAX_PATH - 1] = 0;
    return file_exists(exe);
}

static BOOL reg_path(const wchar_t *key, const wchar_t *value, wchar_t *out) {
    DWORD size = MAX_PATH * sizeof(wchar_t);
    return RegGetValueW(HKEY_LOCAL_MACHINE, key, value, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY,
                        NULL, out, &size) == ERROR_SUCCESS;
}

static BOOL find_game(int argc, wchar_t **argv, int first_arg, wchar_t *exe) {
    wchar_t dir[MAX_PATH];
    for (int i = first_arg; i < argc; i++)
        if (resolve_game(argv[i], exe)) return TRUE;

    /* Next to the installer, then the current folder. */
    if (GetModuleFileNameW(NULL, dir, MAX_PATH)) {
        wchar_t *slash = wcsrchr(dir, L'\\');
        if (slash) *slash = 0;
        if (resolve_game(dir, exe)) return TRUE;
    }
    if (GetCurrentDirectoryW(MAX_PATH, dir) && resolve_game(dir, exe)) return TRUE;

    /* Installers: retail/GOG/Steam register an Eidos key, GOG also its own key. */
    if (reg_path(L"SOFTWARE\\Eidos\\Kane and Lynch Dead Men", L"InstallationPath", dir) &&
        resolve_game(dir, exe)) return TRUE;
    if (reg_path(L"SOFTWARE\\GOG.com\\Games\\1721096025", L"path", dir) &&
        resolve_game(dir, exe)) return TRUE;
    if (reg_path(L"SOFTWARE\\Valve\\Steam", L"InstallPath", dir)) {
        wchar_t steam[MAX_PATH];
        _snwprintf(steam, MAX_PATH, L"%s\\steamapps\\common\\Kane and Lynch Dead Men", dir);
        steam[MAX_PATH - 1] = 0;
        if (resolve_game(steam, exe)) return TRUE;
    }
    return FALSE;
}

static BOOL ask_game_folder(wchar_t *exe) {
    wchar_t line[MAX_PATH];
    for (;;) {
        printf("\nGame not found. Paste the game folder path (or drag kaneandlynch.exe here) and press Enter.\nLeave empty to quit.\n> ");
        if (!read_line(line, MAX_PATH)) return FALSE;
        if (line[0] == L'\r' || line[0] == L'\n' || line[0] == 0) return FALSE;
        if (resolve_game(line, exe)) return TRUE;
        printf("kaneandlynch.exe was not found there.\n");
    }
}

/* ---- Game folder state ----------------------------------------------------- */

typedef struct {
    wchar_t exe[MAX_PATH];
    wchar_t dll[PATH_CAP];
    wchar_t chain[PATH_CAP];
    wchar_t log[PATH_CAP];
    wchar_t ini[PATH_CAP];
    wchar_t backup[PATH_CAP];   /* original exe saved by version 1.0, if any */
    BOOL exe_supported;         /* every patch site found, original or patched */
    BOOL exe_patched;           /* modified on disk by version 1.0 */
    BOOL dll_ours;
    BOOL dll_foreign;           /* another dinput8.dll wrapper is present */
    BOOL chain_present;
} GameState;

static void sibling(const wchar_t *exe, const wchar_t *name, wchar_t *out) {
    wcsncpy(out, exe, PATH_CAP - 1);
    out[PATH_CAP - 1] = 0;
    wchar_t *slash = wcsrchr(out, L'\\');
    if (slash) slash[1] = 0;
    wcsncat(out, name, PATH_CAP - wcslen(out) - 1);
}

/* Counts patch sites in a buffer: returns FALSE if any is missing/ambiguous. */
static BOOL scan_exe(const unsigned char *data, size_t size, int *original, int *patched) {
    *original = *patched = 0;
    for (int i = 0; i < KL_PATCH_COUNT; i++) {
        size_t at = 0;
        int s = kl_state(data, size, &KL_PATCHES[i], &at);
        if (s == KL_ORIGINAL) (*original)++;
        else if (s == KL_PATCHED) (*patched)++;
        else return FALSE;
    }
    return TRUE;
}

static BOOL file_has_marker(const wchar_t *path) {
    size_t size = 0;
    unsigned char *data = read_file(path, &size);
    if (!data) return FALSE;
    const size_t mlen = wcslen(DLL_MARKER) * sizeof(wchar_t);
    BOOL found = FALSE;
    for (size_t i = 0; i + mlen <= size && !found; i++)
        found = memcmp(data + i, DLL_MARKER, mlen) == 0;
    free(data);
    return found;
}

/* A version 1.0 backup is only usable if it holds the fully original code. */
static BOOL find_v1_backup(const wchar_t *exe, wchar_t *out) {
    const wchar_t *suffixes[] = { BACKUP_SUFFIX, LEGACY_BACKUP_SUFFIX };
    for (int i = 0; i < 2; i++) {
        _snwprintf(out, PATH_CAP, L"%s%s", exe, suffixes[i]);
        out[PATH_CAP - 1] = 0;
        size_t size = 0;
        unsigned char *data = read_file(out, &size);
        if (!data) continue;
        int original, patched;
        BOOL ok = scan_exe(data, size, &original, &patched) && original == KL_PATCH_COUNT;
        free(data);
        if (ok) return TRUE;
    }
    out[0] = 0;
    return FALSE;
}

static BOOL load_state(const wchar_t *exe, GameState *st) {
    size_t size = 0;
    int original, patched;
    memset(st, 0, sizeof(*st));
    wcsncpy(st->exe, exe, MAX_PATH - 1);
    sibling(exe, DLL_NAME, st->dll);
    sibling(exe, CHAIN_NAME, st->chain);
    sibling(exe, LOG_NAME, st->log);
    sibling(exe, INI_NAME, st->ini);

    unsigned char *data = read_file(exe, &size);
    if (!data) return FALSE;
    st->exe_supported = scan_exe(data, size, &original, &patched);
    st->exe_patched = st->exe_supported && patched > 0;
    free(data);

    find_v1_backup(exe, st->backup);
    if (file_exists(st->dll)) {
        st->dll_ours = file_has_marker(st->dll);
        st->dll_foreign = !st->dll_ours;
    }
    st->chain_present = file_exists(st->chain);
    return TRUE;
}

static void print_row(const char *label, const char *value) {
    printf("  %-18s %s\n", label, value);
}

static void show_state(const GameState *st) {
    print_row("Stutter fix:", st->dll_ours ? "installed" : "not installed");
    if (st->exe_patched)
        print_row("kaneandlynch.exe:", "modified by version 1.0 (will be restored)");
    if (st->dll_foreign)
        print_row("dinput8.dll:", "another mod (kept, loaded together)");
    else if (st->dll_ours && st->chain_present)
        print_row("dinput8_next.dll:", "another mod (loaded together)");
}

/* Undoes version 1.0's on-disk patch so the signed exe is original again. */
static BOOL restore_v1_exe(GameState *st) {
    if (!st->exe_patched) return TRUE;
    if (!st->backup[0]) {
        printf("Warning: kaneandlynch.exe was modified by version 1.0 and no backup was found.\n"
               "On Steam/retail use \"Verify integrity of game files\" to restore it.\n");
        return TRUE;
    }
    size_t size = 0;
    unsigned char *data = read_file(st->backup, &size);
    if (!data) {
        printf("Could not read the backup: ");
        print_error(GetLastError());
        return FALSE;
    }
    BOOL ok = write_file_atomic(st->exe, data, size);
    free(data);
    if (!ok) return FALSE;
    print_path("Original exe restored from", st->backup);
    /* Only the backup this tool created is removed. */
    size_t n = wcslen(st->backup), s = wcslen(BACKUP_SUFFIX);
    if (n > s && !_wcsicmp(st->backup + n - s, BACKUP_SUFFIX)) DeleteFileW(st->backup);
    st->exe_patched = FALSE;
    return TRUE;
}

static const char DEFAULT_INI[] =
    "; Kane & Lynch: Dead Men - Stutter Fix " VERSION " settings\r\n"
    "; Sizes of the HUD, subtitles and menus at high resolutions.\r\n"
    "; auto = screen height / 1080 (2.0 at 4K, 1.33 at 1440p: they look as at 1080p).\r\n"
    "; Or a number from 0.5 to 4. Changes apply the next time the game starts.\r\n"
    "[Display]\r\n"
    "HudScale=auto\r\n"
    "; auto = same as HudScale\r\n"
    "SubtitleScale=auto\r\n"
    "; 1 = off (menus at native pixel size)\r\n"
    "MenuScale=auto\r\n";

static BOOL do_install(GameState *st) {
    HRSRC res = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_PROXY_DLL), (LPCWSTR)RT_RCDATA);
    HGLOBAL mem = res ? LoadResource(NULL, res) : NULL;
    const void *dll = mem ? LockResource(mem) : NULL;
    DWORD dll_size = res ? SizeofResource(NULL, res) : 0;
    if (!dll || !dll_size) {
        printf("Internal error: embedded DLL missing.\n");
        return FALSE;
    }
    if (!check_writable(st->exe) || !check_writable(st->dll)) return FALSE;
    if (!restore_v1_exe(st)) return FALSE;

    if (st->dll_foreign) {
        if (st->chain_present) {
            printf("Both dinput8.dll and dinput8_next.dll already exist from other mods.\n"
                   "Remove one of them and try again.\n");
            return FALSE;
        }
        if (!move_file(st->dll, st->chain)) return FALSE;
        printf("Existing dinput8.dll kept as dinput8_next.dll (it still loads).\n");
        st->chain_present = TRUE;
    }
    if (!write_file_atomic(st->dll, dll, dll_size)) return FALSE;
    st->dll_ours = TRUE;
    st->dll_foreign = FALSE;
    if (!file_exists(st->ini) && !write_file_atomic(st->ini, DEFAULT_INI, sizeof(DEFAULT_INI) - 1))
        return FALSE;

    printf("\nDone! The stutter fix is installed.\n");
    printf("HUD, subtitle and menu sizes can be changed in KL_StutterFix.ini in the game folder.\n");
    printf("Note: connect your controllers before starting the game; controllers plugged in\nwhile the game is running are not detected.\n");
    return TRUE;
}

static BOOL do_uninstall(GameState *st) {
    if (!check_writable(st->exe) || !check_writable(st->dll)) return FALSE;
    if (st->dll_ours) {
        if (!delete_file(st->dll)) return FALSE;
        st->dll_ours = FALSE;
        if (st->chain_present) {
            if (!move_file(st->chain, st->dll)) return FALSE;
            st->chain_present = FALSE;
            printf("Previous dinput8.dll restored.\n");
        }
    }
    delete_file(st->log);
    delete_file(st->ini);
    if (!restore_v1_exe(st)) return FALSE;
    printf("\nThe stutter fix was removed.\n");
    return TRUE;
}

static void pause_if_interactive(BOOL interactive) {
    if (!interactive) return;
    printf("\nPress Enter to exit...");
    read_choice();
}

int wmain(int argc, wchar_t **argv) {
    wchar_t exe[MAX_PATH];
    int mode = 0;  /* 0 = menu, 1 = install, 2 = uninstall */
    int first_arg = 1;
    GameState st;

    SetConsoleOutputCP(CP_UTF8);

    if (argc > 1) {
        if (!_wcsicmp(argv[1], L"--install") || !_wcsicmp(argv[1], L"--apply")) mode = 1;
        else if (!_wcsicmp(argv[1], L"--uninstall") || !_wcsicmp(argv[1], L"--restore")) mode = 2;
        if (mode) first_arg = 2;
    }
    BOOL interactive = mode == 0;

    printf("Kane & Lynch: Dead Men - Stutter Fix " VERSION "\n");
    printf("Removes the hitch every 2 seconds, smooths frame timing and enlarges the\n"
           "HUD, subtitles and menus at high resolutions (4K).\n\n");

    if (!find_game(argc, argv, first_arg, exe) && !(interactive && ask_game_folder(exe))) {
        printf("Game not found.\n");
        pause_if_interactive(interactive);
        return 1;
    }
    print_path("Game:", exe);
    printf("\n");

    if (!load_state(exe, &st)) {
        printf("Could not read the game exe: ");
        print_error(GetLastError());
        pause_if_interactive(interactive);
        return 1;
    }
    show_state(&st);

    BOOL installed = st.dll_ours;
    if (!st.exe_supported && mode != 2 && !installed) {
        printf("\nThis version of kaneandlynch.exe is not supported. Nothing was changed.\n");
        pause_if_interactive(interactive);
        return 1;
    }

    if (mode == 0) {
        printf("\n  [1] %s\n", installed ? "Reinstall the fix" : "Install the fix");
        if (installed || st.exe_patched) printf("  [2] Uninstall the fix\n");
        printf("  [0] Exit\n> ");
        wchar_t c = read_choice();
        if (c == L'1') mode = 1;
        else if (c == L'2' && (installed || st.exe_patched)) mode = 2;
        else return 0;
        printf("\n");
    }

    BOOL ok = mode == 1 ? do_install(&st) : do_uninstall(&st);
    pause_if_interactive(interactive);
    return ok ? 0 : 1;
}
