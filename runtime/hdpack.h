/* The HD pack (FatalPixels textures, SeedVR2 movies): hd-pack.exe, next to
 * the game, downloads and installs it in the background (tools/hd-pack.c);
 * the game only starts it, shows its bar (hd-pack-progress.txt) and uses the
 * pack once it is there. */
#pragma once

/* the order textures/<top folder>/ is looked in, the first texture found
 * winning: 0 the player's own packs, 1 hd-pack (the downloaded pack), 2 hd
 * (made by textures-hd); any letter case */
int hdpack_dir_rank(const char* top_folder);

/* start hd-pack.exe (Windows, if it is next to the game and enabled):
 * detached, no window, idle priority; it does nothing if the pack is there */
void hdpack_start(int enabled);
