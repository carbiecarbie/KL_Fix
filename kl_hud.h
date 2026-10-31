/*
 * Kane & Lynch: Dead Men - HUD and subtitle scale for high resolutions, applied in
 * memory by the dinput8.dll proxy. Include after kl_patches.h, in one file only.
 *
 * The game draws its 2D layer in real backbuffer pixels. The on-screen display
 * (onscreendisplay2.cpp) multiplies its widget sizes and positions by one global
 * float, set every frame: 0.65 up to 800 px wide, 0.8 up to 1024 px, otherwise 1.0.
 * So at 4K the HUD keeps its 720p pixel size and looks tiny.
 *
 *  1. hud-scale: above 1024 px the scale comes from kl_hud_value() instead of 1.0.
 *
 *  2. subtitles: the subtitle box (ZSubtitleController + four text/shadow line pairs)
 *     is laid out by one OSD method that sets the word-wrap width to
 *     width - 152 - 400 * scale and calls SetScale(scale) on every line. It is
 *     replaced by kl_subtitle_layout(), which uses its own scale and divides the wrap
 *     width by it, so scaled lines still fit on screen.
 *
 * Settings, read from KL_StutterFix.ini in the game folder:
 *     [Display]
 *     HudScale=auto        ; auto = screen height / 1080, or a number (0.5 - 4)
 *     SubtitleScale=auto   ; auto = same as HudScale
 */
#ifndef KL_HUD_H
#define KL_HUD_H

#include <windows.h>
#include <stdlib.h>

/* ---- HUD scale: OSD per-frame method, after "scale = 1.0" ------------------ */
static const short kl_hud_pat[] = {
    0xD9, 0xE8,                                         /* fld1                      */
    0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD,           /* mov eax, [render]         */
    0x83, 0xEC, 0x10,                                   /* sub esp, 0x10             */
    0xD9, 0x1D, KL_WILD, KL_WILD, KL_WILD, KL_WILD,     /* fstp [osd_scale]          */
    0x8B, 0x80, 0x80, 0x00, 0x00, 0x00,                 /* mov eax, [eax+0x80] ; w   */
    0x3D, 0x20, 0x03, 0x00, 0x00,                       /* cmp eax, 800              */
    0x55, 0x56, 0x8B, 0xF1,                             /* push ebp/esi; mov esi,ecx */
    0x7F, 0x08,                                         /* jg  next                  */
    0xD9, 0x05, KL_WILD, KL_WILD, KL_WILD, KL_WILD,     /* fld [0.65]                */
    0xEB, 0x0D,                                         /* jmp store                 */
    0x3D, 0x00, 0x04, 0x00, 0x00,                       /* next: cmp eax, 1024  <- hook */
    0x7F, 0x0C,                                         /* jg  done                  */
    0xD9, 0x05, KL_WILD, KL_WILD, KL_WILD, KL_WILD,     /* fld [0.8]                 */
    0xD9, 0x1D, KL_WILD, KL_WILD, KL_WILD, KL_WILD      /* store: fstp [osd_scale]   */
};
#define KL_HUD_RENDER  3      /* offset of the render object pointer's address    */
#define KL_HUD_SITE    41     /* "cmp eax, 1024", replaced by jmp + 2 nops        */
#define KL_HUD_SMALL   50     /* address of the 0.8 constant                      */
#define KL_HUD_STORE   54     /* "fstp [osd_scale]", where the hook returns       */
#define KL_HUD_SCALE   56     /* address of the OSD scale global                  */

