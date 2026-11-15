/* hd-pack.exe: installs the HD pack (FatalPixels textures, SeedVR2 movies).
 *
 * Started by the game in the background (like textures-hd.exe). Works on the
 * folder it sits in (the game folder):
 *   - one at a time (hd-pack.lock held open), IDLE priority;
 *   - reads the manifest hd-pack.txt (HDPACK_SOURCE=<folder>: manifest and zips
 *     from that local folder; the download comes later, plan Task 5);
 *   - nothing to do if textures/hd-pack/hd-pack.txt has the same version;
 *   - each zip into hd-pack-tmp/, size and SHA-256 checked, unpacked into
 *     hd-pack-tmp/stage/; then hp_install (all or nothing);
 *   - then the Real-ESRGAN textures are set aside in hd-pack-ancien/ (never
 *     deleted: the player empties that folder if they want);
 *   - then the verified zips are deleted by their manifest names and the
 *     empty work folders it made are removed, deepest first;
 *   - hd-pack-progress.txt says "working <percent> 100" then "done 0 100"
 *     (removed again if it fails, so the game shows no bar); log hd-pack.log.
 * Deletion safety: see hdpack_core.h. Build (MSYS2 or the cloud's mingw):
 *   x86_64-w64-mingw32-gcc -O2 -municode -I tools -o hd-pack.exe tools/hd-pack.c -lbcrypt
 * Tests: tests/hdpack/run.sh, in a Linux cloud session under Wine only. */
#include "hdpack_core.h"

#define MAX_ZIPS 64
#define MANIFEST_MAX (64 * 1024)

static wchar_t G[HP_WMAX];          /* the game folder */
static wchar_t PROGRESS[HP_WMAX];

static void progress(const char* state, int pct) {
    char b[64];
    int n = snprintf(b, sizeof b, "%s %d 100", state, pct);
    hp_write_own(G, PROGRESS, b, (size_t)n, 0);
}
/* a small text file, at most max-1 bytes; -1 if missing or bigger */
static int read_small(const wchar_t* path, char* out, DWORD max) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return -1;
    DWORD got = 0;
    BOOL ok = ReadFile(f, out, max - 1, &got, NULL);
    char extra; DWORD more = 0;
    if (ok && got == max - 1) ReadFile(f, &extra, 1, &more, NULL);
    CloseHandle(f);
    if (!ok || more) return -1;
    out[got] = 0;
    if (strlen(out) != got) return -1;                         /* a NUL inside: not a text manifest */
    return (int)got;
}
static int size_of(const wchar_t* path, long long* size) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &d)) return -1;
    *size = (long long)d.nFileSizeHigh << 32 | d.nFileSizeLow;
    return 0;
}
/* the zip in tmp is the manifest's one (size and SHA-256) */
static int zip_ok(const wchar_t* path, const HpZip* z) {
    long long size; char sha[65];
    if (!hp_isfile(path) || size_of(path, &size) || size != z->size) return 0;
    if (hp_sha256_fileW(path, sha)) return 0;
    return !strcmp(sha, z->sha);
}

