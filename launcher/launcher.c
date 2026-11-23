/* The launcher (game/dqix-launcher): the game and its HD pack brought up to
 * date before playing, the news, Discord, the language and the options.
 * Spec: docs/superpowers/specs/2026-11-23-launcher-design.md; the work itself
 * (update, build, pack) is lnch_work.c, the drawing lnch_ui.c.
 *
 * Tests and pictures: DQIX_LAUNCHER_DEMO=download|build|ready|failed|options|options-classic
 * shows a fixed state (no network, nothing run); DQIX_LAUNCHER_SHOT=<file.png>
 * saves the window and quits. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED                 /* our own main(): no SDL2main to link */
#include <SDL.h>
#include "lnch.h"
#include "lnch_ui.h"
#include "lnch_work.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

#define DISCORD_URL "https://discord.gg/z9X9PH8AH4"
#define REPO_URL    "https://github.com/romusan495/Dragon-Quest-IX-Modern"
static const char* const LANGUAGE_KEY[6] = {"", "english", "french", "german", "italian", "spanish"};
static const char* const NOTES_LANG[6] = {"", "", "fr", "de", "it", "es"};

Model g_m;                               /* shared with lnch_work.c under g_m.lock */
static SDL_Window* g_win;
static SDL_Renderer* g_ren;
static SDL_Texture* g_back; static int g_back_w, g_back_h;
static int g_lang = 1, g_classic, g_fullscreen, g_volume = 100;
static int g_options, g_lang_menu, g_drag_volume;
static float S = 1, OX, OY;              /* logical 1100x674 -> pixels */

/* ---- settings.ini */
static void settings_read(void) {
    char* t; char v[64];
    ini_load("settings.ini", &t);
    g_lang = 1;
    if (ini_get(t, "menu_language", v, sizeof v)) for (int i = 1; i <= 5; i++) if (!strcmp(v, LANGUAGE_KEY[i])) g_lang = i;
    if (g_lang == 1 && ini_get(t, "language", v, sizeof v)) for (int i = 1; i <= 5; i++) if (!strcmp(v, LANGUAGE_KEY[i])) g_lang = i;
    g_classic = ini_get(t, "mode", v, sizeof v) && strcmp(v, "enhanced") != 0;
    g_fullscreen = ini_get(t, "fullscreen", v, sizeof v) && atoi(v) != 0;
    g_volume = ini_get(t, "volume", v, sizeof v) ? atoi(v) : 100;
    if (g_volume < 0 || g_volume > 100) g_volume = 100;
    free(t);
}
static void settings_set(const char* key, const char* value) {
    char* t;
    ini_load("settings.ini", &t);
    char* n = ini_set(t, key, value);
    if (n && !ini_save("settings.ini", n)) fprintf(stderr, "launcher: can't write settings.ini\n");
    free(n); free(t);
}
static void set_language(int l) {
    g_lang = l;
    tr_set_language(l);
    settings_set("language", LANGUAGE_KEY[l]);
    settings_set("menu_language", LANGUAGE_KEY[l]);
    work_news_language(NOTES_LANG[l]);
}

/* ---- hit areas, made again every frame */
enum { A_NONE, A_LANG, A_LANG1, A_LANG5 = A_LANG1 + 4, A_DISCORD, A_GITHUB, A_FOLDER, A_NEWS_ALL, A_VERIFY, A_OPTIONS,
       A_PLAY, A_RETRY, A_CLASSIC, A_MODERN, A_FULLSCREEN, A_VOLUME, A_KEYS_PAD, A_KEYS_KB, A_RESET, A_CLOSE_OPT };