/* ---- Subtitle layout: OSD method, replaced entirely ------------------------ */
static const short kl_sub_pat[] = {
    0x83, 0xEC, 0x0C,                                   /* sub esp, 0xc              */
    0xA1, KL_WILD, KL_WILD, KL_WILD, KL_WILD,           /* mov eax, [render]         */
    0xD9, 0x7C, 0x24, 0x02,                             /* fnstcw [esp+2]            */
    0xDB, 0x80, 0x80, 0x00, 0x00, 0x00,                 /* fild [eax+0x80] ; width   */
    0x81, 0xC1, 0x6C, 0x08, 0x00, 0x00,                 /* add ecx, 0x86c ; lines    */
    0x0F, 0xB7, 0x44, 0x24, 0x02,                       /* movzx eax, word [esp+2]   */
    0x0D, 0x00, 0x0C, 0x00, 0x00,                       /* or eax, 0xc00             */
    0x89, 0x44, 0x24, 0x04,                             /* mov [esp+4], eax          */
    0xDC, 0x25, KL_WILD, KL_WILD, KL_WILD, KL_WILD,     /* fsub qword [152.0]        */
    0xD9, 0x05, KL_WILD, KL_WILD, KL_WILD, KL_WILD,     /* fld  [osd_scale]          */
    0xDC, 0x0D, KL_WILD, KL_WILD, KL_WILD, KL_WILD,     /* fmul qword [400.0]        */
    0xDE, 0xE9,                                         /* fsubp                     */
    0xD9, 0x6C, 0x24, 0x04,                             /* fldcw [esp+4]             */
    0xDF, 0x7C, 0x24, 0x04,                             /* fistp qword [esp+4]       */
    0x8B, 0x54, 0x24, 0x04,                             /* mov edx, [esp+4]          */
    0x89, 0x51, 0xE8,                                   /* mov [ecx-0x18], edx ; wrap */
    0xD9, 0x6C, 0x24, 0x02,                             /* fldcw [esp+2]             */
    0xE8, KL_WILD, KL_WILD, KL_WILD, KL_WILD,           /* call lines.SetScale(osd)  */
    0x83, 0xC4, 0x0C,                                   /* add esp, 0xc              */
    0xC2, 0x08, 0x00                                    /* ret 8                     */
};
#define KL_SUB_MARGIN  40     /* address of the 152.0 constant (double)           */
#define KL_SUB_GAP     52     /* address of the 400.0 constant (double)           */
#define KL_SUB_LINES   0x86c  /* subtitle line set, relative to the OSD element   */
#define KL_SUB_WRAP    0x854  /* word-wrap width in font pixels (int)             */

#define KL_SUB_LINE_COUNT  4
#define KL_SUB_LINE_FIRST  0x24  /* text control pointer of line 0, then shadow   */
#define KL_SUB_LINE_STRIDE 0x14
#define KL_CTRL_DIRTY      0x88  /* control flag byte; bit 0 = needs relayout     */
#define KL_CTRL_SETSCALE   0x21c /* vtable offset: SetScale(const float[2], int)  */

#define KL_HUD_REF_HEIGHT 1080.0f

static unsigned char **kl_render;  /* render object: width at +0x80, height +0x84 */
static const float *kl_small_scale;
static float *kl_osd_scale;
static const double *kl_sub_margin, *kl_sub_gap;
static void *kl_hud_return;
static float kl_hud_user, kl_sub_user;  /* 0 = auto */

typedef void(__fastcall *KlSetScale)(void *self, void *unused, const float *scale, int flag);

/* Scale for screens wider than 1024 px; smaller ones keep the game's values. */
static float kl_hud_value(void) {
    if (kl_hud_user > 0.0f) return kl_hud_user;
    float s = (float)*(int *)(*kl_render + 0x84) / KL_HUD_REF_HEIGHT;
    return s < 1.0f ? 1.0f : s;
}

static float __cdecl kl_hud_scale(int width) {
    return width <= 1024 ? *kl_small_scale : kl_hud_value();
}

/* Entered with eax = screen width and an empty x87 stack; returns st0 = scale to
 * the game's "fstp [osd_scale]". */
static __declspec(naked) void kl_hud_hook(void) {
    __asm {
        push ecx
        push edx
        push eax
        call kl_hud_scale
        add  esp, 4
        pop  edx
        pop  ecx
        jmp  dword ptr [kl_hud_return]
    }
}

static void __fastcall kl_subtitle_layout(unsigned char *self, void *unused, int a, int b) {
    (void)unused;
    (void)a;
    (void)b;
    int width = *(int *)(*kl_render + 0x80);
    float s = *kl_osd_scale;
    if (width > 1024) s = kl_sub_user > 0.0f ? kl_sub_user : kl_hud_value();

    double wrap = width - *kl_sub_margin - *kl_sub_gap * s;
    if (s > 1.0f) wrap /= s;
    *(int *)(self + KL_SUB_WRAP) = (int)wrap;

    float scale[2] = { s, s };
    unsigned char *lines = self + KL_SUB_LINES;
    for (int i = 0; i < KL_SUB_LINE_COUNT * 2; i++) {
        unsigned char *ctrl =
            *(unsigned char **)(lines + KL_SUB_LINE_FIRST + (i / 2) * KL_SUB_LINE_STRIDE + (i % 2) * 4);
        ctrl[KL_CTRL_DIRTY] |= 1;
        KlSetScale set_scale = *(KlSetScale *)(*(unsigned char **)ctrl + KL_CTRL_SETSCALE);
        set_scale(ctrl, NULL, scale, 0);
    }
}