int wmain(void) {
    /* the game folder: where this exe is */
    DWORD n = GetModuleFileNameW(NULL, G, HP_WMAX);
    if (!n || n >= HP_WMAX) return 1;
    wchar_t* s = wcsrchr(G, L'\\');
    if (!s) return 1;
    *s = 0;
    if (!SetCurrentDirectoryW(G)) return 1;
    if (hp_join(PROGRESS, G, L"hd-pack-progress.txt")) return 1;

    /* one at a time: a file held open, gone when this ends even if killed */
    wchar_t lock[HP_WMAX];
    if (hp_join(lock, G, L"hd-pack.lock") || !hp_safe_path(G, lock)) return 0;
    HANDLE lh = CreateFileW(lock, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (lh == INVALID_HANDLE_VALUE) return 0;                   /* another one is at work */
    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);  /* the game comes first */

    /* the manifest */
    wchar_t src[HP_WMAX], path[HP_WMAX];
    DWORD sl = GetEnvironmentVariableW(L"HDPACK_SOURCE", src, HP_WMAX);
    if (!sl || sl >= HP_WMAX) { CloseHandle(lh); return 0; }   /* no local pack: the download is Task 5 */
    static char manifest[MANIFEST_MAX];
    static HpZip zips[MAX_ZIPS];
    int version = -1, nz;
    if (hp_join(path, src, L"hd-pack.txt") || read_small(path, manifest, sizeof manifest) < 0
        || (nz = hp_parse_manifest(manifest, &version, zips, MAX_ZIPS)) <= 0) {
        CloseHandle(lh); return 1;                             /* nothing changed, nothing to log to yet */
    }

    /* installed already? (no log, no progress file: nothing touched) */
    {
        static char have[MANIFEST_MAX]; static HpZip hz[MAX_ZIPS]; int hv = -1;
        if (!hp_join(path, G, L"textures\\hd-pack\\hd-pack.txt") && read_small(path, have, sizeof have) >= 0
            && hp_parse_manifest(have, &hv, hz, MAX_ZIPS) >= 0 && hv == version) { CloseHandle(lh); return 0; }
    }

    wchar_t logp[HP_WMAX];
    if (!hp_join(logp, G, L"hd-pack.log") && hp_safe_path(G, logp) && (!hp_exists(logp) || hp_isfile(logp)))
        hp_logfile = _wfopen(logp, L"ab");
    SYSTEMTIME now; GetLocalTime(&now);
    hp_log("\n%04d-%02d-%02d %02d:%02d hd-pack %d: %d zip(s) from %s\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, version, nz, hp_u8(src));
    progress("working", 0);

    int ok = 0;
    wchar_t tmp[HP_WMAX], stage[HP_WMAX], moved[HP_WMAX];
    if (hp_join(tmp, G, L"hd-pack-tmp") || hp_join(stage, tmp, L"stage")) goto fail;
    if (hp_mkdir(G, tmp)) goto fail;
    if (hp_exists(stage)) {                                    /* left by a run that was killed: set aside, never used */
        if (!hp_isdir(stage) || hp_aside(G, stage, L"stage-interrompu", moved)) goto fail;
        hp_log("  unfinished stage of an earlier run set aside: %s\n", hp_u8(moved));
    }
    if (hp_mkdir(G, stage)) goto fail;

    for (int i = 0; i < nz; i++) {
        wchar_t zname[128], from[HP_WMAX], zpath[HP_WMAX];
        if (hp_wide(zips[i].name, zname) || hp_join(from, src, zname) || hp_join(zpath, tmp, zname)) goto fail;
        if (!hp_safe_path(G, zpath)) { hp_log("  refused (unsafe path): %s\n", hp_u8(zpath)); goto fail; }
        if (hp_exists(zpath) && !zip_ok(zpath, &zips[i])) {   /* ours from an earlier run, not the right one */
            if (hp_delete_own(G, zpath)) goto fail;
        }
        if (!hp_exists(zpath)) {
            if (!CopyFileW(from, zpath, TRUE)) { hp_log("  can't copy %s (error %lu)\n", hp_u8(from), GetLastError()); goto fail; }
            if (!zip_ok(zpath, &zips[i])) {
                hp_log("  %s: size or SHA-256 differs from the manifest\n", zips[i].name);
                hp_delete_own(G, zpath);
                goto fail;
            }
        } else hp_log("  %s: already here and checked\n", zips[i].name);
        if (hp_unzip_storedW(zpath, stage)) { hp_log("  %s: can't unpack\n", zips[i].name); goto fail; }
        progress("working", (i + 1) * 90 / nz);
    }

    wchar_t smark[HP_WMAX];
    if (hp_join(smark, stage, L"hd-pack.txt") || hp_write_own(G, smark, manifest, strlen(manifest), 1)) goto fail;
    if (hp_list_add(&hp_written, smark)) goto fail;
    if (hp_installW(G, stage)) goto fail;
    ok = 1;
    progress("working", 95);

    if (hp_realesrgan_packW(G)) {
        int left = hp_set_asideW(G);
        hp_log("  Real-ESRGAN textures set aside in hd-pack-ancien (empty that folder to free the space)%s\n",
               left ? ", some left in place (see above)" : "");
    }

    /* tidy up: the verified zips by their names, then the empty folders made */
    for (int i = 0; i < nz; i++) {
        wchar_t zname[128], zpath[HP_WMAX];
        if (!hp_wide(zips[i].name, zname) && !hp_join(zpath, tmp, zname)) hp_delete_own(G, zpath);
    }
    hp_rmdirs_since(G, 0);
    hp_rmdir_own(G, stage);                                    /* ours from an earlier run, if empty */
    hp_rmdir_own(G, tmp);
    progress("done", 0);
    hp_log("  done\n");
    CloseHandle(lh);
    return 0;

fail:
    /* the files this run unpacked, by exact name; verified zips stay for next time */
    if (!ok) {
        for (int i = hp_written.n - 1; i >= 0; i--) hp_delete_own(G, hp_written.v[i]);
        hp_rmdirs_since(G, 0);
        hp_log("  failed: nothing installed, nothing set aside\n");
    }
    hp_delete_own(G, PROGRESS);                                /* no bar left saying "working" */
    CloseHandle(lh);
    return 1;
}
