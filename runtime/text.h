/* Text drawn by the PC version (its windows, the session lobby): text.c. */
#pragma once
#include <stdint.h>

int text_ready(void);                      /* a font was found */
/* Draws UTF-8 text into a 0x00RRGGBB buffer of width w, height h. Returns the
 * width drawn. */
int text_draw(uint32_t* buf, int w, int h, int x, int y, float px, uint32_t color, const char* s);
/* Word-wrapped within max_w pixels; returns the y below the last line. */
int text_wrap(uint32_t* buf, int w, int h, int x, int y, int max_w, float px, uint32_t color, const char* s);
/* the font: ttf a .ttf (fonts/, the system's; "" or "original": the
 * system's sans), or (game_letters) the game's own letters, that font for
 * the characters the game hasn't; text_font_gen() changes with it */
void text_set_font(const char* ttf, int game_letters);
unsigned text_font_gen(void);