typedef struct { float x, y, w, h; int id; } Hit;
static Hit g_hits[64]; static int g_nhits;
static int g_mx = -1, g_my = -1;
static void hit(float x, float y, float w, float h, int id) {
    if (g_nhits < 64) { Hit hh = {OX + x * S, OY + y * S, w * S, h * S, id}; g_hits[g_nhits++] = hh; }
}
static int hit_at(int x, int y) {
    for (int i = g_nhits - 1; i >= 0; i--)
        if (x >= g_hits[i].x && x < g_hits[i].x + g_hits[i].w && y >= g_hits[i].y && y < g_hits[i].y + g_hits[i].h) return g_hits[i].id;
    return A_NONE;
}
static int hover(float x, float y, float w, float h) {
    float px = (g_mx - OX) / S, py = (g_my - OY) / S;
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* drawing in logical units */
#define X(v) (OX + (v) * S)
#define Y(v) (OY + (v) * S)
static float text(int f, float px, float x, float y, Col c, const char* s, float maxw, int al) {
    return ui_text(f, px * S, X(x), Y(y), c, s, maxw > 0 ? maxw * S : 0, al) / S;
}
static void rrect(float x, float y, float w, float h, float r, Col a, Col b) { ui_round_rect(X(x), Y(y), w * S, h * S, r * S, a, b); }
static void rborder(float x, float y, float w, float h, float r, float t, Col c) { ui_round_border(X(x), Y(y), w * S, h * S, r * S, t * S, c); }
static void frame(float x, float y, float w, float h) {         /* the game's window look: blue, white edge */
    rrect(x - 2, y - 2, w + 4, h + 4, 14, col(0x0B1430, 230), col(0x0B1430, 230));
    rrect(x, y, w, h, 12, col(0x14275A, 215), col(0x0B1638, 238));
    rborder(x, y, w, h, 12, 2, col(0xC9D6FF, 255));
}
static void button(float x, float y, float w, float h, const char* label, int id, Col bg, Col edge, Col fg) {
    int hv = hover(x, y, w, h);
    rrect(x, y, w, h, 10, hv ? col(0x1A2B5E, 255) : bg, hv ? col(0x1A2B5E, 255) : bg);
    rborder(x, y, w, h, 10, 1, hv ? col(0xE8C46A, 255) : edge);
    text(FONT_BOLD, 13, x + w / 2, y + h / 2 - 9, fg, label, w - 12, 1);
    hit(x, y, w, h, id);
}

static const Col GOLD = {0xE8, 0xC4, 0x6A, 255}, INK = {0xEE, 0xF2, 0xFF, 255}, MUTE = {0x9F, 0xB0, 0xD8, 255},
                 OK = {0x9F, 0xF0, 0xB5, 255}, BAD = {0xFF, 0x9A, 0x8F, 255}, SKY = {0x5F, 0xB4, 0xFF, 255};

static void draw_news(const Model* m) {
    float x = 22, y = 76, w = 418;
    float h = 46 + (m->nnews ? m->nnews : 1) * 72;
    frame(x, y, w, h);
    text(FONT_TITLE, 15, x + 18, y + 14, GOLD, tr("news"), 0, 0);
    int hv = hover(x + w - 150, y + 8, 140, 30);
    text(FONT_BOLD, 12, x + w - 18, y + 17, hv ? INK : SKY, tr("nav_all"), 0, 2);
    hit(x + w - 150, y + 8, 140, 30, A_NEWS_ALL);
    ui_fill(X(x), Y(y + 45), w * S, S, col(0xFFFFFF, 34));
    const char* months = tr("month");
    for (int i = 0; i < m->nnews; i++) {
        const NewsItem* n = &m->news[i];
        float iy = y + 46 + i * 72;
        char day[3] = "", mon[8] = "";
        if (n->date[0]) {
            int mm = atoi(n->date + 5);
            memcpy(day, n->date + 8, 2); day[2] = 0;
            const char* p = months;
            for (int k = 1; k < mm && p; k++) { p = strchr(p, ' '); if (p) p++; }
            if (p) { int l = 0; while (p[l] && p[l] != ' ' && l < 7) { mon[l] = p[l]; l++; } mon[l] = 0; }
        }
        if (day[0]) {
            text(FONT_BOLD, 21, x + 39, iy + 12, INK, day, 0, 1);
            text(FONT_BOLD, 11, x + 39, iy + 40, MUTE, mon, 0, 1);
        }
        float tx = x + 76;
        float tw = text(FONT_BOLD, 10, 0, -100, GOLD, n->tag, 0, 0) + 12;      /* measured off screen */
        rrect(tx, iy + 15, tw, 17, 4, GOLD, GOLD);
        text(FONT_BOLD, 10, tx + tw / 2, iy + 16, col(0x0B1430, 255), n->tag, 0, 1);
        text(FONT_BOLD, 14, tx + tw + 8, iy + 13, INK, n->title, w - (tx - x) - tw - 26, 0);
        if (i + 1 < m->nnews) ui_fill(X(x), Y(iy + 71), w * S, S, col(0xFFFFFF, 20));
    }
    if (m->news_offline) text(FONT_TEXT, 11, x + 18, y + h + 6, MUTE, tr("offline_news"), 0, 0);
}

static void draw_bar(float x, float y, float w, const char* label, const Bar* b, int blue) {
    float lw = 150;
    Col tc = b->state == 1 ? OK : b->state == 2 ? BAD : INK;
    float rw = b->right[0] ? text(FONT_TEXT, 12.5f, 0, -100, MUTE, b->right, 0, 0) + 12 : 0;
    text(FONT_BOLD, 13.5f, x + lw, y, tc, b->text, w - lw - rw - (b->state == 2 ? 110 : 0), 0);
    if (rw) text(FONT_TEXT, 12.5f, x + w, y + 1, MUTE, b->right, 0, 2);
    float by = y + 24;
    text(FONT_BOLD, 11, x, by - 5, GOLD, label, lw - 8, 0);
    rrect(x + lw, by, w - lw, 9, 5, col(0x1A2547, 255), col(0x1A2547, 255));
    float f = b->frac < 0 ? 0 : b->frac > 1 ? 1 : b->frac;
    if (f > 0) {
        if (b->state == 2) rrect(x + lw, by, (w - lw) * f, 9, 5, col(0xC0453F, 255), col(0xC0453F, 255));
        else if (blue) { ui_hgrad(X(x + lw + 2), Y(by), ((w - lw) * f - 4) * S, 9 * S, col(0x2B7FD0, 255), col(0xCFE9FF, 255)); }
        else ui_hgrad(X(x + lw + 2), Y(by), ((w - lw) * f - 4) * S, 9 * S, col(0xB8892F, 255), col(0xFFF3C4, 255));
    }
    rborder(x + lw, by, w - lw, 9, 5, 1, col(0x2C3B6A, 255));
    if (b->state == 2) {                                 /* RETRY, right of the failed bar's text */
        float bw = 100;
        button(x + w - bw - rw, y - 4, bw, 22, tr("retry"), A_RETRY, col(0xFF9A8F, 255), col(0xFF9A8F, 255), col(0x0B1430, 255));
    }
}

static void draw_options(void) {
    float x = 22, y = 72, w = 1056, h = 400;
    frame(x, y, w, h);
    text(FONT_TITLE, 15, x + 18, y + 14, GOLD, tr("options_title"), 0, 0);
    text(FONT_TEXT, 12, x + w - 52, y + 17, MUTE, tr("options_note"), 0, 2);
    int hv = hover(x + w - 42, y + 8, 32, 30);
    text(FONT_BOLD, 18, x + w - 26, y + 9, hv ? GOLD : INK, "\xC3\x97", 0, 1);   /* × closes */
    hit(x + w - 42, y + 8, 32, 30, A_CLOSE_OPT);
    ui_fill(X(x), Y(y + 45), w * S, S, col(0xFFFFFF, 34));
    /* the two modes */
    for (int k = 0; k < 2; k++) {
        float cx = x + 18 + k * (w - 36) / 2 + (k ? 6 : 0), cw = (w - 36) / 2 - 6, cy = y + 60, ch = 96;
        int on = k == 0 ? g_classic : !g_classic;
        int hvv = hover(cx, cy, cw, ch);
        rrect(cx, cy, cw, ch, 10, on ? col(0x1A2A5E, 255) : col(0x0B1638, 255), on ? col(0x1A2A5E, 255) : col(0x0B1638, 255));
        rborder(cx, cy, cw, ch, 10, 2, on ? GOLD : hvv ? col(0x6A80C0, 255) : col(0x2F4277, 255));
        /* the screens' picture: two DS screens, or one wide */
        if (k == 0) { rborder(cx + 20, cy + 22, 44, 24, 2, 1, col(0x9FD0FF, 255)); rborder(cx + 20, cy + 50, 44, 24, 2, 1, col(0x9FD0FF, 255)); }
        else rborder(cx + 14, cy + 26, 56, 44, 2, 1, col(0x9FD0FF, 255));
        text(FONT_TITLE, 20, cx + 88, cy + 22, GOLD, tr(k == 0 ? "mode_classic" : "mode_modern"), 0, 0);
        text(FONT_TEXT, 13, cx + 88, cy + 54, MUTE, tr(k == 0 ? "mode_classic_help" : "mode_modern_help"), cw - 100, 0);
        hit(cx, cy, cw, ch, k == 0 ? A_CLASSIC : A_MODERN);
    }
    /* below: window, volume, keys */
    float ry = y + 176, colw = (w - 36 - 30) / 2;
    for (int i = 0; i < 4; i++) {
        float rx = x + 18 + (i % 2) * (colw + 30), yy = ry + (i / 2) * 46;
        ui_fill(X(rx), Y(yy + 40), colw * S, S, col(0xFFFFFF, 16));
        if (i == 0) {
            text(FONT_TEXT, 14, rx, yy + 10, INK, tr("window"), 0, 0);
            char v[64]; snprintf(v, sizeof v, "\xE2\x80\xB9  %s  \xE2\x80\xBA", tr(g_fullscreen ? "fullscreen" : "windowed"));
            text(FONT_BOLD, 14, rx + colw, yy + 10, GOLD, v, 0, 2);
            hit(rx + colw - 220, yy, 220, 40, A_FULLSCREEN);
        } else if (i == 1) {
            text(FONT_TEXT, 14, rx, yy + 10, INK, tr("volume"), 0, 0);
            float sx = rx + colw - 220, sw = 170;
            rrect(sx, yy + 17, sw, 6, 3, col(0x38446B, 255), col(0x38446B, 255));
            rrect(sx, yy + 17, sw * g_volume / 100, 6, 3, GOLD, GOLD);
            rrect(sx + sw * g_volume / 100 - 7, yy + 13, 14, 14, 7, INK, INK);
            char v[8]; snprintf(v, sizeof v, "%d", g_volume);
            text(FONT_BOLD, 13, rx + colw, yy + 11, GOLD, v, 0, 2);
            hit(sx - 8, yy + 5, sw + 16, 30, A_VOLUME);
        } else {
            text(FONT_TEXT, 14, rx, yy + 10, INK, tr(i == 2 ? "keys_pad" : "keys_keyboard"), 0, 0);
            int h2 = hover(rx + colw - 220, yy, 220, 40);
            text(FONT_BOLD, 14, rx + colw, yy + 10, h2 ? INK : GOLD, tr("configure"), 0, 2);
            hit(rx + colw - 220, yy, 220, 40, i == 2 ? A_KEYS_PAD : A_KEYS_KB);
        }
    }
    ui_fill(X(x), Y(y + h - 46), w * S, S, col(0xFFFFFF, 24));
    text(FONT_TEXT, 12, x + 18, y + h - 30, MUTE, tr("mode_note"), w - 260, 0);
    button(x + w - 218, y + h - 38, 200, 30, tr("reset_defaults"), A_RESET, col(0x0E1A3D, 255), col(0x3A4F86, 255), INK);
}

static void draw(int W, int H) {
    S = (float)W / 1100 < (float)H / 674 ? (float)W / 1100 : (float)H / 674;
    OX = (W - 1100 * S) / 2; OY = (H - 674 * S) / 2;
    g_nhits = 0;
    SDL_SetRenderDrawColor(g_ren, 5, 9, 22, 255);
    SDL_RenderClear(g_ren);
    SDL_LockMutex(g_m.lock);
    Model m = g_m;                                       /* a copy: the work thread goes on meanwhile */
    m.lock = NULL;
    SDL_UnlockMutex(g_m.lock);
    if (!g_back && m.background_ready) g_back = ui_load_picture("textures/hd-pack/launcher-back.jpg", &g_back_w, &g_back_h);
    /* the background: the HD picture, or the night sky; never a tint over the picture */
    if (g_back) {
        ui_picture_cover(g_back, g_back_w, g_back_h, 0, 0, (float)W, (float)H);
        ui_hgrad(0, 0, W * 0.7f, (float)H, col(0x0B1430, 204), col(0x0B1430, 0));
    } else ui_stars(0, 0, (float)W, (float)H);
    ui_vgrad(0, 0, (float)W, 60 * S + OY, col(0x000000, 130), col(0x000000, 0));
    /* top */
    text(FONT_TITLE, 24, 22, 12, GOLD, "DRAGON QUEST IX", 0, 0);
    text(FONT_BOLD, 10, 24, 42, col(0xCFE3FF, 255), "M  O  D  E  R  N", 0, 0);
    float rx = 1078;
    rx -= 40; button(rx, 14, 40, 34, "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2", A_FOLDER, col(0x0B1430, 170), col(0x2F4277, 255), INK);
    rx -= 90; button(rx, 14, 82, 34, "GitHub", A_GITHUB, col(0x0B1430, 170), col(0x2F4277, 255), INK);
    rx -= 102; button(rx, 14, 94, 34, "Discord", A_DISCORD, col(0x3B4BB8, 255), col(0x7E8CFF, 255), INK);
    char ln[48]; snprintf(ln, sizeof ln, "%s \xE2\x96\xBE", tr("lang_name"));
    rx -= 126; button(rx, 14, 118, 34, ln, A_LANG, col(0x0B1430, 170), col(0x2F4277, 255), INK);
    /* middle */
    if (g_options) draw_options(); else draw_news(&m);
    /* bottom */
    float by = 492;
    ui_vgrad(X(0) - OX, Y(by), (float)W, (674 - by) * S + OY + 1, col(0x070D22, 242), col(0x050916, 255));
    ui_fill(0, Y(by), (float)W, S, col(0xE8C46A, 85));
    button(22, by + 40, 165, 42, tr("verify"), A_VERIFY, col(0x0E1A3D, 255), col(0x3A4F86, 255), col(0xDFE7FF, 255));
    button(22, by + 96, 165, 42, tr("options"), A_OPTIONS, col(0x0E1A3D, 255), g_options ? GOLD : col(0x3A4F86, 255), col(0xDFE7FF, 255));
    float mx = 210, mw = 1100 - 22 - 220 - 24 - mx;
    draw_bar(mx, by + 46, mw, tr("download"), &m.dl, 0);
    draw_bar(mx, by + 104, mw, tr("install"), &m.in, 1);
    int can = lstate_can_play(&m.st) && !m.busy;
    float px = 1100 - 22 - 220, py = by + 54;
    if (can) {
        int hv = hover(px, py, 220, 74);
        rrect(px - 3, py - 3, 226, 80, 16, col(0xE8C46A, hv ? 110 : 70), col(0xE8C46A, hv ? 110 : 70));
        rrect(px, py, 220, 74, 14, col(0xFFE9A0, 255), col(0xB8892F, 255));
        rborder(px, py, 220, 74, 14, 2, col(0xFFF3C4, 255));
        text(FONT_TITLE, 30, px + 110, py + 18, col(0x1B1204, 255), tr("play"), 200, 1);
        hit(px, py, 220, 74, A_PLAY);
    } else {
        rrect(px, py, 220, 74, 14, col(0x141C36, 255), col(0x141C36, 255));
        rborder(px, py, 220, 74, 14, 2, col(0x2A3558, 255));
        text(FONT_TITLE, 30, px + 110, py + 18, col(0x6B7699, 255), tr("play"), 200, 1);
    }
    char ver[96]; snprintf(ver, sizeof ver, "%s%s", m.st.installed[0] ? m.st.installed : "", m.pack_version[0] ? "  \xC2\xB7  " : "");
    if (m.pack_version[0]) strncat(ver, m.pack_version, sizeof ver - strlen(ver) - 1);
    text(FONT_TEXT, 11, 1090, 658, col(0x5C6A92, 255), ver, 0, 2);
    /* the language list, over everything */
    if (g_lang_menu) {
        float lx = 1078 - 40 - 90 - 102 - 126, ly = 52;
        rrect(lx, ly, 118, 5 * 32 + 8, 10, col(0x0B1638, 250), col(0x0B1638, 250));
        rborder(lx, ly, 118, 5 * 32 + 8, 10, 1, col(0x3A4F86, 255));
        for (int i = 1; i <= 5; i++) {
            int keep = g_lang; tr_set_language(i);
            const char* name = tr("lang_name");
            tr_set_language(keep);
            float iy = ly + 4 + (i - 1) * 32;
            if (hover(lx, iy, 118, 32) || i == g_lang) rrect(lx + 4, iy, 110, 32, 6, col(0x1F3470, 255), col(0x1F3470, 255));
            text(FONT_BOLD, 13, lx + 14, iy + 7, i == g_lang ? GOLD : INK, name, 0, 0);
            hit(lx, iy, 118, 32, A_LANG1 + i - 1);
        }
    }
}

/* ---- actions */
static void open_url(const char* u) { if (SDL_OpenURL(u)) fprintf(stderr, "launcher: can't open %s\n", u); }
static void open_here(const char* rel) {
    char* base = SDL_GetBasePath();
    char u[2048];
    snprintf(u, sizeof u, "file:///%s%s", base ? base : "", rel);
    for (char* c = u; *c; c++) if (*c == '\\') *c = '/';
    SDL_free(base);
    open_url(u);
}

/* the game, then the launcher closes; DQIX_LAUNCHER tells dqix not to come back here */
static int start_game(void) {
#ifdef _WIN32
    SetEnvironmentVariableA("DQIX_LAUNCHER", "1");
    wchar_t exe[MAX_PATH + 16];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    wchar_t* s = wcsrchr(exe, L'\\');
    if (!s) return 0;
    wcscpy(s + 1, L"dqix.exe");
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    wchar_t cmd[MAX_PATH + 32]; swprintf(cmd, MAX_PATH + 32, L"\"%ls\"", exe);
    *s = 0;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, exe, &si, &pi)) return 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
