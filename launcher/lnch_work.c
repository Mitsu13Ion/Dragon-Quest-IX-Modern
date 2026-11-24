/* See lnch_work.h. Paths are relative to the game folder (the launcher's
 * own, where main() goes first); the project is the folder above it.
 * Order (spec): check for an update, install it and build, then the HD pack.
 * Linux: the news only (the updater and hd-pack.exe are Windows' for now). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnch_work.h"
#include "update.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define RELEASES_PATH "/repos/" UPDATE_REPO "/releases?per_page=10"

static char g_notes_lang[8];
static SDL_Thread* g_thread;
static SDL_atomic_t g_done = {1};          /* the thread has finished (or never ran) */
static SDL_atomic_t g_restart;              /* the build put a new launcher in place */

static int exists(const char* p) { FILE* f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

/* the version of the project here: DQIX_VERSION at a line's start in ../runtime/version.h */
static void local_version(char* out, size_t n) {
    out[0] = 0;
    FILE* f = fopen("../runtime/version.h", "rb");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, "#define DQIX_VERSION \"", 22)) {
            char* e = strchr(line + 22, '"');
            if (e) { *e = 0; snprintf(out, n, "%.31s", line + 22); }
            break;
        }
    fclose(f);
}

static void settings_wants(LState* s) {
    char* t; char v[32];
    ini_load("settings.ini", &t);
    int classic = ini_get(t, "mode", v, sizeof v) && strcmp(v, "enhanced") != 0;
    int pack = !ini_get(t, "hd_pack_download", v, sizeof v) || atoi(v) != 0;
    s->hd_pack_wanted = pack && !classic;
    s->update_check = !ini_get(t, "update_check", v, sizeof v) || atoi(v) != 0;
    free(t);
}

static void scan_local(Model* m) {
#ifdef _WIN32
    m->st.platform_updates = 1;
    m->st.game_built = exists("dqix.exe") && exists("data/dqix.nds");
#else
    m->st.platform_updates = 0;
    m->st.game_built = exists("dqix") && exists("data/dqix.nds");
#endif
    m->st.pack_complete = exists("textures/hd-pack/hd-pack.txt");
    m->background_ready = exists("textures/hd-pack/launcher-back.jpg");
    local_version(m->st.installed, sizeof m->st.installed);
    settings_wants(&m->st);
    m->pack_version[0] = 0;
    FILE* f = fopen("textures/hd-pack/hd-pack.txt", "rb");
    if (f) { int v = 0; if (fscanf(f, "hd-pack %d", &v) == 1 && v > 0) snprintf(m->pack_version, sizeof m->pack_version, "hd-pack-%d", v); fclose(f); }
}

/* ---- writing what the screen shows */
#define LOCK SDL_LockMutex(g_m.lock)
#define UNLOCK SDL_UnlockMutex(g_m.lock)
static void bar(Bar* b, const char* text, const char* right, float frac, int state) {
    LOCK;
    snprintf(b->text, sizeof b->text, "%s", text ? text : "");
    snprintf(b->right, sizeof b->right, "%.79s", right ? right : "");
    b->frac = frac; b->state = state;
    UNLOCK;
}
static void step(StepState* s, StepState v) { LOCK; *s = v; UNLOCK; }