/* "auto", empty or invalid -> 0 (auto); otherwise clamped to 0.5 - 4. */
static float kl_read_scale(const wchar_t *ini, const wchar_t *key) {
    wchar_t buf[32];
    GetPrivateProfileStringW(L"Display", key, L"auto", buf, 32, ini);
    wchar_t *end;
    double v = wcstod(buf, &end);
    if (end == buf || v <= 0.0) return 0.0f;
    if (v < 0.5) v = 0.5;
    if (v > 4.0) v = 4.0;
    return (float)v;
}

static BOOL kl_write_jmp(unsigned char *at, ULONG_PTR to, int len) {
    DWORD old;
    if (!VirtualProtect(at, len, PAGE_EXECUTE_READWRITE, &old)) return FALSE;
    at[0] = 0xE9;
    *(INT32 *)(at + 1) = (INT32)(to - (ULONG_PTR)(at + 5));
    for (int i = 5; i < len; i++) at[i] = 0x90;
    VirtualProtect(at, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, len);
    return TRUE;
}

#define KL_PAT_LEN(p) (int)(sizeof(p) / sizeof((p)[0]))

/* Installs both hooks; writes one result line per hook through log. */
static void kl_hud_apply(unsigned char *code, size_t code_size, const wchar_t *ini,
                         void (*log)(const char *)) {
    char msg[160];
    size_t hud = 0, sub = 0;
    int hud_n = kl_find(code, code_size, kl_hud_pat, KL_PAT_LEN(kl_hud_pat), &hud);
    int sub_n = kl_find(code, code_size, kl_sub_pat, KL_PAT_LEN(kl_sub_pat), &sub);

    kl_hud_user = kl_read_scale(ini, L"HudScale");
    kl_sub_user = kl_read_scale(ini, L"SubtitleScale");

    if (hud_n != 1) {
        log("hud-scale  code not found (unsupported version), skipped");
        log("subtitles  skipped (needs hud-scale)");
        return;
    }
    unsigned char *h = code + hud;
    kl_render = *(unsigned char ***)(h + KL_HUD_RENDER);
    kl_small_scale = *(const float **)(h + KL_HUD_SMALL);
    kl_osd_scale = *(float **)(h + KL_HUD_SCALE);
    kl_hud_return = h + KL_HUD_STORE;
    BOOL ok = kl_write_jmp(h + KL_HUD_SITE, (ULONG_PTR)kl_hud_hook, 7);
    if (kl_hud_user > 0.0f)
        _snprintf(msg, sizeof(msg), "hud-scale  %s (scale %.2f)", ok ? "applied" : "VirtualProtect failed",
                  kl_hud_user);
    else
        _snprintf(msg, sizeof(msg), "hud-scale  %s (scale auto: screen height / 1080)",
                  ok ? "applied" : "VirtualProtect failed");
    msg[sizeof(msg) - 1] = 0;
    log(msg);

    if (sub_n != 1) {
        log("subtitles  code not found (unsupported version), skipped");
        return;
    }
    unsigned char *s = code + sub;
    kl_sub_margin = *(const double **)(s + KL_SUB_MARGIN);
    kl_sub_gap = *(const double **)(s + KL_SUB_GAP);
    ok = kl_write_jmp(s, (ULONG_PTR)kl_subtitle_layout, 5);
    if (kl_sub_user > 0.0f)
        _snprintf(msg, sizeof(msg), "subtitles  %s (scale %.2f)", ok ? "applied" : "VirtualProtect failed",
                  kl_sub_user);
    else
        _snprintf(msg, sizeof(msg), "subtitles  %s (scale auto: same as HUD)",
                  ok ? "applied" : "VirtualProtect failed");
    msg[sizeof(msg) - 1] = 0;
    log(msg);
}

#endif
