/* The party's HP and MP, modern: a panel at the bottom left (partyhud.c). */
#pragma once
#include <stdint.h>

/* its picture (premultiplied RGBA, pw x ph pixels) at s pixels per DS pixel; open: the menu's
 * larger one. wu, hu: its size in its own units (4 to a DS pixel). NULL: no party to show. */
const uint8_t* partyhud_picture(int open, float s, int* pw, int* ph, int* wu, int* hu, int* changed);