/* "610 Mo" / "1,4 Go" in French, "610 MB" / "1.4 GB" otherwise */
static void size_text(long long b, char* out, size_t n) {
    int fr = !strcmp(g_notes_lang, "fr");
    if (b >= 1000LL * 1000 * 1000) {
        snprintf(out, n, "%.1f %s", b / 1e9, fr ? "Go" : "GB");
        if (fr) { char* d = strchr(out, '.'); if (d) *d = ','; }
    } else snprintf(out, n, "%lld %s", b / 1000000, fr ? "Mo" : "MB");
}
static Uint64 g_dl_t0;
static long long g_dl_b0;
/* "610 Mo / 1,4 Go · 8,2 Mo/s · 1 min 40" (what is known of it) */
static void dl_right(long long done, long long total, char* out, size_t n) {
    char a[24], b[24], sp[32] = "", eta[32] = "";
    size_text(done, a, sizeof a);
    Uint64 now = SDL_GetTicks64();
    if (!g_dl_t0 || done < g_dl_b0) { g_dl_t0 = now; g_dl_b0 = done; }
    double secs = (now - g_dl_t0) / 1000.0;
    if (secs > 1.5) {
        double rate = (done - g_dl_b0) / secs;
        char r[24]; size_text((long long)rate, r, sizeof r);
        snprintf(sp, sizeof sp, " \xC2\xB7 %s/s", r);
        if (total > 0 && rate > 1) {
            long long left = (long long)((total - done) / rate);
            if (left >= 60) snprintf(eta, sizeof eta, " \xC2\xB7 %lld min %02lld", left / 60, left % 60);
            else snprintf(eta, sizeof eta, " \xC2\xB7 %lld s", left);
        }
    }
    if (total > 0) { size_text(total, b, sizeof b); snprintf(out, n, "%s / %s%s%s", a, b, sp, eta); }
    else snprintf(out, n, "%s%s", a, sp);
}

/* ---- the news: GitHub's releases; a title from the notes here when there is one (they are translated) */
static void news_titles_from_notes(NewsItem* it, int n) {
    for (int i = 0; i < n; i++) {
        char p[128], text[4096];
        const char* langs[2] = {g_notes_lang, ""};
        for (int k = 0; k < 2; k++) {
            if (k == 0 && !g_notes_lang[0]) continue;
            if (langs[k][0]) snprintf(p, sizeof p, "notes/%s/%.23s.md", langs[k], it[i].tag); else snprintf(p, sizeof p, "notes/%.23s.md", it[i].tag);
            FILE* f = fopen(p, "rb");
            if (!f) continue;
            size_t got = fread(text, 1, sizeof text - 1, f); fclose(f);
            text[got] = 0;
            if (news_title_from_notes(text, it[i].title, sizeof it[i].title)) break;
        }
    }
}
static int fetch_news(char** json) {
    size_t len; char err[160];
    const char* src = getenv("DQIX_UPDATE_SOURCE");     /* tests: <folder>/releases.json */
    if (src && *src) {
        char p[1024]; snprintf(p, sizeof p, "%s/releases.json", src);
        FILE* f = fopen(p, "rb");
        if (!f) return 0;
        *json = calloc(1, 1 << 20);
        if (*json) { size_t got = fread(*json, 1, (1 << 20) - 1, f); (*json)[got] = 0; }
        fclose(f);
        return *json != NULL;
    }
#ifdef _WIN32
    return update_https_get("api.github.com", RELEASES_PATH, json, &len, 1 << 20, err, sizeof err);
#else
    const char* argv[] = {"curl", "-fsSL", "--max-time", "15", "-H", "User-Agent: dqix-launcher", "https://api.github.com" RELEASES_PATH, NULL};
    LProc* p = proc_start(argv, NULL, NULL);
    if (!p) return 0;
    size_t cap = 1 << 16; len = 0;
    char* b = malloc(cap); char line[4096]; int r;
    while (b && (r = proc_read_line(p, line, sizeof line)) != -1) {
        if (r == 0) { SDL_Delay(20); continue; }
        size_t l = strlen(line);
        if (len + l + 2 > cap) { if (cap > (1 << 22)) break; cap *= 2; char* b2 = realloc(b, cap); if (!b2) break; b = b2; }
        memcpy(b + len, line, l); len += l; b[len++] = '\n';
    }
    int ok = proc_exit_code(p) == 0 && b;
    proc_free(p);
    if (b) b[len] = 0;
    if (!ok) { free(b); return 0; }
    *json = b;
    (void)err;
    return 1;
#endif
}
static int do_news(void) {
    char* json = NULL;
    if (!fetch_news(&json)) return 0;
    NewsItem it[3];
    int n = news_from_github(json, it, 3);
    free(json);
    if (n <= 0) return 0;
    news_titles_from_notes(it, n);
    LOCK; memcpy(g_m.news, it, sizeof it); g_m.nnews = n; g_m.news_offline = 0; UNLOCK;
    return 1;
}

