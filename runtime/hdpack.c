/* The HD pack: see hdpack.h. */
#include "hdpack.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <strings.h>
#endif

static int same(const char* a, const char* b) {
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

int hdpack_dir_rank(const char* top_folder) {
    if (!top_folder) return 0;
    if (same(top_folder, "hd-pack")) return 1;
    if (same(top_folder, "hd")) return 2;
    return 0;
}

void hdpack_start(int enabled) {
#ifdef _WIN32
    if (!enabled) return;
    wchar_t exe[MAX_PATH + 16], cmd[MAX_PATH + 24];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    wchar_t* s = wcsrchr(exe, L'\\');
    if (!s) return;
    wcscpy(s + 1, L"hd-pack.exe");
    DWORD a = GetFileAttributesW(exe);
    if (a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY)) return;
    _snwprintf(cmd, MAX_PATH + 24, L"\"%ls\"", exe);
    cmd[MAX_PATH + 23] = 0;
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    if (CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | IDLE_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
#else
    (void)enabled;                 /* Linux / Steam Deck: later (design, "Reste au builder") */
#endif
}
