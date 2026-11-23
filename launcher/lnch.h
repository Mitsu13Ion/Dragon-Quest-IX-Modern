/* The launcher (game/dqix-launcher): see docs/superpowers/specs/2026-11-23-launcher-design.md.
 * This header is the part without SDL: settings.ini, the state that decides
 * when PLAY lights up, the news, the tools it runs and the texts. */
#pragma once
#include <stddef.h>

/* ---- settings.ini: only the keys the launcher sets change, the rest of the
 * text (comments, line ends, order) stays byte for byte */
int   ini_get(const char* text, const char* key, char* out, size_t n);   /* 1 found (the first one) */
char* ini_set(const char* text, const char* key, const char* value);    /* new text (malloc): replaced in place, or added at the end */
int   ini_load(const char* path, char** text);                           /* 1 read; 0 missing: *text = "" (malloc) */
int   ini_save(const char* path, const char* text);                      /* path.tmp then renamed; 1 ok */

/* ---- what the screen shows and whether PLAY is lit */
typedef enum { ST_IDLE, ST_RUNNING, ST_DONE, ST_FAILED, ST_SKIPPED } StepState;
typedef struct {
    int platform_updates;     /* 1 on Windows (updater + hd-pack.exe), 0 elsewhere */
    int online;               /* -1 unknown, 0 offline, 1 online */
    int update_check;         /* settings.ini update_check (default 1) */
    int hd_pack_wanted;       /* hd_pack_download != 0 and mode = enhanced */
    int game_built;           /* game/dqix[.exe] and game/data/dqix.nds are there */
    int pack_complete;        /* textures/hd-pack/hd-pack.txt is there (the pack is all or nothing) */
    char installed[32], available[32];   /* the local version.h's, the remote one's ("" unknown) */
    StepState update, build, pack;
} LState;
int         lstate_can_play(const LState* s);
const char* lstate_status_key(const LState* s);    /* a text key (lnch_text.c) for the status line */
const char* lstate_branch(const char* installed);  /* "main" for a nightly, else "stable" */

/* ---- news */
typedef struct { char tag[24]; char date[11]; char title[160]; } NewsItem;  /* date "YYYY-MM-DD" or "" */
int news_from_github(const char* json, NewsItem* out, int max);   /* GitHub's releases list: tag_name, published_at, name; drafts and prereleases left out */
int news_title_from_notes(const char* md, char* out, size_t n);   /* first non-empty line, "Dragon Quest IX Modern vX: " taken off */
int news_local(const char* notes_dir, const char* lang, NewsItem* out, int max);  /* <dir>[/<lang>]/v*.md, newest first, date "" */

/* from runtime/update.c */
int update_newer(const char* a, const char* b);
