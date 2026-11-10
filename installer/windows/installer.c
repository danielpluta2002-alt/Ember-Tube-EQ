// Ember Tube EQ - Windows Installer / Uninstaller (self-contained)
// Installs VST3 + CLAP into the standard system folders and registers an uninstaller.
#include <windows.h>
#include <shlobj.h>
#include <wchar.h>

#define APP_NAME    L"Ember Tube EQ"
#define APP_VERSION L"1.0.0"
#define APP_VENDOR  L"Ember Audio"
#define UNINST_KEY  L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EmberTubeEQ"

#define RES_VST3 101
#define RES_CLAP 102

static void path_join(wchar_t* out, const wchar_t* a, const wchar_t* b)
{
    lstrcpyW(out, a);
    size_t n = lstrlenW(out);
    if (n && out[n - 1] != L'\\') lstrcatW(out, L"\\");
    lstrcatW(out, b);
}

static BOOL mkdirs(const wchar_t* path)
{
    wchar_t tmp[MAX_PATH * 2];
    lstrcpyW(tmp, path);
    for (wchar_t* p = tmp + 3; *p; ++p)
        if (*p == L'\\') { *p = 0; CreateDirectoryW(tmp, NULL); *p = L'\\'; }
    CreateDirectoryW(tmp, NULL);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static BOOL write_resource(int id, const wchar_t* dest)
{
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
    if (!r) return FALSE;
    HGLOBAL g = LoadResource(NULL, r);
    DWORD size = SizeofResource(NULL, r);
    const void* data = LockResource(g);
    HANDLE f = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    BOOL ok = WriteFile(f, data, size, &written, NULL) && written == size;
    CloseHandle(f);
    return ok;
}

static void get_paths(wchar_t* vst3Dir, wchar_t* vst3File, wchar_t* clapDir, wchar_t* clapFile, wchar_t* appDir)
{
    wchar_t common[MAX_PATH], progs[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, 0, common);
    SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, progs);

    wchar_t tmp[MAX_PATH * 2];
    path_join(vst3Dir, common, L"VST3\\EmberEQ.vst3");
    path_join(tmp, vst3Dir, L"Contents\\x86_64-win");
    path_join(vst3File, tmp, L"EmberEQ.vst3");
    path_join(clapDir, common, L"CLAP");
    path_join(clapFile, clapDir, L"EmberEQ.clap");
    path_join(appDir, progs, L"Ember Audio\\Ember Tube EQ");
}

static int do_install(void)
{
    wchar_t vst3Dir[MAX_PATH * 2], vst3File[MAX_PATH * 2], clapDir[MAX_PATH * 2], clapFile[MAX_PATH * 2], appDir[MAX_PATH * 2];
    get_paths(vst3Dir, vst3File, clapDir, clapFile, appDir);

    wchar_t msg[4096];
    wsprintfW(msg,
        APP_NAME L" " APP_VERSION L" wird installiert.\n\n"
        L"VST3:  %s\n"
        L"CLAP:  %s\n\n"
        L"Bitte schließe vorher deine DAW.\n\nJetzt installieren?",
        vst3Dir, clapFile);
    if (MessageBoxW(NULL, msg, APP_NAME L" Setup", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 1;

    wchar_t vst3Bin[MAX_PATH * 2];
    path_join(vst3Bin, vst3Dir, L"Contents\\x86_64-win");

    if (!mkdirs(vst3Bin) || !mkdirs(clapDir) || !mkdirs(appDir) ||
        !write_resource(RES_VST3, vst3File) || !write_resource(RES_CLAP, clapFile))
    {
        MessageBoxW(NULL, L"Die Installation ist fehlgeschlagen.\n\n"
                          L"Ist die DAW noch geöffnet? Bitte schließen und erneut versuchen.",
                    APP_NAME L" Setup", MB_OK | MB_ICONERROR);
        return 2;
    }

    // copy ourselves as uninstaller + register in "Apps & Features"
    wchar_t self[MAX_PATH], uninst[MAX_PATH * 2], cmd[MAX_PATH * 3];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    path_join(uninst, appDir, L"Uninstall.exe");
    CopyFileW(self, uninst, FALSE);
    wsprintfW(cmd, L"\"%s\" /uninstall", uninst);

    HKEY key;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, UNINST_KEY, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, L"DisplayName", 0, REG_SZ, (const BYTE*)APP_NAME, sizeof(APP_NAME));
        RegSetValueExW(key, L"DisplayVersion", 0, REG_SZ, (const BYTE*)APP_VERSION, sizeof(APP_VERSION));
        RegSetValueExW(key, L"Publisher", 0, REG_SZ, (const BYTE*)APP_VENDOR, sizeof(APP_VENDOR));
        RegSetValueExW(key, L"InstallLocation", 0, REG_SZ, (const BYTE*)appDir, (lstrlenW(appDir) + 1) * 2);
        RegSetValueExW(key, L"UninstallString", 0, REG_SZ, (const BYTE*)cmd, (lstrlenW(cmd) + 1) * 2);
        DWORD one = 1;
        RegSetValueExW(key, L"NoModify", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
        RegSetValueExW(key, L"NoRepair", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
        RegCloseKey(key);
    }

    MessageBoxW(NULL, APP_NAME L" wurde erfolgreich installiert.\n\n"
                      L"Starte deine DAW und lass sie nach neuen Plugins suchen.\n"
                      L"Du findest es unter \"Ember Audio\" > \"Ember Tube EQ\".",
                APP_NAME L" Setup", MB_OK | MB_ICONINFORMATION);
    return 0;
}

static void remove_tree(const wchar_t* dir)
{
    wchar_t pattern[MAX_PATH * 2];
    path_join(pattern, dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do {
            if (!lstrcmpW(fd.cFileName, L".") || !lstrcmpW(fd.cFileName, L"..")) continue;
            wchar_t child[MAX_PATH * 2];
            path_join(child, dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) remove_tree(child);
            else DeleteFileW(child);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir);
}

static int do_uninstall(void)
{
    wchar_t vst3Dir[MAX_PATH * 2], vst3File[MAX_PATH * 2], clapDir[MAX_PATH * 2], clapFile[MAX_PATH * 2], appDir[MAX_PATH * 2];
    get_paths(vst3Dir, vst3File, clapDir, clapFile, appDir);

    if (MessageBoxW(NULL, APP_NAME L" wirklich deinstallieren?", APP_NAME,
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 1;

    remove_tree(vst3Dir);
    DeleteFileW(clapFile);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, UNINST_KEY);

    // the uninstaller can't delete itself while running: schedule removal on reboot
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    MoveFileExW(self, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileExW(appDir, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    MessageBoxW(NULL, APP_NAME L" wurde entfernt.", APP_NAME, MB_OK | MB_ICONINFORMATION);
    return 0;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdLine, int show)
{
    (void)inst; (void)prev; (void)show;
    if (cmdLine && wcsstr(cmdLine, L"/uninstall"))
        return do_uninstall();
    return do_install();
}
