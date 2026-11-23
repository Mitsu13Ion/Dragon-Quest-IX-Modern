/* When PLAY lights up, and what the status line says: see lnch.h and the
 * spec (docs/superpowers/specs/2026-11-23-launcher-design.md). */
#include <string.h>
#include "lnch.h"

static int update_pending(const LState* s) {
    return s->platform_updates && s->online != 0 && s->update_check && s->available[0]
        && update_newer(s->available, s->installed) && s->update != ST_DONE;
}
/* the HD pack still has to come before playing (offline: play without it,
 * it is only ever installed whole) */
static int pack_pending(const LState* s) {
    return s->platform_updates && s->hd_pack_wanted && !s->pack_complete && s->online != 0;
}

int lstate_can_play(const LState* s) {
    if (s->update == ST_RUNNING || s->build == ST_RUNNING || s->pack == ST_RUNNING) return 0;
    if (s->update == ST_FAILED || s->build == ST_FAILED || s->pack == ST_FAILED) return 0;
    if (!s->game_built) return 0;
    return !update_pending(s) && !pack_pending(s);
}

const char* lstate_status_key(const LState* s) {
    if (s->update == ST_FAILED || s->build == ST_FAILED || s->pack == ST_FAILED) return "st_failed";
    if (s->update == ST_RUNNING || update_pending(s)) return "st_updating";
    if (s->build == ST_RUNNING || !s->game_built) return "st_building";
    if (s->pack == ST_RUNNING || pack_pending(s)) return "st_pack";
    if (!s->platform_updates) return "st_no_updates_here";
    if (s->online == 0) return "st_offline";
    return "st_ready";
}

const char* lstate_branch(const char* installed) {
    return strstr(installed, "nightly") ? "main" : "stable";
}
