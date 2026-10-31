#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../xliveless.h"
#include "required_ordinals.h"

#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
template<class T> T ordinal(HMODULE dll, WORD n) { return reinterpret_cast<T>(GetProcAddress(dll, MAKEINTRESOURCEA(n))); }

int wmain(int argc, wchar_t **argv)
{
    if (argc != 4) return 2;
    REQUIRE(SetEnvironmentVariableW(L"KL_NOGFWL_PROFILE_ROOT", argv[3]));
    HMODULE dll = LoadLibraryW(argv[2]);
    REQUIRE(dll);
    for (WORD required : kRequiredOrdinals)
        REQUIRE(GetProcAddress(dll, MAKEINTRESOURCEA(required)));
    auto version = reinterpret_cast<const char *(__cdecl*)()>(GetProcAddress(dll,"KLNoGFWLVersion"));
    REQUIRE(version && std::strstr(version(),"built from source"));
    auto init = ordinal<int(WINAPI*)(DWORD)>(dll,5000);
    auto signin = ordinal<XUSER_SIGNIN_STATE(WINAPI*)(DWORD)>(dll,5262);
    auto name = ordinal<DWORD(WINAPI*)(DWORD,LPSTR,DWORD)>(dll,5263);
    auto xuid = ordinal<int(WINAPI*)(DWORD,PXUID)>(dll,5261);
    auto info = ordinal<int(WINAPI*)(DWORD,DWORD,PXUSER_SIGNIN_INFO)>(dll,5267);
    auto read = ordinal<DWORD(WINAPI*)(DWORD,DWORD,DWORD,DWORD*,DWORD*,XUSER_READ_PROFILE_SETTING_RESULT*,PXOVERLAPPED)>(dll,5331);
    auto write = ordinal<DWORD(WINAPI*)(DWORD,DWORD,const PXUSER_PROFILE_SETTING,PXOVERLAPPED)>(dll,5337);
    REQUIRE(init && signin && name && xuid && info && read && write);
    REQUIRE(init(0) == 0);
    REQUIRE(signin(0) == eXUserSigninState_SignedInLocally);
    REQUIRE(signin(1) == eXUserSigninState_NotSignedIn);
    REQUIRE(signin(0xffffffff) == eXUserSigninState_NotSignedIn);
    XUID id = 0;
    REQUIRE(xuid(0,&id) == 0 && id != 0);
    REQUIRE(xuid(0xffffffff,&id) != 0);
    REQUIRE(info(0,0,nullptr) == ERROR_INVALID_PARAMETER);
    XUSER_SIGNIN_INFO user = {};
    REQUIRE(info(0,0,&user) == 0 && user.UserSigninState == eXUserSigninState_SignedInLocally);
    struct { char text[2]; char guard[8]; } tiny;
    std::memset(&tiny,0x5a,sizeof(tiny));
    REQUIRE(name(0,tiny.text,sizeof(tiny.text)) == ERROR_INSUFFICIENT_BUFFER);
    for (char c : tiny.guard) REQUIRE(c == 0x5a);
    char username[16];
    REQUIRE(name(0,username,sizeof(username)) == 0 && std::strlen(username) > 0);
    REQUIRE(name(0xffffffff,username,sizeof(username)) == ERROR_NO_SUCH_USER);

    auto listen = ordinal<HANDLE(WINAPI*)(ULONGLONG)>(dll,5270);
    auto next = ordinal<BOOL(WINAPI*)(HANDLE,DWORD,PDWORD,PULONG_PTR)>(dll,651);
    REQUIRE(listen && next);
    HANDLE listener = listen(XNOTIFY_SYSTEM);
    REQUIRE(listener && WaitForSingleObject(listener,0) == WAIT_OBJECT_0);
    // Begin the normal sign-in event sequence, then read its notification.
    DWORD event = 0; ULONG_PTR param = 0;
    REQUIRE(next(listener,0,&event,&param));
    // The upstream shim completes its synthetic UI transition on a short timer.
    Sleep(50);
    REQUIRE(next(listener,XN_SYS_UI,&event,&param) && param == 0);
    REQUIRE(next(listener,XN_SYS_SIGNINCHANGED,&event,&param));
    REQUIRE(param == 1);
    CloseHandle(listener);

    DWORD settingId = 0x60403fff; // binary, maximum 64 bytes, title-specific slot 1
    BYTE bytes[] = {0x00,0x7f,0xff,0x01,0x42,0x00,0x88};
    if (!wcscmp(argv[1],L"write"))
    {
        XUSER_PROFILE_SETTING setting = {};
        setting.dwSettingId = settingId;
        setting.data.type = 6;
        setting.data.binary.cbData = sizeof(bytes);
        setting.data.binary.pbData = bytes;
        REQUIRE(write(0,1,&setting,nullptr) == ERROR_SUCCESS);
        // A rejected write must leave the previous good value intact.
        setting.data.binary.cbData = 65;
        REQUIRE(write(0,1,&setting,nullptr) == ERROR_INVALID_PARAMETER);
    }
    DWORD count = 0;
    REQUIRE(read(0,0,1,&settingId,&count,nullptr,nullptr) == ERROR_INSUFFICIENT_BUFFER);
    REQUIRE(count >= sizeof(XUSER_READ_PROFILE_SETTING_RESULT) + sizeof(XUSER_PROFILE_SETTING) + 64);
    std::vector<BYTE> storage(count + 16,0x5a);
    DWORD shortCount = count - 1;
    REQUIRE(read(0,0,1,&settingId,&shortCount,reinterpret_cast<XUSER_READ_PROFILE_SETTING_RESULT*>(storage.data()),nullptr) == ERROR_INSUFFICIENT_BUFFER);
    for (BYTE b : storage) REQUIRE(b == 0x5a);
    auto result = reinterpret_cast<XUSER_READ_PROFILE_SETTING_RESULT*>(storage.data());
    REQUIRE(read(0,0,1,&settingId,&count,result,nullptr) == ERROR_SUCCESS);
    for (size_t i = count; i < storage.size(); ++i) REQUIRE(storage[i] == 0x5a);
    if (!wcscmp(argv[1],L"corrupt"))
        REQUIRE(result->pSettings[0].source == XSOURCE_NO_VALUE);
    else
    {
        REQUIRE(result->pSettings[0].source == XSOURCE_TITLE);
        REQUIRE(result->pSettings[0].data.binary.cbData == sizeof(bytes));
        REQUIRE(!std::memcmp(result->pSettings[0].data.binary.pbData,bytes,sizeof(bytes)));
    }
    XOVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    REQUIRE(read(0,0,1,&settingId,&count,result,&overlapped) == ERROR_IO_PENDING);
    REQUIRE(overlapped.InternalLow == ERROR_SUCCESS && WaitForSingleObject(overlapped.hEvent,0) == WAIT_OBJECT_0);
    CloseHandle(overlapped.hEvent);
    FreeLibrary(dll);
    std::puts("PASS: independent identity, offline user, bounds, notifications, profile persistence, completion event");
    return 0;
}