#else
    setenv("DQIX_LAUNCHER", "1", 1);
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) { setsid(); execl("./dqix", "./dqix", (char*)NULL); _exit(127); }
    return 1;
#endif
}

static void action(int a) {
    if (a != A_LANG && g_lang_menu) { g_lang_menu = 0; if (a < A_LANG1 || a > A_LANG5) return; }
    char v[16];
    switch (a) {
    case A_LANG: g_lang_menu = !g_lang_menu; break;
    case A_DISCORD: open_url(DISCORD_URL); break;
    case A_GITHUB: open_url(REPO_URL); break;
    case A_NEWS_ALL: open_url(REPO_URL "/releases"); break;
    case A_FOLDER: open_here(""); break;
    case A_OPTIONS: g_options = !g_options; break;
    case A_CLOSE_OPT: g_options = 0; break;
    case A_VERIFY: case A_RETRY: work_start(1); break;
    case A_PLAY: if (start_game()) { SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q); } break;
    case A_CLASSIC: g_classic = 1; settings_set("mode", "classic"); work_settings_changed(); break;
    case A_MODERN: g_classic = 0; settings_set("mode", "enhanced"); work_settings_changed(); break;
    case A_FULLSCREEN: g_fullscreen = !g_fullscreen; settings_set("fullscreen", g_fullscreen ? "1" : "0"); break;
    case A_KEYS_PAD: case A_KEYS_KB: open_here("controls.ini"); break;
    case A_RESET:
        g_classic = 0; g_fullscreen = 0; g_volume = 100;
        settings_set("mode", "enhanced"); settings_set("fullscreen", "0"); settings_set("volume", "100");
        work_settings_changed();
        break;
    case A_VOLUME: g_drag_volume = 1; break;
    default:
        if (a >= A_LANG1 && a <= A_LANG5) set_language(a - A_LANG1 + 1);
    }
    (void)v;
}
static void volume_from_mouse(int mx) {
    float x = 22 + 18 + (1056 - 36 - 30) / 2 + 30, colw = (1056 - 36 - 30) / 2;
    float sx = x + colw - 220, sw = 170;
    int v = (int)(((mx - OX) / S - sx) / sw * 100 + 0.5f);
    g_volume = v < 0 ? 0 : v > 100 ? 100 : v;
}

