/* save_file_atomic (runtime/savefile.c): the save is written to <path>.tmp
 * and only then put in place, so a failed write never empties dqix.sav.
 * argv[1]: an empty folder to work in. Built and run by tests/savefile/run.sh. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif
#include "savefile.h"

static int pass, fail;
#define CHECK(name, cond) do { if (cond) { pass++; printf("PASS %s\n", name); } else { fail++; printf("FAIL %s\n", name); } } while (0)

#define SIZE (64 * 1024)
static unsigned char data[SIZE], got[SIZE + 1];

static void put(const char* path, const char* s) { FILE* f = fopen(path, "wb"); if (f) { fputs(s, f); fclose(f); } }
static size_t get(const char* path) { FILE* f = fopen(path, "rb"); if (!f) return (size_t)-1; size_t n = fread(got, 1, sizeof got, f); fclose(f); return n; }
static int exists(const char* path) { struct stat st; return stat(path, &st) == 0; }
static int holds_data(const char* path) { return get(path) == SIZE && !memcmp(got, data, SIZE); }
static int holds_old(const char* path) { return get(path) == 3 && !memcmp(got, "old", 3); }

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_savefile <empty folder>\n"); return 2; }
    for (int i = 0; i < SIZE; i++) data[i] = (unsigned char)(i * 7 + 1);
    char p[1024], tmp[1100];

    snprintf(p, sizeof p, "%s/new.sav", argv[1]); snprintf(tmp, sizeof tmp, "%s.tmp", p);
    CHECK("new file: returns 0", save_file_atomic(p, data, SIZE) == 0);
    CHECK("new file: content written", holds_data(p));
    CHECK("new file: no .tmp left", !exists(tmp));

    snprintf(p, sizeof p, "%s/replace.sav", argv[1]); snprintf(tmp, sizeof tmp, "%s.tmp", p);
    put(p, "old");
    CHECK("replace: returns 0", save_file_atomic(p, data, SIZE) == 0);
    CHECK("replace: new content in place", holds_data(p));
    CHECK("replace: no .tmp left", !exists(tmp));

    snprintf(p, sizeof p, "%s/stale.sav", argv[1]); snprintf(tmp, sizeof tmp, "%s.tmp", p);
    put(p, "old"); put(tmp, "junk from a crash");
    CHECK("stale tmp: returns 0", save_file_atomic(p, data, SIZE) == 0);
    CHECK("stale tmp: new content in place", holds_data(p));
    CHECK("stale tmp: no .tmp left", !exists(tmp));

    snprintf(p, sizeof p, "%s/blocked.sav", argv[1]); snprintf(tmp, sizeof tmp, "%s.tmp", p);
    put(p, "old"); MKDIR(tmp);
    CHECK("tmp cannot be made: returns -1", save_file_atomic(p, data, SIZE) == -1);
    CHECK("tmp cannot be made: old save untouched", holds_old(p));

    snprintf(p, sizeof p, "%s/isdir.sav", argv[1]); snprintf(tmp, sizeof tmp, "%s.tmp", p);
    MKDIR(p);
    CHECK("replace fails: returns -1", save_file_atomic(p, data, SIZE) == -1);
    CHECK("replace fails: whole .tmp kept", holds_data(tmp));

    snprintf(p, sizeof p, "%s/missing/x.sav", argv[1]);
    CHECK("folder missing: returns -1", save_file_atomic(p, data, SIZE) == -1);
    CHECK("folder missing: nothing created", !exists(p));

    /* save_slot_path: slot 1 is the given save, slots 2..3 in saves/ next to it */
    char o[64];
    CHECK("slot 1: the save itself", save_slot_path(o, sizeof o, "dqix.sav", 1) == 0 && !strcmp(o, "dqix.sav"));
    CHECK("slot 2: saves/ here", save_slot_path(o, sizeof o, "dqix.sav", 2) == 0 && !strcmp(o, "saves/slot2.sav"));
    CHECK("slot 3: next to a full path", save_slot_path(o, sizeof o, "/tmp/x/dqix.sav", 3) == 0 && !strcmp(o, "/tmp/x/saves/slot3.sav"));
    CHECK("slot 3: Windows path", save_slot_path(o, sizeof o, "C:\\g\\dqix.sav", 3) == 0 && !strcmp(o, "C:\\g\\saves/slot3.sav"));
    CHECK("slot 0 refused", save_slot_path(o, sizeof o, "dqix.sav", 0) == -1);
    CHECK("slot 4 refused", save_slot_path(o, sizeof o, "dqix.sav", 4) == -1);
    CHECK("too small refused", save_slot_path(o, 8, "dqix.sav", 2) == -1);

    printf("t_savefile: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
