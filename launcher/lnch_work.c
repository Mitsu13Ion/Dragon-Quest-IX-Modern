/* See lnch_work.h. Paths are relative to the game folder (the launcher's
 * own, where main() goes first); the project is the folder above it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnch_work.h"

static char g_notes_lang[8];

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

void work_init(const char* notes_lang) {
    snprintf(g_notes_lang, sizeof g_notes_lang, "%s", notes_lang ? notes_lang : "");
    SDL_LockMutex(g_m.lock);
    g_m.st.online = -1;
    scan_local(&g_m);
    g_m.nnews = news_local("notes", g_notes_lang, g_m.news, 3);
    g_m.news_offline = 1;
    snprintf(g_m.dl.text, sizeof g_m.dl.text, "%s", tr("dl_waiting"));
    snprintf(g_m.in.text, sizeof g_m.in.text, "%s", tr(lstate_status_key(&g_m.st)));
    SDL_UnlockMutex(g_m.lock);
}

/* shortcut: the update, build and HD pack steps come with Task 6 of the plan */
void work_start(int verify) { (void)verify; }
void work_poll(void) {}
void work_stop(void) {}
void work_news_language(const char* notes_lang) {
    snprintf(g_notes_lang, sizeof g_notes_lang, "%s", notes_lang ? notes_lang : "");
    SDL_LockMutex(g_m.lock);
    if (g_m.news_offline) g_m.nnews = news_local("notes", g_notes_lang, g_m.news, 3);
    SDL_UnlockMutex(g_m.lock);
}
void work_settings_changed(void) {
    SDL_LockMutex(g_m.lock);
    settings_wants(&g_m.st);
    SDL_UnlockMutex(g_m.lock);
}
