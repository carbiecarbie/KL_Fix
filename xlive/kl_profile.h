// MIT; KL NoGFWL contributors. Included after the upstream completion helper.
// Store profile values with bounded reads and atomic per-setting replacement.
// The three title-specific binary settings retain the public XLiveLess format.
static DWORD KLComplete(DWORD result, PXOVERLAPPED overlapped, DWORD bytes = 0)
{
    if (!overlapped) return result;
    overlapped->InternalLow = result;
    overlapped->InternalHigh = bytes;
    overlapped->dwExtendedError = result;
    Check_Overlapped(overlapped);
    return ERROR_IO_PENDING;
}

static bool KLSettingPath(DWORD user, DWORD id, WCHAR (&path)[512])
{
    Local_Storage_W(user, path);
    if (!path[0]) return false;
    wcscat_s(path, L"\\Offline");
    CreateDirectoryW(path, NULL);
    switch (id & 0x3fff)
    {
    case 0x3fff: wcscat_s(path, L"\\Title1.dat"); break;
    case 0x3ffe: wcscat_s(path, L"\\Title2.dat"); break;
    case 0x3ffd: wcscat_s(path, L"\\Title3.dat"); break;
    default:
        WCHAR name[32];
        swprintf_s(name, L"\\Setting_%08X.dat", id);
        wcscat_s(path, name);
    }
    return true;
}

DWORD WINAPI XUserReadProfileSettings(DWORD, DWORD user, DWORD count, DWORD *ids,
    DWORD *bufferBytes, XUSER_READ_PROFILE_SETTING_RESULT *results, PXOVERLAPPED overlapped)
{
    if (user >= 4 || !g_signin[user]) return KLComplete(ERROR_NO_SUCH_USER, overlapped);
    if (!bufferBytes || !ids || !count || count > 64)
        return KLComplete(ERROR_INVALID_PARAMETER, overlapped);
    DWORD required = sizeof(*results) + count * sizeof(XUSER_PROFILE_SETTING);
    for (DWORD i = 0; i < count; ++i) required += (ids[i] >> 16) & 0xfff;
    if (!results || *bufferBytes < required)
    {
        *bufferBytes = required;
        return KLComplete(ERROR_INSUFFICIENT_BUFFER, overlapped, required);
    }
    *bufferBytes = required;
    memset(results, 0, required);
    results->dwSettingsLen = count;
    results->pSettings = reinterpret_cast<XUSER_PROFILE_SETTING *>(results + 1);
    BYTE *payload = reinterpret_cast<BYTE *>(results->pSettings + count);
    for (DWORD i = 0; i < count; ++i)
    {
        auto &setting = results->pSettings[i];
        DWORD capacity = (ids[i] >> 16) & 0xfff;
        BYTE type = static_cast<BYTE>(ids[i] >> 28);
        setting.user.dwUserIndex = user;
        setting.dwSettingId = ids[i];
        setting.data.type = type;
        WCHAR path[512];
        FILE *file = KLSettingPath(user, ids[i], path) ? _wfopen(path, L"rb") : NULL;
        if (file)
        {
            DWORD length = 0;
            BYTE *destination = payload;
            if (type != 4 && type != 6)
                destination = reinterpret_cast<BYTE *>(&setting.data.nData);
            DWORD limit = (type == 4 || type == 6) ? capacity : 8;
            if (fread(&length, sizeof(length), 1, file) == 1 && length <= limit &&
                fread(destination, 1, length, file) == length)
            {
                setting.source = XSOURCE_TITLE;
                if (type == 6) { setting.data.binary.cbData = length; setting.data.binary.pbData = payload; }
                if (type == 4) { setting.data.string.cbData = length; setting.data.string.pwszData = reinterpret_cast<LPWSTR>(payload); }
            }
            fclose(file);
        }
        payload += capacity;
    }
    return KLComplete(ERROR_SUCCESS, overlapped, required);
}

DWORD WINAPI XUserWriteProfileSettings(DWORD user, DWORD count,
    const PXUSER_PROFILE_SETTING settings, PXOVERLAPPED overlapped)
{
    if (user >= 4 || !g_signin[user]) return KLComplete(ERROR_NO_SUCH_USER, overlapped);
    if (!settings || !count || count > 64) return KLComplete(ERROR_INVALID_PARAMETER, overlapped);
    // Validate the complete request before changing any profile value.
    for (DWORD i = 0; i < count; ++i)
    {
        const auto &setting = settings[i];
        DWORD type = setting.dwSettingId >> 28;
        DWORD capacity = (setting.dwSettingId >> 16) & 0xfff;
        DWORD length = type == 6 ? setting.data.binary.cbData : setting.data.string.cbData;
        if (type > 7 || type != setting.data.type ||
            ((type == 4 || type == 6) && (length > capacity || (length && !setting.data.binary.pbData))))
            return KLComplete(ERROR_INVALID_PARAMETER, overlapped);
    }
    for (DWORD i = 0; i < count; ++i)
    {
        const auto &setting = settings[i];
        DWORD type = setting.dwSettingId >> 28;
        DWORD length = (type == 1 || type == 5) ? 4 : 8;
        const BYTE *payload = reinterpret_cast<const BYTE *>(&setting.data.nData);
        if (type == 6) { length = setting.data.binary.cbData; payload = setting.data.binary.pbData; }
        if (type == 4) { length = setting.data.string.cbData; payload = reinterpret_cast<const BYTE *>(setting.data.string.pwszData); }
        WCHAR path[512], temp[512], directory[512];
        if (!KLSettingPath(user, setting.dwSettingId, path)) return KLComplete(ERROR_PATH_NOT_FOUND, overlapped);
        wcscpy_s(directory, path);
        *wcsrchr(directory, L'\\') = 0;
        if (!GetTempFileNameW(directory, L"KLN", 0, temp)) return KLComplete(GetLastError(), overlapped);
        FILE *file = _wfopen(temp, L"wb");
        if (!file) { DeleteFileW(temp); return KLComplete(ERROR_WRITE_FAULT, overlapped); }
        bool good = fwrite(&length, sizeof(length), 1, file) == 1;
        if (good && length) good = fwrite(payload, 1, length, file) == length;
        if (fflush(file)) good = false;
        if (fclose(file)) good = false;
        if (!good) { DeleteFileW(temp); return KLComplete(ERROR_WRITE_FAULT, overlapped); }
        if (!MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DWORD error = GetLastError();
            DeleteFileW(temp);
            return KLComplete(error, overlapped);
        }
    }
    return KLComplete(ERROR_SUCCESS, overlapped);
}