static void demo(const char* d) {
    Model* m = &g_m;
    m->st.platform_updates = 1; m->st.online = 1; m->st.update_check = 1; m->st.hd_pack_wanted = 1;
    m->st.game_built = 1; m->st.pack_complete = 1; strcpy(m->st.installed, "v3.2"); strcpy(m->st.available, "v3.2");
    m->st.update = m->st.build = m->st.pack = ST_DONE;
    strcpy(m->pack_version, "hd-pack-2");
    static const NewsItem N[3] = {{"v3.2", "2026-11-22", "Installing and updating work again"},
                                  {"v3.1", "2026-11-21", "Install fixes: the build tools checked first"},
                                  {"v3", "2026-11-20", "The HD pack: textures and movies downloaded"}};
    memcpy(m->news, N, sizeof N); m->nnews = 3;
    snprintf(m->dl.text, sizeof m->dl.text, "%s", tr("dl_done")); m->dl.frac = 1; m->dl.state = 1;
    snprintf(m->in.text, sizeof m->in.text, "%s", tr("st_ready")); m->in.frac = 1; m->in.state = 1;
    snprintf(m->in.right, sizeof m->in.right, "v3.2 \xC2\xB7 hd-pack-2");
    if (!strcmp(d, "download") || !strncmp(d, "options", 7)) {
        m->st.pack_complete = 0; m->st.pack = ST_RUNNING;
        snprintf(m->dl.text, sizeof m->dl.text, tr("dl_pack"), 2, 3); m->dl.frac = 0.44f; m->dl.state = 0;
        snprintf(m->dl.right, sizeof m->dl.right, "610 Mo / 1,4 Go \xC2\xB7 8,2 Mo/s \xC2\xB7 1 min 40");
        snprintf(m->in.text, sizeof m->in.text, tr("in_pack"), 4812, 4812); m->in.state = 0; m->in.right[0] = 0;
    } else if (!strcmp(d, "build")) {
        strcpy(m->st.available, "v3.3"); m->st.update = ST_DONE; m->st.build = ST_RUNNING;
        snprintf(m->dl.text, sizeof m->dl.text, tr("dl_update"), "v3.3"); snprintf(m->dl.right, sizeof m->dl.right, "12 Mo");
        snprintf(m->in.text, sizeof m->in.text, tr("in_build"), "Compiling (the first time takes a while)"); m->in.frac = 0.38f; m->in.state = 0;
        snprintf(m->in.right, sizeof m->in.right, "4/9");
    } else if (!strcmp(d, "failed")) {
        m->st.pack_complete = 0; m->st.pack = ST_FAILED;
        snprintf(m->dl.text, sizeof m->dl.text, "GitHub didn't answer (are you online?)"); m->dl.frac = 0.44f; m->dl.state = 2; m->dl.right[0] = 0;
        snprintf(m->in.text, sizeof m->in.text, "%s", tr("dl_waiting")); m->in.frac = 0; m->in.state = 0; m->in.right[0] = 0;
    }
    if (!strncmp(d, "options", 7)) { g_options = 1; if (!strcmp(d, "options-classic")) g_classic = 1; }
}

