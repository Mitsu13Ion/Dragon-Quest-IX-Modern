/* Tests of the launcher's logic without a window (launcher/lnch.h).
 *   sh tests/launcher/run.sh <temp folder>      (run.sh builds and runs this)
 * Writes only inside the temp folder given as argv[1]. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnch.h"

static int fails, runs;
static const char* g_tmp;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __func__, __LINE__, #c); fails++; return; } } while (0)
#define RUN(f) do { int before = fails; runs++; f(); printf("%s %s\n", fails == before ? "ok  " : "FAIL", #f); } while (0)

static void put(const char* rel, const char* text) {
    char p[1024]; snprintf(p, sizeof p, "%s/%s", g_tmp, rel);
    FILE* f = fopen(p, "wb"); if (!f) { printf("  can't write %s\n", p); exit(2); }
    fputs(text, f); fclose(f);
}

/* ---- settings.ini */
static void ini_set_keeps_comments_and_crlf(void) {
    char* t = ini_set("# a\r\nmode = classic\r\nvolume = 70\r\n", "mode", "enhanced");
    CHECK(t && !strcmp(t, "# a\r\nmode = enhanced\r\nvolume = 70\r\n"));
    free(t);
}
static void ini_set_appends_missing_key(void) {
    char* t = ini_set("volume = 70\n", "mode", "classic");
    CHECK(t && !strcmp(t, "volume = 70\nmode = classic\n"));
    free(t);
    t = ini_set("volume = 70", "mode", "classic");          /* no line end at the end */
    CHECK(t && !strcmp(t, "volume = 70\nmode = classic\n"));
    free(t);
    t = ini_set("", "mode", "classic");
    CHECK(t && !strcmp(t, "mode = classic\n"));
    free(t);
}
static void ini_set_does_not_touch_longer_key(void) {
    char* t = ini_set("modern = 1\nmode = classic\n", "mode", "enhanced");
    CHECK(t && !strcmp(t, "modern = 1\nmode = enhanced\n"));
    free(t);
}
static void ini_get_first_of_duplicates(void) {
    char v[32];
    CHECK(ini_get("mode = classic\nmode = enhanced\n", "mode", v, sizeof v) == 1);
    CHECK(!strcmp(v, "classic"));
    CHECK(ini_get("  volume=55  \r\n", "volume", v, sizeof v) == 1 && !strcmp(v, "55"));
    CHECK(ini_get("# mode = x\n", "mode", v, sizeof v) == 0);
}
static void ini_ignores_line_without_equals(void) {
    char v[32];
    CHECK(ini_get("mode\nmode = classic\n", "mode", v, sizeof v) == 1 && !strcmp(v, "classic"));
    char* t = ini_set("mode\nvolume = 1\n", "mode", "enhanced");
    CHECK(t && !strcmp(t, "mode\nvolume = 1\nmode = enhanced\n"));
    free(t);
}
static void ini_load_missing_is_empty(void) {
    char p[1024]; snprintf(p, sizeof p, "%s/nothing-here.ini", g_tmp);
    char* t = NULL;
    CHECK(ini_load(p, &t) == 0 && t && t[0] == 0);
    free(t);
}
static void ini_save_then_load(void) {
    char p[1024]; snprintf(p, sizeof p, "%s/settings.ini", g_tmp);
    CHECK(ini_save(p, "a = 1\r\n") == 1);
    char* t = NULL;
    CHECK(ini_load(p, &t) == 1 && !strcmp(t, "a = 1\r\n"));
    free(t);
}

