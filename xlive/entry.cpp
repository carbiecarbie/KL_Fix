// Standalone host for the openly licensed XLiveLess compatibility layer.
// This DLL loads no GOG or Microsoft xlive implementation.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// The open-source Ultimate ASI Loader also makes the main image writable when
// hosting XLiveLess. GFWL titles expect these permissions during initialization.
// Only page permissions change; executable bytes and the file on disk do not.
static BOOL PrepareImage()
{
    BYTE *base = reinterpret_cast<BYTE *>(GetModuleHandleW(NULL));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;
    auto section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
    {
        if (memcmp(section->Name,".text",5) && memcmp(section->Name,".rdata",6))
            continue;
        SIZE_T size = section->Misc.VirtualSize;
        if (!size) continue;
        DWORD oldProtection;
        DWORD protection = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE)
            ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
        if (!VirtualProtect(base + section->VirtualAddress,size,protection,&oldProtection))
            return FALSE;
    }
    return TRUE;
}

extern "C" const char* __cdecl KLNoGFWLVersion()
{
    return "KL NoGFWL 0.1.0; XLiveLess; offline; built from source";
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        return PrepareImage();
    }
    return TRUE;
}
