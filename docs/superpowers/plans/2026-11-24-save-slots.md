# Save slots Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** up to 10 saves, switched from Game > Saves by restarting the game on the chosen one.

**Architecture:** slot 1 is the save named on the command line (`dqix.sav`); slots 2..10 are `saves/slot<N>.sav` next to it. `settings.ini` holds the active slot and the names. After `settings_load`, `frontend_init` reloads the save chip from the active slot's file; switching = `settings_save` + `restart_with(NULL)`.

**Tech Stack:** C (runtime), gcc/mingw+Wine for `tests/savefile`.

**Spec:** `docs/superpowers/specs/2026-11-24-save-slots-design.md`

## Global Constraints
- `dqix.sav` stays slot 1, same name, same place; missing `save_slot` = 1.
- Max 10 slots (`SAVE_SLOTS 10`).
- Never switch while the game is writing its save; always `backup_flush` before restarting (`restart_with` does).
- English texts in the code; fr/de/it/es in `lang/*.txt` by `dqix-langues`.
- Nothing runs on Romuald's PC: `tests/savefile/run.sh` and the game run in a cloud session.

## Review Focus
- A name with `=`, `#`, a newline or > 40 bytes typed in Rename: stored on one line, cut to 40 bytes, newlines removed (Task 3).
- `settings.ini` with `save_slot = 0`, `11` or garbage: falls back to slot 1 (Task 3).
- A slot file that exists but has no name (copied in by hand): shown as used, default name (Task 3).
- `saves/` folder cannot be made (read-only folder): the write fails like any save write (warned, retried), `dqix.sav` untouched (Task 2).
- Switching during a session: refused like Reset (Task 3).

---

### Task 1: slot file path

**Files:** Modify `runtime/savefile.h`, `runtime/savefile.c`; Test `tests/savefile/t_savefile.c`

**Interfaces — Produces:** `#define SAVE_SLOTS 10`; `int save_slot_path(char* out, size_t n, const char* base, int slot);` 0 = ok, -1 = slot out of 1..10 or `out` too small. Slot 1 → `base`; slot N → folder of `base` (up to and including its last `/` or `\`, or nothing) + `saves/slot<N>.sav`.

- [ ] Step 1: tests in `t_savefile.c`: `save_slot_path(o,sizeof o,"dqix.sav",1)` → `"dqix.sav"`; `(…,"dqix.sav",2)` → `"saves/slot2.sav"`; `(…,"/tmp/x/dqix.sav",10)` → `"/tmp/x/saves/slot10.sav"`; `(…,"C:\\g\\dqix.sav",3)` → `"C:\\g\\saves/slot3.sav"`; slot 0 and 11 → -1; `n = 8` with slot 2 → -1.
- [ ] Step 2: cloud: `sh tests/savefile/run.sh` → FAIL (undefined `save_slot_path`).
- [ ] Step 3: implement in `savefile.c` with `snprintf`.
- [ ] Step 4: cloud: `run.sh` → `all passed` (Linux and Wine).
- [ ] Step 5: commit `feat(save): slot file path`.

### Task 2: save chip loads a slot

**Files:** Modify `runtime/card.c`, `runtime/nds.h`

**Interfaces — Consumes:** `save_slot_path`. **Produces:**
`void backup_use_slot(int slot);` reloads the chip from the slot's file (path from the save given at launch), makes `saves/` for slots ≥ 2; no-op for slot 1.
`int backup_slot_path(int slot, char* out, size_t n);` same as `save_slot_path` with the launch save as base.
`int backup_busy(void);` 1 while the game wrote to the chip less than 30 frames ago and the file is not yet written.

- [ ] Step 1: `backup_init` can run twice: frees `g_backup_at_boot`, resets `g_backup_size` to 64 KB and `g_backup_existed` to 0; the first call keeps the path as the base (`g_backup_base`).
- [ ] Step 2: the three functions above.
- [ ] Step 3: commit `feat(save): load the save chip from a slot`.

### Task 3: settings, menu, switch

**Files:** Modify `runtime/menu.h` (ids `MI_SLOT0`..`+9`, `MI_SLOT_RENAME`, range from the coordinator), `runtime/menu_win32.c` (Game > Saves submenu after `MI_SAVEBAK`), `runtime/frontend_sdl.c`.

- [ ] Step 1: settings: `g_save_slot` (1..10, else 1), `g_save_name[11][41]`; keys `save_slot`, `save_name_<N>` read and written with a comment line; names cleaned of `\r\n`, cut to 40 bytes.
- [ ] Step 2: `frontend_init`: after `settings_load`, before the DLC unlock: `backup_use_slot(g_save_slot)`.
- [ ] Step 3: menu labels refreshed where `MI_SAVEBAK` is checked: `"%d. %s"` with the name or `T("Adventure %d")`, or `T("%d. (empty: a new adventure)")` when no file and no name; check on the active one; all enabled only when `!session_requested()`.
- [ ] Step 4: actions: other slot → refuse in a session (like Reset), refuse with "The game is saving: try again in a moment." if `backup_busy()`; empty slot asks a name (`ask_text`, NULL = default); confirm with `ask_yes("Saves", "Switch to <name>? The game restarts on that save.\n\nAnything since you last saved in the game is lost.", "Switch")`; then `g_save_slot = N`, `restart_with(NULL)` (it saves the settings). `MI_SLOT_RENAME`: `ask_text`, store, `settings_save`.
- [ ] Step 5: commit `feat(save): Game > Saves menu`.

### Task 4: translations
- [ ] `dqix-langues` adds fr/de/it/es for every new English text in `lang/*.txt`; commit `lang: save slots`.

### Task 5: cloud test
- [ ] Brief in the shared notes folder: build EU, `tests/savefile/run.sh`, game under xvfb with the test save: slot 1 loads `dqix.sav`; `settings.ini` with `save_slot = 2` starts on the title screen with no save (empty slot) and `dqix.sav` keeps its bytes (sha256 before/after); `save_slot = 1` again loads the test save; captures of Game > Saves (Windows menu: mingw build under Wine if the recipe allows, else describe).