/* ---- state */
static LState ready(void) {
    LState s; memset(&s, 0, sizeof s);
    s.platform_updates = 1; s.online = 1; s.update_check = 1; s.hd_pack_wanted = 1;
    s.game_built = 1; s.pack_complete = 1;
    strcpy(s.installed, "v3.2"); strcpy(s.available, "v3.2");
    s.update = s.build = s.pack = ST_DONE;
    return s;
}
static void state_ready_can_play(void) {
    LState s = ready();
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_ready"));
}
static void state_offline_complete_can_play(void) {
    LState s = ready();
    s.online = 0; s.available[0] = 0; s.update = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_offline"));
}
static void state_offline_pack_incomplete_plays_without_it(void) {
    LState s = ready();
    s.online = 0; s.pack_complete = 0; s.pack = ST_SKIPPED; s.update = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_offline"));
}
static void state_pack_incomplete_online_cannot_play(void) {
    LState s = ready();
    s.pack_complete = 0; s.pack = ST_RUNNING;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_pack"));
}
static void state_pack_not_wanted_can_play(void) {
    LState s = ready();
    s.hd_pack_wanted = 0; s.pack_complete = 0; s.pack = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
}
static void state_classic_does_not_wait_pack(void) {
    LState s = ready();                    /* classic: the caller sets hd_pack_wanted 0 */
    s.hd_pack_wanted = 0; s.pack_complete = 0; s.pack = ST_IDLE;
    CHECK(lstate_can_play(&s));
}
static void state_failed_cannot_play(void) {
    LState s = ready();
    s.build = ST_FAILED;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_failed"));
}
static void state_resume_after_kill(void) {
    LState s = ready();                    /* closed during the pack's download: started again */
    s.pack_complete = 0; s.pack = ST_IDLE;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_pack"));
}
static void state_update_pending_cannot_play(void) {
    LState s = ready();
    strcpy(s.available, "v3.3"); s.update = ST_IDLE;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_updating"));
    s.update_check = 0;                    /* the player turned updates off */
    CHECK(lstate_can_play(&s));
}
static void state_not_built_cannot_play(void) {
    LState s = ready();
    s.game_built = 0; s.build = ST_RUNNING;
    CHECK(!lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_building"));
}
static void state_linux_no_updates(void) {
    LState s = ready();
    s.platform_updates = 0; s.pack_complete = 0; s.update = s.pack = ST_SKIPPED;
    CHECK(lstate_can_play(&s));
    CHECK(!strcmp(lstate_status_key(&s), "st_no_updates_here"));
}
static void branch_nightly_stays_main(void) {
    CHECK(!strcmp(lstate_branch("v3.3-nightly2"), "main"));
    CHECK(!strcmp(lstate_branch("v3.2"), "stable"));
    CHECK(!strcmp(lstate_branch(""), "stable"));
}

/* ---- news */
static void news_github_three_latest(void) {
    const char* json =
        "[{\"tag_name\":\"v3.3\",\"name\":\"draft\",\"draft\":true,\"prerelease\":false,\"published_at\":null},"
        "{\"tag_name\":\"v3.2\",\"name\":\"v3.2 : install fixed \\\"again\\\"\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-11-22T10:00:00Z\",\"body\":\"x\"},"
        "{\"tag_name\":\"v3.2-rc1\",\"name\":\"rc\",\"draft\":false,\"prerelease\":true,\"published_at\":\"2026-11-22T09:00:00Z\"},"
        "{\"tag_name\":\"v3.1\",\"name\":\"v3.1\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-11-21T10:00:00Z\"},"
        "{\"tag_name\":\"v3\",\"name\":\"v3 : HD pack\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-11-20T10:00:00Z\"}]";
    NewsItem n[3];
    CHECK(news_from_github(json, n, 3) == 3);
    CHECK(!strcmp(n[0].tag, "v3.2") && !strcmp(n[0].date, "2026-11-22"));
    CHECK(!strcmp(n[0].title, "v3.2 : install fixed \"again\""));
    CHECK(!strcmp(n[1].tag, "v3.1") && !strcmp(n[2].tag, "v3"));
    CHECK(news_from_github("{\"message\":\"API rate limit exceeded\"}", n, 3) == 0);
    CHECK(news_from_github("", n, 3) == 0);
}
static void news_title_strips_prefix(void) {
    char t[160];
    CHECK(news_title_from_notes("\n\nDragon Quest IX Modern v3.2: installing works again.\n\nmore", t, sizeof t) == 1);
    CHECK(!strcmp(t, "installing works again."));
    CHECK(news_title_from_notes("Dragon Quest IX Modern v3.2 : l'installation remarche.\r\n", t, sizeof t) == 1);
    CHECK(!strcmp(t, "l'installation remarche."));
    CHECK(news_title_from_notes("Just a line\n", t, sizeof t) == 1 && !strcmp(t, "Just a line"));
    CHECK(news_title_from_notes("   \n", t, sizeof t) == 0);
}
static void news_local_sorted_newest_first(void) {
    char d[1024]; snprintf(d, sizeof d, "%s/notes", g_tmp);
#ifdef _WIN32
    char cmd[1100]; snprintf(cmd, sizeof cmd, "mkdir \"%s\\fr\" 2>nul", d); system(cmd);
#else
    char cmd[1100]; snprintf(cmd, sizeof cmd, "mkdir -p '%s/fr'", d); if (system(cmd)) {}
#endif
    put("notes/v3.md", "Dragon Quest IX Modern v3: the HD pack.\n");
    put("notes/v3.1.md", "Dragon Quest IX Modern v3.1: fixes.\n");
    put("notes/v3.2.md", "Dragon Quest IX Modern v3.2: install again.\n");
    put("notes/fr/v3.2.md", "Dragon Quest IX Modern v3.2 : l'installation remarche.\n");
    put("notes/nightly.md", "not a version\n");
    NewsItem n[3];
    CHECK(news_local(d, "", n, 3) == 3);
    CHECK(!strcmp(n[0].tag, "v3.2") && !strcmp(n[1].tag, "v3.1") && !strcmp(n[2].tag, "v3"));
    CHECK(!strcmp(n[0].title, "install again.") && n[0].date[0] == 0);
    CHECK(news_local(d, "fr", n, 3) == 3);             /* the language's notes, English where it has none */
    CHECK(!strcmp(n[0].title, "l'installation remarche.") && !strcmp(n[1].title, "fixes."));
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: t_launcher <temp folder>\n"); return 2; }
    g_tmp = argv[1];
    RUN(ini_set_keeps_comments_and_crlf); RUN(ini_set_appends_missing_key); RUN(ini_set_does_not_touch_longer_key);
    RUN(ini_get_first_of_duplicates); RUN(ini_ignores_line_without_equals); RUN(ini_load_missing_is_empty); RUN(ini_save_then_load);
    RUN(state_ready_can_play); RUN(state_offline_complete_can_play); RUN(state_offline_pack_incomplete_plays_without_it);
    RUN(state_pack_incomplete_online_cannot_play); RUN(state_pack_not_wanted_can_play); RUN(state_classic_does_not_wait_pack);
    RUN(state_failed_cannot_play); RUN(state_resume_after_kill); RUN(state_update_pending_cannot_play);
    RUN(state_not_built_cannot_play); RUN(state_linux_no_updates); RUN(branch_nightly_stays_main);
    RUN(news_github_three_latest); RUN(news_title_strips_prefix); RUN(news_local_sorted_newest_first);
    printf("%d/%d passed\n", runs - fails, runs);
    return fails ? 1 : 0;
}
