/*
 * Kane & Lynch: Dead Men - menu scale for high resolutions (EXPERIMENTAL), applied in
 * memory by the dinput8.dll proxy. Include after kl_hud.h, in one file only.
 *
 * Menus are a scene graph of windows laid out in backbuffer pixels (top-left origin)
 * and drawn by five GUI cameras (background, menu, dialog, TRC, mouse).
 *
 *  1. layout: the menu framework's reads of the screen size are redirected to a
 *     virtual screen of width / s by height / s, so menus are laid out as at 1080p.
 *  2. projection: after the game builds a GUI camera's projection, it is stretched
 *     so that virtual screen covers the whole backbuffer. The GUI cameras are
 *     recognised by their entities, captured when the menu system creates them.
 *
 * Setting, in KL_StutterFix.ini:
 *     [Display]
 *     MenuScale=auto       ; auto = screen height / 1080, 1 = off
 */
#ifndef KL_MENU_H
#define KL_MENU_H

#pragma comment(lib, "user32.lib")

/* ---- Projection builder (all cameras): thiscall, 5 args, ret 0x14 ------- */
static const short kl_proj_pat[] = {
    0x83, 0xEC, 0x0C,                                   /* sub esp, 0xc       <- hook */
    0x8B, 0x44, 0x24, 0x18,                             /* mov eax, [esp+0x18]       */
    0x56, 0x8B, 0xF1, 0x8B, 0x48, 0x08, 0x2B, 0x08, 0x57, 0x85, 0xC9
};
#define KL_PROJ_STOLEN 7
#define KL_SNAP_ENTITY 0x14c  /* camera snapshot: camera entity pointer            */

/* ---- GUI camera setup: stores the five GUI camera entities ----------------- */
static const short kl_guicam_pat[] = {
    0x83, 0xEC, 0x18,                                   /* sub esp, 0x18      <- hook */
    0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD,           /* mov eax, [render]         */
    0x53, 0x55, 0x56, 0x8B, 0xF1, 0x8B, 0x88, 0xA8, 0x00, 0x00, 0x00
};
#define KL_GUICAM_STOLEN 8
static const int kl_guicam_slots[] = { 0x4c4, 0x4c8, 0x4cc, 0x4d0, 0x4d8 };
#define KL_GUICAM_COUNT 5

/* ---- Menu framework reads of the screen size (operand = &render pointer) --- */
typedef struct {
    int operand;
    short pat[16];
} KlSizeRead;

static const KlSizeRead kl_size_reads[] = {
    /* element centring, parentless elements */
    { 1, { 0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0x8B, 0x98, 0x80, 0x00, 0x00, 0x00, 0x8B, 0x80, 0x84, 0x00, 0x00 } },
    /* window vertical centring */
    { 2, { 0x8B, 0x15, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0x83, 0xEC, 0x0C, 0x0D, 0x00, 0x0C, 0x00, 0x00, 0x89, 0x44 } },
    /* cursor clamp */
    { 1, { 0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0xDB, 0x80, 0x80, 0x00, 0x00, 0x00, 0x53, 0x56, 0x57, 0xD9, 0x5C } },
    /* window width compare */
    { 2, { 0x8B, 0x0D, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0xDB, 0x81, 0x80, 0x00, 0x00, 0x00, 0xD9, 0x5C, 0x24, 0x10 } },
    /* normalised placement */
    { 1, { 0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0xDB, 0x80, 0x84, 0x00, 0x00, 0x00, 0x8B, 0x16, 0x8B, 0xCE, 0xD8 } },
    { 1, { 0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0xDA, 0xB0, 0x80, 0x00, 0x00, 0x00, 0xD9, 0x5C, 0x24, 0x28, 0xD9 } },
    /* ScaleToViewPort root windows */
    { 1, { 0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0xD9, 0x54, 0x24, 0x18, 0xD9, 0x54, 0x24, 0x1C, 0xDB, 0x80, 0x80 } },
    { 1, { 0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD, 0xDB, 0x80, 0x80, 0x00, 0x00, 0x00, 0xD9, 0x5C, 0x24, 0x0C, 0xDB } },
};
#define KL_SIZE_READ_COUNT (int)(sizeof(kl_size_reads) / sizeof(kl_size_reads[0]))

/* Stand-in for the render object: only the screen size fields are read. */
static struct {
    unsigned char unused[0x80];
    int width, height;
} kl_virt;
static void *kl_virt_ptr = &kl_virt;

static float kl_menu_user;   /* 0 = auto */
static void *kl_gui_cams[KL_GUICAM_COUNT * 2];
static unsigned char *kl_proj_tramp, *kl_guicam_tramp;

static float kl_menu_value(int height) {
    float s = kl_menu_user > 0.0f ? kl_menu_user : (float)height / KL_HUD_REF_HEIGHT;
    return s < 1.0f ? 1.0f : s;
}

static void kl_menu_update(int width, int height) {
    float s = kl_menu_value(height);
    kl_virt.width = (int)(width / s + 0.5f);
    kl_virt.height = (int)(height / s + 0.5f);
}

static void __cdecl kl_guicam_capture(unsigned char *menu) {
    for (int i = 0; i < KL_GUICAM_COUNT; i++) {
        void *ent = *(void **)(menu + kl_guicam_slots[i]);
        kl_gui_cams[i * 2] = ent;
        kl_gui_cams[i * 2 + 1] = ent ? *(void **)((unsigned char *)ent + 4) : NULL;
    }
}

