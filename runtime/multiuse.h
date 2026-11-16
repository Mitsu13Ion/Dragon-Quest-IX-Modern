/* Use several at once: the item menu's "How many?" (multiuse.c). */
#pragma once
#include <stdint.h>

const uint8_t* multiuse_picture(int w, int h, int* changed);   /* its window over everything (premultiplied RGBA), NULL: none */
int multiuse_keys(uint16_t* keys, int field);   /* the game's keys this frame (field: on foot, not a battle): 1 if it took or changed them */
