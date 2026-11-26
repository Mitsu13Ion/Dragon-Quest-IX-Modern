/* A row of the player's keys with what they do (keybar.h): each key drawn as
 * hints.c draws the game's buttons (a keyboard's key, a controller's button),
 * its words after it, in a window of the modern style (see-through dark, a
 * thin grey line). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "journal.h"
#include "text.h"
#include "hints.h"
#include "keybar.h"

#define KB_H 64          /* (units: a 1920 x 1080 screen's pixels) */
#define KEY_H 40
#define TEXT 26
#define PAD 18
#define GAP 30

static uint8_t* g_pic; static int g_pw, g_ph;
static KeybarEntry g_was[8]; static char g_was_what[8][64]; static int g_was_n = -1; static float g_was_u;

static void blend(uint8_t* dst, int dw, int dh, int X, int Y, const uint8_t* src, int w, int h) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int px = X + x, py = Y + y;
            if (px < 0 || py < 0 || px >= dw || py >= dh) continue;
            const uint8_t* s = &src[((size_t)y * w + x) * 4];
            if (!s[3]) continue;
            uint8_t* d = &dst[((size_t)py * dw + px) * 4];
            float k = 1 - s[3] / 255.0f;
            for (int i = 0; i < 4; i++) d[i] = (uint8_t)fminf(255, s[i] + d[i] * k);
        }
}
static float key_w(const KeybarEntry* e) { return KEY_H * (e->style == HINT_KEYBOARD ? fmaxf(1, hint_key_aspect(e->key)) : 1); }

const uint8_t* keybar_picture(const KeybarEntry* e, int n, float u, int* pw, int* ph, float* wu, float* hu, int* changed) {
    *changed = 0;
    if (n > 8) n = 8;
    float W = PAD;
    int shown = 0;
    for (int i = 0; i < n; i++) {
        if (!e[i].key[0]) continue;
        W += (shown ? GAP : 0) + key_w(&e[i]) + 10 + journal_text_width(TEXT, e[i].what);
        shown++;
    }
    if (!shown || u <= 0) return NULL;
    W += PAD;
    *wu = W; *hu = KB_H;
    static unsigned font;                                                /* (the font changed: drawn again) */
    int same = g_pic && n == g_was_n && u == g_was_u && font == text_font_gen();
    font = text_font_gen();
    for (int i = 0; same && i < n; i++)
        same = g_was[i].style == e[i].style && g_was[i].face == e[i].face && !strcmp(g_was[i].key, e[i].key) && !strcmp(g_was_what[i], e[i].what ? e[i].what : "");
    if (same) { *pw = g_pw; *ph = g_ph; return g_pic; }
    int w = (int)(W * u + 0.5f), h = (int)(KB_H * u + 0.5f);
    if (w != g_pw || h != g_ph || !g_pic) {
        free(g_pic); g_pic = calloc((size_t)w * h, 4);
        if (!g_pic) { g_pw = g_ph = 0; return NULL; }
        g_pw = w; g_ph = h;
    } else memset(g_pic, 0, (size_t)w * h * 4);
    g_was_n = n; g_was_u = u;
    for (int i = 0; i < n; i++) { g_was[i] = e[i]; snprintf(g_was_what[i], sizeof g_was_what[i], "%s", e[i].what ? e[i].what : ""); }
    *pw = w; *ph = h; *changed = 1;
    JournalCanvas c = { g_pic, w, h, u, 0, 0, 0, 0, (int)W, KB_H, 0 };
    int Wi = (int)ceilf(W);
    journal_box(&c, 0, 0, Wi, KB_H, 0x0E1014, 0.72f);
    journal_box(&c, 0, 0, Wi, 2, 0x8A8A8A, 0.95f); journal_box(&c, 0, KB_H - 2, Wi, 2, 0x8A8A8A, 0.95f);
    journal_box(&c, 0, 0, 2, KB_H, 0x8A8A8A, 0.95f); journal_box(&c, Wi - 2, 0, 2, KB_H, 0x8A8A8A, 0.95f);
    float x = PAD;
    for (int i = 0, k = 0; i < n; i++) {
        if (!e[i].key[0]) continue;
        if (k++) x += GAP;
        float kw = key_w(&e[i]);
        int iw = (int)lroundf(kw * u), ih = (int)lroundf(KEY_H * u);
        if (iw > 2 && ih > 2 && iw < 2000 && ih < 1000) {
            uint8_t* img = malloc((size_t)iw * ih * 4);
            if (img) { hint_draw(img, iw, ih, e[i].style, e[i].face, e[i].key); blend(g_pic, w, h, (int)lroundf(x * u), (int)lroundf((KB_H - KEY_H) / 2.0f * u), img, iw, ih); free(img); }
        }
        x += kw + 10;
        journal_text(&c, (int)lroundf(x), (KB_H - TEXT) / 2 - 2, TEXT, 0xF0F0F0, e[i].what);
        x += journal_text_width(TEXT, e[i].what);
    }
    return g_pic;
}