#ifdef _WIN32
/* the project's new version in place, like the game's updater (runtime/update.c
 * APPLY_BAT) without waiting for a game or building: run from update\ */
static const char APPLY[] =
    "@echo off\r\n"
    "rem Written by the launcher to install an update. Safe to delete.\r\n"
    "setlocal\r\n"
    "set NoDefaultCurrentDirectoryInExePath=\r\n"
    "cd /d \"%~dp0..\"\r\n"
    "if exist update\\previous rmdir /s /q update\\previous\r\n"
    "robocopy . update\\previous /E /NFL /NDL /NJH /NJS /NP /XD game build rom extras update bookmarks reports release .git >nul\r\n"
    "if errorlevel 8 (echo FAILED robocopy: keeping a copy of the current version & exit /b 1)\r\n"
    "if exist update\\new rmdir /s /q update\\new\r\n"
    "mkdir update\\new\r\n"
    "tar -xf update\\download.zip -C update\\new\r\n"
    "if errorlevel 1 (echo FAILED tar: unpacking the new version & exit /b 2)\r\n"
    "set \"SRC=\"\r\n"
    "for /d %%d in (update\\new\\*) do if not defined SRC set \"SRC=%%d\"\r\n"
    "if not defined SRC (echo FAILED the download holds no project & exit /b 3)\r\n"
    "if not exist \"%SRC%\\BUILD.bat\" (echo FAILED the download holds no project & exit /b 3)\r\n"
    "robocopy \"%SRC%\" . /E /NFL /NDL /NJH /NJS /NP >nul\r\n"
    "if errorlevel 8 (echo FAILED robocopy: copying the new version & exit /b 4)\r\n"
    "rmdir /s /q update\\new\r\n"
    "exit /b 0\r\n";

