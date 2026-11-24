#ifndef SAVEFILE_H
#define SAVEFILE_H
#include <stddef.h>

/* Writes n bytes to path without ever leaving it empty or cut short: they go
 * to <path>.tmp, are flushed to the disk, then replace path in one step.
 * 0 on success; -1 on failure, path then unchanged (a whole <path>.tmp is
 * left behind if only the replace failed). */
int save_file_atomic(const char* path, const void* data, size_t n);

/* Game > Saves: slot 1 is the save given at launch (dqix.sav), slots 2..3
 * are saves/slot<N>.sav next to it. 0, or -1 if slot is out of range or the
 * path does not fit in n. */
#define SAVE_SLOTS 3
int save_slot_path(char* out, size_t n, const char* base, int slot);

#endif
