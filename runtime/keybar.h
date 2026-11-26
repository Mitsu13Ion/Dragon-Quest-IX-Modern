/* A row of the player's keys with what they do, in a modern window (keybar.c):
 * "[C] Next  [Q] Previous  [V] Exit". */
#pragma once
#include <stdint.h>

typedef struct { int style, face; char key[32]; const char* what; } KeybarEntry;   /* key: hints.h's sign (style, face, its label); "": not shown */

/* its picture (premultiplied RGBA, pw x ph pixels) at u pixels per unit (a
 * 1920 x 1080 screen's pixel); wu, hu: its size in those units. Redrawn only
 * when the entries or u change (*changed). NULL: nothing to show. */
const uint8_t* keybar_picture(const KeybarEntry* e, int n, float u, int* pw, int* ph, float* wu, float* hu, int* changed);