static int write_text(const char* path, const char* t) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t n = strlen(t);
    int ok = fwrite(t, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* run a tool and wait: its lines go to on_line; 0 on success, else its exit code (or -1 not started) */
static int run(const char* const* argv, const char* const* env, const char* cwd, void (*on_line)(const char*, void*), void* u) {
    LProc* p = proc_start(argv, env, cwd);
    if (!p) return -1;
    char line[1024]; int r;
    while ((r = proc_read_line(p, line, sizeof line)) != -1) {
        if (r == 1) { if (on_line) on_line(line, u); }
        else SDL_Delay(30);
    }
    int code = proc_exit_code(p);
    proc_free(p);
    return code;
}

static void on_dl(long long done, long long total) {
    char r[80]; dl_right(done, total, r, sizeof r);
    LOCK;
    snprintf(g_m.dl.right, sizeof g_m.dl.right, "%s", r);
    g_m.dl.frac = total > 0 ? (float)done / (float)total : 0.5f;
    UNLOCK;
}
static void on_apply_line(const char* l, void* u) { if (!strncmp(l, "FAILED ", 7)) snprintf((char*)u, 160, "%.150s", l + 7); }

static int do_update(const char* branch, const char* version) {
    char t[200], err[160] = "";
    step(&g_m.st.update, ST_RUNNING);
    snprintf(t, sizeof t, tr("dl_update"), version);
    bar(&g_m.dl, t, "", 0, 0);
    g_dl_t0 = 0;
    CreateDirectoryA("../update", NULL);
    if (!update_download(branch, "../update/download.zip", on_dl, err, sizeof err)) {
        char t2[200]; snprintf(t2, sizeof t2, tr("err_download"), err);
        bar(&g_m.dl, t2, "", g_m.dl.frac, 2); step(&g_m.st.update, ST_FAILED); return 0;
    }
    LOCK; g_m.dl.frac = 1; g_m.dl.state = 1; UNLOCK;
    bar(&g_m.in, tr("in_copy"), "", 0.05f, 0);
    if (!write_text("../update/launcher-apply.bat", APPLY)) { bar(&g_m.in, "couldn't write to the update folder", "", 0, 2); step(&g_m.st.update, ST_FAILED); return 0; }
    const char* argv[] = {"cmd.exe", "/c", "update\\launcher-apply.bat", NULL};
    err[0] = 0;
    int code = run(argv, NULL, "..", on_apply_line, err);
    if (code != 0) {
        bar(&g_m.in, err[0] ? err : "the update could not be put in place", "", 0.05f, 2);
        step(&g_m.st.update, ST_FAILED);
        return 0;
    }
    write_text("../update/rollback.bat", update_rollback_bat());
    step(&g_m.st.update, ST_DONE);
    return 1;
}

typedef struct { char failed[200]; int steps; } BuildRun;
static void on_build_line(const char* l, void* u) {
    BuildRun* b = u;
    float f; char title[160];
    int k = parse_build_line(l, &f, title, sizeof title);
    if (k == 1) {
        char t[200], r[16];
        snprintf(t, sizeof t, tr("in_build"), tr(title));     /* the step's title, translated when known */
        snprintf(r, sizeof r, "%d%%", (int)(f * 100));
        bar(&g_m.in, t, r, f, 0);
    } else if (k == 2) snprintf(b->failed, sizeof b->failed, "%s", title);
}
static int do_build(void) {
    step(&g_m.st.build, ST_RUNNING);
    wchar_t wrom[MAX_PATH]; char rom[MAX_PATH * 3];
    if (!GetFullPathNameW(L"data\\dqix.nds", MAX_PATH, wrom, NULL) || !WideCharToMultiByte(CP_UTF8, 0, wrom, -1, rom, sizeof rom, NULL, NULL) || !exists("data/dqix.nds")) {
        bar(&g_m.in, "Your ROM copy (game\\data\\dqix.nds) is missing: run the install again", "", 0, 2);
        step(&g_m.st.build, ST_FAILED);
        return 0;
    }
    char t[200]; snprintf(t, sizeof t, tr("in_build"), "...");
    bar(&g_m.in, t, "", 0.01f, 0);
    BuildRun b; memset(&b, 0, sizeof b);
    const char* argv[] = {"cmd.exe", "/c", "BUILD.bat", "--prepare", rom, NULL};
    const char* env[] = {"DQIX_NOPAUSE=1", NULL};
    int code = run(argv, env, "..", on_build_line, &b);
    if (code != 0) {
        bar(&g_m.in, b.failed[0] ? b.failed : "The build stopped: build\\build.log says why", "", g_m.in.frac, 2);
        step(&g_m.st.build, ST_FAILED);
        return 0;
    }
    remove("../update/download.zip");
    if (exists("dqix-launcher.old")) SDL_AtomicSet(&g_restart, 1);   /* a new launcher was put in place */
    step(&g_m.st.build, ST_DONE);
    return 1;
}

static int read_status(PackStatus* s) {
    char line[256];
    FILE* f = fopen("hd-pack-status.txt", "rb");
    if (!f) return 0;
    int ok = fgets(line, sizeof line, f) && parse_pack_status(line, s);
    fclose(f);
    return ok;
}
static void show_pack_status(const PackStatus* s) {
    char t[200], r[80];
    if (s->kind == 'd') {
        snprintf(t, sizeof t, tr("dl_pack"), s->i, s->n);
        dl_right(s->done, s->total, r, sizeof r);
        bar(&g_m.dl, t, r, s->total > 0 ? (float)s->done / (float)s->total : 0, 0);
    } else if (s->kind == 'i') {
        LOCK; g_m.dl.frac = 1; g_m.dl.state = 1; snprintf(g_m.dl.text, sizeof g_m.dl.text, "%s", tr("dl_done")); g_m.dl.right[0] = 0; UNLOCK;
        snprintf(t, sizeof t, tr("in_pack"), (int)s->done, (int)s->total);
        bar(&g_m.in, t, "", s->total > 0 ? (float)s->done / (float)s->total : 0, 0);
    }
}
static int do_pack(void) {
    if (!exists("hd-pack.exe")) { bar(&g_m.dl, "hd-pack.exe is missing: build the game again", "", 0, 2); step(&g_m.st.pack, ST_FAILED); return 0; }
    step(&g_m.st.pack, ST_RUNNING);
    remove("hd-pack-status.txt");
    g_dl_t0 = 0;
    const char* argv[] = {"hd-pack.exe", "--foreground", NULL};
    LProc* p = proc_start(argv, NULL, NULL);
    if (!p) { bar(&g_m.dl, "hd-pack.exe didn't start", "", 0, 2); step(&g_m.st.pack, ST_FAILED); return 0; }
    char line[512]; int r; Uint64 last = 0;
    PackStatus s; memset(&s, 0, sizeof s);
    while ((r = proc_read_line(p, line, sizeof line)) != -1) {
        if (r == 1) continue;
        if (SDL_GetTicks64() - last > 250) { last = SDL_GetTicks64(); if (read_status(&s)) show_pack_status(&s); }
        SDL_Delay(30);
    }
    int code = proc_exit_code(p);
    proc_free(p);
    int have = read_status(&s);
    if (code == 0) { step(&g_m.st.pack, ST_DONE); return 1; }
    const char* why = code == 2 ? "hd-pack.exe is already at work (the game open?)" : have && s.kind == 'f' && s.reason[0] ? s.reason : "The HD pack couldn't be installed: hd-pack.log says why";
    char t[200]; snprintf(t, sizeof t, "HD pack: %s", why);
    if (have && s.kind == 'i') bar(&g_m.in, t, "", g_m.in.frac, 2); else bar(&g_m.dl, t, "", g_m.dl.frac, 2);
    step(&g_m.st.pack, ST_FAILED);
    return 0;
}
#endif

static int work_thread(void* arg) {
    (void)arg;
    LOCK;
    scan_local(&g_m);
    g_m.st.update = g_m.st.build = g_m.st.pack = ST_IDLE;
    g_m.dl.state = g_m.in.state = 0; g_m.dl.frac = g_m.in.frac = 0; g_m.dl.right[0] = g_m.in.right[0] = 0;
    snprintf(g_m.dl.text, sizeof g_m.dl.text, "%s", tr("dl_waiting"));
    snprintf(g_m.in.text, sizeof g_m.in.text, "%s", tr("dl_waiting"));
    UNLOCK;
    int ok = 1;
#ifdef _WIN32
    UpdateInfo u;
    update_check_now(&u);
    LState st; LOCK; st = g_m.st; UNLOCK;
    const char* branch = lstate_branch(st.installed);
    LOCK;
    g_m.st.online = u.ok ? 1 : 0;
    snprintf(g_m.st.available, sizeof g_m.st.available, "%s", u.ok ? (strcmp(branch, "main") ? u.stable : u.nightly) : "");
    st = g_m.st;
    UNLOCK;
    if (st.online) do_news();
    int updated = 0;
    if (st.online && st.update_check && st.available[0] && update_newer(st.available, st.installed)) {
        ok = updated = do_update(branch, st.available);
    } else step(&g_m.st.update, ST_SKIPPED);
    if (ok && (updated || !st.game_built)) ok = do_build();
    else if (ok) step(&g_m.st.build, ST_SKIPPED);
    LOCK; scan_local(&g_m); st = g_m.st; UNLOCK;
    if (ok && st.hd_pack_wanted && st.online) ok = do_pack();
    else if (ok) step(&g_m.st.pack, ST_SKIPPED);
#else
    int online = do_news();
    LOCK; g_m.st.online = online; g_m.st.update = g_m.st.build = g_m.st.pack = ST_SKIPPED; UNLOCK;
#endif
    LOCK;
    scan_local(&g_m);
    if (ok) {
        int on = g_m.st.online != 0;
        snprintf(g_m.dl.text, sizeof g_m.dl.text, "%s", tr(on ? "dl_done" : "st_offline"));
        g_m.dl.frac = 1; g_m.dl.state = on; g_m.dl.right[0] = 0;
        snprintf(g_m.in.text, sizeof g_m.in.text, "%s", tr(lstate_status_key(&g_m.st)));
        snprintf(g_m.in.right, sizeof g_m.in.right, "%s%s%s", g_m.st.installed, g_m.pack_version[0] ? " \xC2\xB7 " : "", g_m.pack_version);
        g_m.in.frac = 1; g_m.in.state = 1;
    }
    g_m.busy = 0;
    UNLOCK;
    SDL_AtomicSet(&g_done, 1);
    return 0;
}

void work_init(const char* notes_lang) {
    snprintf(g_notes_lang, sizeof g_notes_lang, "%s", notes_lang ? notes_lang : "");
    remove("dqix-launcher.old");                     /* left by the last build that replaced us */
    LOCK;
    g_m.st.online = -1;
    scan_local(&g_m);
    g_m.nnews = news_local("notes", g_notes_lang, g_m.news, 3);
    g_m.news_offline = 1;
    snprintf(g_m.dl.text, sizeof g_m.dl.text, "%s", tr("dl_waiting"));
    snprintf(g_m.in.text, sizeof g_m.in.text, "%s", tr("dl_waiting"));
    UNLOCK;
}

void work_start(int verify) {
    (void)verify;                                    /* every run checks everything again */
    if (!SDL_AtomicGet(&g_done)) return;
    if (g_thread) { SDL_WaitThread(g_thread, NULL); g_thread = NULL; }
    SDL_AtomicSet(&g_done, 0);
    LOCK; g_m.busy = 1; UNLOCK;
    g_thread = SDL_CreateThread(work_thread, "work", NULL);
    if (!g_thread) { SDL_AtomicSet(&g_done, 1); LOCK; g_m.busy = 0; UNLOCK; }
}

void work_poll(void) {
    if (!SDL_AtomicGet(&g_done) || !SDL_AtomicGet(&g_restart)) return;
    SDL_AtomicSet(&g_restart, 0);
#ifdef _WIN32
    STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    char cmd[] = "\"dqix-launcher.exe\"";
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q);
    }