static void to_own_folder(void) {
    char* b = SDL_GetBasePath();
    if (b && chdir(b) != 0) fprintf(stderr, "launcher: can't go to %s\n", b);
    SDL_free(b);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "launcher: SDL: %s\n", SDL_GetError()); return 1; }
    to_own_folder();
    settings_read();
    tr_set_language(g_lang);
    g_m.lock = SDL_CreateMutex();
    const char* d = getenv("DQIX_LAUNCHER_DEMO");
    const char* shot = getenv("DQIX_LAUNCHER_SHOT");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    g_win = SDL_CreateWindow("Dragon Quest IX Modern", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1100, 674,
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (shot ? SDL_WINDOW_HIDDEN : 0));
    if (!g_win) { fprintf(stderr, "launcher: window: %s\n", SDL_GetError()); return 1; }
    SDL_SetWindowMinimumSize(g_win, 960, 588);
    g_ren = SDL_CreateRenderer(g_win, -1, shot ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_ren || !ui_init(g_ren)) { fprintf(stderr, "launcher: can't draw (fonts/Marcellus.ttf and fonts/Nunito.ttf needed)\n"); return 1; }
    if (d) { demo(d); g_m.background_ready = 1; }
    else { work_init(NOTES_LANG[g_lang]); work_start(0); }
    int W = 1100, H = 674, frames = 0, run = 1;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = 0;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) ui_forget_fonts();
            else if (e.type == SDL_MOUSEMOTION) { g_mx = e.motion.x; g_my = e.motion.y; if (g_drag_volume) volume_from_mouse(g_mx); }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int a = hit_at(e.button.x, e.button.y);
                if (a == A_NONE) g_lang_menu = 0; else action(a);
                if (a == A_VOLUME) volume_from_mouse(e.button.x);
            } else if (e.type == SDL_MOUSEBUTTONUP && g_drag_volume) {
                char v[8]; snprintf(v, sizeof v, "%d", g_volume);
                settings_set("volume", v); g_drag_volume = 0;
            } else if (e.type == SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(e.cdevice.which);
            else if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) {
                int k = e.type == SDL_KEYDOWN ? e.key.keysym.sym : -1, b = e.type == SDL_CONTROLLERBUTTONDOWN ? e.cbutton.button : -1;
                if (k == SDLK_ESCAPE || b == SDL_CONTROLLER_BUTTON_B) { g_options = 0; g_lang_menu = 0; }
                else if (k == SDLK_RETURN || b == SDL_CONTROLLER_BUTTON_A || b == SDL_CONTROLLER_BUTTON_START) {
                    SDL_LockMutex(g_m.lock); int can = lstate_can_play(&g_m.st) && !g_m.busy; SDL_UnlockMutex(g_m.lock);
                    if (can) action(A_PLAY);
                } else if (b == SDL_CONTROLLER_BUTTON_Y) g_options = !g_options;
            }
        }
        if (!d) work_poll();
        SDL_GetRendererOutputSize(g_ren, &W, &H);
        draw(W, H);
        if (shot && ++frames == 2) {
            if (!ui_save_png(shot, W, H)) { fprintf(stderr, "launcher: can't save %s\n", shot); return 1; }
            break;
        }
        SDL_RenderPresent(g_ren);
        if (shot) continue;
        SDL_Delay(16);
    }
    work_stop();
    SDL_Quit();
    return 0;
}