/* After the game built projection m (row vectors) for a camera: GUI cameras get
 * x' = s*x + (s-1)*w, y' = s*y - (s-1)*w, which stretches the top-left
 * width/s x height/s of the screen over the whole screen. */
static void __cdecl kl_proj_post(float *m, unsigned char *snap) {
    void *ent = *(void **)(snap + KL_SNAP_ENTITY);
    int gui = 0;
    for (int i = 0; ent && i < KL_GUICAM_COUNT * 2; i++) gui |= kl_gui_cams[i] == ent;

    int width = *(int *)(*kl_render + 0x80);
    int height = *(int *)(*kl_render + 0x84);
    kl_menu_update(width, height);
    float s = gui ? kl_menu_value(height) : 1.0f;

    if (s == 1.0f) return;
    for (int i = 0; i < 4; i++) {
        float w = m[4 * i + 3];
        m[4 * i + 0] = s * m[4 * i + 0] + (s - 1.0f) * w;
        m[4 * i + 1] = s * m[4 * i + 1] - (s - 1.0f) * w;
    }
}

/* Replaces the projection builder's entry: runs the original, then kl_proj_post. */
static __declspec(naked) void kl_proj_hook(void) {
    __asm {
        push dword ptr [esp + 0x14]
        push dword ptr [esp + 0x14]
        push dword ptr [esp + 0x14]
        push dword ptr [esp + 0x14]
        push dword ptr [esp + 0x14]
        call dword ptr [kl_proj_tramp]
        push eax
        push dword ptr [esp + 0xc]    ; snapshot
        push dword ptr [esp + 0xc]    ; matrix
        call kl_proj_post
        add  esp, 8
        pop  eax
        ret  0x14
    }
}

static __declspec(naked) void kl_guicam_hook(void) {
    __asm {
        push ecx
        call dword ptr [kl_guicam_tramp]
        pop  ecx
        push eax
        push ecx
        call kl_guicam_capture
        add  esp, 4
        pop  eax
        ret
    }
}

/* Executable copy of the first bytes of code at, followed by a jmp back. */
static unsigned char *kl_trampoline(unsigned char *at, int len) {
    unsigned char *t = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return NULL;
    memcpy(t, at, len);
    t[len] = 0xE9;
    *(INT32 *)(t + len + 1) = (INT32)((at + len) - (t + len + 5));
    return t;
}

/* Initial virtual size from the game's settings, before the first frame. */
static void kl_menu_initial_size(void) {
    wchar_t path[MAX_PATH], line[128];
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (n && n < MAX_PATH - 32) {
        wcscat(path, L"\\kaneandlynch\\main.ini");
        FILE *f = _wfopen(path, L"r");
        if (f) {
            while (fgetws(line, 128, f)) {
                int rw, rh;
                if (swscanf(line, L"Resolution %dx%d", &rw, &rh) == 2 && rw > 0 && rh > 0) {
                    w = rw;
                    h = rh;
                }
            }
            fclose(f);
        }
    }
    kl_menu_update(w, h);
}

/* Needs kl_hud_apply to have run (render pointer). */
static void kl_menu_apply(unsigned char *code, size_t code_size, const wchar_t *ini,
                          void (*log)(const char *)) {
    char msg[160];
    size_t proj = 0, guicam = 0;
    kl_menu_user = kl_read_scale(ini, L"MenuScale");
    if (!kl_render) {
        log("menu       skipped (needs hud-scale)");
        return;
    }
    if (kl_menu_user > 0.0f && kl_menu_user <= 1.0f) {
        log("menu       off (MenuScale=1)");
        return;
    }
    if (kl_find(code, code_size, kl_proj_pat, KL_PAT_LEN(kl_proj_pat), &proj) != 1 ||
        kl_find(code, code_size, kl_guicam_pat, KL_PAT_LEN(kl_guicam_pat), &guicam) != 1) {
        log("menu       code not found (unsupported version), skipped");
        return;
    }
    size_t at[KL_SIZE_READ_COUNT];
    for (int i = 0; i < KL_SIZE_READ_COUNT; i++) {
        if (kl_find(code, code_size, kl_size_reads[i].pat, 16, &at[i]) != 1) {
            log("menu       code not found (unsupported version), skipped");
            return;
        }
    }

    kl_menu_initial_size();
    kl_proj_tramp = kl_trampoline(code + proj, KL_PROJ_STOLEN);
    kl_guicam_tramp = kl_trampoline(code + guicam, KL_GUICAM_STOLEN);
    BOOL ok = kl_proj_tramp && kl_guicam_tramp &&
              kl_write_jmp(code + proj, (ULONG_PTR)kl_proj_hook, KL_PROJ_STOLEN) &&
              kl_write_jmp(code + guicam, (ULONG_PTR)kl_guicam_hook, KL_GUICAM_STOLEN);
    for (int i = 0; ok && i < KL_SIZE_READ_COUNT; i++) {
        unsigned char *op = code + at[i] + kl_size_reads[i].operand;
        DWORD old;
        ok = VirtualProtect(op, 4, PAGE_EXECUTE_READWRITE, &old);
        if (ok) {
            *(void ***)op = &kl_virt_ptr;
            VirtualProtect(op, 4, old, &old);
            FlushInstructionCache(GetCurrentProcess(), op, 4);
        }
    }
    _snprintf(msg, sizeof(msg), "menu       %s (scale %s, virtual screen %dx%d)",
              ok ? "applied" : "failed", kl_menu_user > 0.0f ? "from ini" : "auto",
              kl_virt.width, kl_virt.height);
    msg[sizeof(msg) - 1] = 0;
    log(msg);
}

#endif