#endif
}

void work_stop(void) {
    /* the tools still at work end with the launcher (lnch_proc.c's job); the thread with the process */
    if (g_thread && SDL_AtomicGet(&g_done)) { SDL_WaitThread(g_thread, NULL); g_thread = NULL; }
}

void work_news_language(const char* notes_lang) {
    snprintf(g_notes_lang, sizeof g_notes_lang, "%s", notes_lang ? notes_lang : "");
    LOCK;
    if (g_m.news_offline) g_m.nnews = news_local("notes", g_notes_lang, g_m.news, 3);
    else news_titles_from_notes(g_m.news, g_m.nnews);
    if (!g_m.busy) {                                 /* the finished bars' texts in the new language */
        int on = g_m.st.online != 0;
        if (g_m.dl.state == 1) snprintf(g_m.dl.text, sizeof g_m.dl.text, "%s", tr(on ? "dl_done" : "st_offline"));
        if (g_m.in.state == 1) snprintf(g_m.in.text, sizeof g_m.in.text, "%s", tr(lstate_status_key(&g_m.st)));
    }
    UNLOCK;
}

void work_settings_changed(void) {
    LOCK;
    settings_wants(&g_m.st);
    int need = g_m.st.platform_updates && g_m.st.hd_pack_wanted && !g_m.st.pack_complete && !g_m.busy;
    UNLOCK;
    if (need) work_start(0);                         /* back to Modern: the HD pack is wanted again */
}
