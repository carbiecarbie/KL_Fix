/*
 * Kane & Lynch: Dead Men - Stutter Fix, in-memory patch (dinput8.dll proxy).
 *
 * The game imports dinput8.dll, so this DLL is loaded from the game folder at
 * startup. It applies the patches from kl_patches.h to the game's code in memory,
 * installs the HUD/subtitle/menu scale hooks (kl_hud.h, kl_menu.h) and forwards DirectInput to the real DLL. kaneandlynch.exe is never modified on
 * disk, which keeps its signature valid for Games for Windows - LIVE (Steam and
 * retail versions refuse to start with a modified exe).
 *
 * If another dinput8.dll wrapper was installed before (e.g. DirectInputFpsFix),
 * the installer renames it to dinput8_next.dll and it is chain-loaded here.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "kl_patches.h"
#include "kl_hud.h"
#include "kl_menu.h"

#define GAME_EXE L"kaneandlynch.exe"
#define CHAIN_DLL L"dinput8_next.dll"
#define LOG_FILE L"KL_StutterFix.log"
#define INI_FILE L"KL_StutterFix.ini"

static HMODULE g_real;
static wchar_t g_game_dir[MAX_PATH];

static void log_line(const char *text) {
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%s\\%s", g_game_dir, LOG_FILE);
    path[MAX_PATH - 1] = 0;
    FILE *f = _wfopen(path, L"a");
    if (!f) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  %s\n", t.wYear, t.wMonth, t.wDay, t.wHour,
            t.wMinute, t.wSecond, text);
    fclose(f);
}

static void apply_patches(void) {
    unsigned char *base = (unsigned char *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    unsigned char *code = NULL;
    size_t code_size = 0;
    char msg[160];

    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sec[i].Characteristics & IMAGE_SCN_CNT_CODE) {
            code = base + sec[i].VirtualAddress;
            code_size = sec[i].Misc.VirtualSize;
            break;
        }
    }
    if (!code) {
        log_line("no code section found, nothing patched");
        return;
    }

    for (int i = 0; i < KL_PATCH_COUNT; i++) {
        const KlPatch *p = &KL_PATCHES[i];
        size_t at = 0;
        int state = kl_state(code, code_size, p, &at);
        const char *result = "code not found (unsupported version), skipped";
        if (state == KL_ORIGINAL) {
            unsigned char *dst = code + at + p->patch_offset;
            DWORD old;
            if (VirtualProtect(dst, p->patch_len, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy(dst, p->patch_bytes, p->patch_len);
                VirtualProtect(dst, p->patch_len, old, &old);
                FlushInstructionCache(GetCurrentProcess(), dst, p->patch_len);
                result = "applied";
            } else {
                result = "VirtualProtect failed";
            }
        } else if (state == KL_PATCHED) {
            result = "already present in exe";
        } else if (state == KL_AMBIGUOUS) {
            result = "ambiguous match (unsupported version), skipped";
        }
        _snprintf(msg, sizeof(msg), "%-10s %s", p->name, result);
        msg[sizeof(msg) - 1] = 0;
        log_line(msg);
    }

    wchar_t ini[MAX_PATH];
    _snwprintf(ini, MAX_PATH, L"%s\\%s", g_game_dir, INI_FILE);
    ini[MAX_PATH - 1] = 0;
    kl_hud_apply(code, code_size, ini, log_line);
    kl_menu_apply(code, code_size, ini, log_line);
}

static BOOL is_game_process(void) {
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return FALSE;
    wchar_t *slash = wcsrchr(exe, L'\\');
    if (!slash) return FALSE;
    *slash = 0;
    wcscpy(g_game_dir, exe);
    return _wcsicmp(slash + 1, GAME_EXE) == 0;
}

/* Loads the chained wrapper if present, otherwise the system dinput8.dll. */
static HMODULE real_dll(void) {
    if (!g_real) {
        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%s\\%s", g_game_dir, CHAIN_DLL);
        path[MAX_PATH - 1] = 0;
        if (g_game_dir[0] && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
            g_real = LoadLibraryW(path);
        if (!g_real) {
            UINT n = GetSystemDirectoryW(path, MAX_PATH);
            if (n && n < MAX_PATH - 16) {
                wcscat(path, L"\\dinput8.dll");
                g_real = LoadLibraryW(path);
            }
        }
    }
    return g_real;
}

static FARPROC real_proc(const char *name) {
    HMODULE m = real_dll();
    return m ? GetProcAddress(m, name) : NULL;
}

typedef HRESULT (WINAPI *DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID *, void *);
typedef HRESULT (WINAPI *DllGetClassObject_t)(REFCLSID, REFIID, LPVOID *);
typedef HRESULT (WINAPI *NoArgs_t)(void);
typedef const void *(WINAPI *GetdfDIJoystick_t)(void);

HRESULT WINAPI Proxy_DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID iid, LPVOID *out,
                                        void *outer) {
    DirectInput8Create_t fn = (DirectInput8Create_t)real_proc("DirectInput8Create");
    return fn ? fn(inst, ver, iid, out, outer) : E_FAIL;
}

HRESULT WINAPI Proxy_DllCanUnloadNow(void) {
    NoArgs_t fn = (NoArgs_t)real_proc("DllCanUnloadNow");
    return fn ? fn() : S_FALSE;
}

HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID *out) {
    DllGetClassObject_t fn = (DllGetClassObject_t)real_proc("DllGetClassObject");
    return fn ? fn(clsid, iid, out) : E_FAIL;
}

HRESULT WINAPI Proxy_DllRegisterServer(void) {
    NoArgs_t fn = (NoArgs_t)real_proc("DllRegisterServer");
    return fn ? fn() : E_FAIL;
}

HRESULT WINAPI Proxy_DllUnregisterServer(void) {
    NoArgs_t fn = (NoArgs_t)real_proc("DllUnregisterServer");
    return fn ? fn() : E_FAIL;
}

const void *WINAPI Proxy_GetdfDIJoystick(void) {
    GetdfDIJoystick_t fn = (GetdfDIJoystick_t)real_proc("GetdfDIJoystick");
    return fn ? fn() : NULL;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (is_game_process()) apply_patches();
    }
    return TRUE;
}
