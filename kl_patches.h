/*
 * Kane & Lynch: Dead Men - stutter fix code patterns, shared by the in-memory
 * patch DLL and the installer.
 *
 *  1. no-rescan: ZSysInputWintel::Update re-enumerates every DirectInput device
 *     (IDirectInput8::EnumDevices, all classes) every 2.0 s while fewer than
 *     three gamepads are connected. On modern Windows each enumeration blocks the
 *     main thread for ~100-200 ms, producing a hitch every two seconds. The
 *     periodic rescan is skipped; the enumeration at startup is kept.
 *
 *  2. frame-dt: the per-frame delta time is derived from an integer tick counter
 *     in 1/1024 s units, quantising every frame's dt to ~0.98 ms steps. The patch
 *     uses the precise QueryPerformanceCounter delta the engine already computes,
 *     and keeps dt = 0 while the game is paused.
 *
 * Code is located by byte pattern, so any build containing the same code works.
 */
#ifndef KL_PATCHES_H
#define KL_PATCHES_H

#include <stddef.h>

#define KL_WILD 0x100

typedef struct {
    const char *name;
    const short *orig;      /* pattern of unpatched code, KL_WILD = any byte */
    const short *patched;   /* pattern of patched code */
    int len;
    int patch_offset;       /* first byte replaced, relative to the pattern */
    const unsigned char *patch_bytes;
    int patch_len;
} KlPatch;

/* ---- Patch 1: skip periodic DirectInput rescan ----------------------------- */
static const short kl_rescan_orig[] = {
    0x83, 0xC1, 0x28,                   /* add ecx, 0x28                  */
    0x81, 0xC2, 0x00, 0x08, 0x00, 0x00, /* add edx, 0x800  ; 2.0 s        */
    0x3B, 0x11,                         /* cmp edx, [ecx]                 */
    0x7D, 0x11                          /* jge skip_rescan                */
};
static const short kl_rescan_patched[] = {
    0x83, 0xC1, 0x28, 0x81, 0xC2, 0x00, 0x08, 0x00, 0x00, 0x3B, 0x11,
    0xEB, 0x11                          /* jmp skip_rescan                */
};
static const unsigned char kl_rescan_bytes[] = { 0xEB };

/* ---- Patch 2: precise frame delta time ------------------------------------- */
static const short kl_dt_orig[] = {
    0x8B, 0x00,                         /* mov  eax, [eax]                */
    0x2B, 0x01,                         /* sub  eax, [ecx]                */
    0x5F,                               /* pop  edi                       */
    0x89, 0x44, 0x24, 0x04,             /* mov  [esp+4], eax              */
    0xDB, 0x44, 0x24, 0x04,             /* fild dword [esp+4]             */
    0xDC, 0x0D, KL_WILD, KL_WILD, KL_WILD, KL_WILD, /* fmul qword [1/1024] */
    0xD9, 0x5E, 0x50                    /* fstp dword [esi+0x50]          */
};
static const unsigned char kl_dt_bytes[] = {
    0xD9, 0xEE,                         /* fldz                           */
    0x85, 0xC0,                         /* test eax, eax                  */
    0x74, 0x05,                         /* jz   store  ; paused -> 0      */
    0xDD, 0xD8,                         /* fstp st(0)                     */
    0xD9, 0x46, 0x3C,                   /* fld  dword [esi+0x3c]          */
    0xD9, 0x5E, 0x50,                   /* store: fstp dword [esi+0x50]   */
    0x90, 0x90, 0x90
};
static const short kl_dt_patched[] = {
    0x8B, 0x00, 0x2B, 0x01, 0x5F,
    0xD9, 0xEE, 0x85, 0xC0, 0x74, 0x05, 0xDD, 0xD8, 0xD9, 0x46, 0x3C,
    0xD9, 0x5E, 0x50, 0x90, 0x90, 0x90
};

static const KlPatch KL_PATCHES[] = {
    { "no-rescan", kl_rescan_orig, kl_rescan_patched, 13, 11, kl_rescan_bytes, 1 },
    { "frame-dt",  kl_dt_orig,     kl_dt_patched,     22, 5,  kl_dt_bytes,     17 },
};
#define KL_PATCH_COUNT (int)(sizeof(KL_PATCHES) / sizeof(KL_PATCHES[0]))

enum { KL_MISSING, KL_ORIGINAL, KL_PATCHED, KL_AMBIGUOUS };

static int kl_find(const unsigned char *data, size_t size, const short *pat, int len,
                   size_t *found) {
    int count = 0;
    for (size_t i = 0; i + len <= size; i++) {
        int j = 0;
        while (j < len && (pat[j] == KL_WILD || data[i + j] == pat[j])) j++;
        if (j == len) {
            if (count == 0) *found = i;
            count++;
        }
    }
    return count;
}

/* Classifies one patch site in a buffer; *at receives its offset when found. */
static int kl_state(const unsigned char *data, size_t size, const KlPatch *p, size_t *at) {
    size_t a = 0, b = 0;
    int orig = kl_find(data, size, p->orig, p->len, &a);
    int done = kl_find(data, size, p->patched, p->len, &b);
    if (orig + done > 1) return KL_AMBIGUOUS;
    if (orig == 1) { *at = a; return KL_ORIGINAL; }
    if (done == 1) { *at = b; return KL_PATCHED; }
    return KL_MISSING;
}

#endif
