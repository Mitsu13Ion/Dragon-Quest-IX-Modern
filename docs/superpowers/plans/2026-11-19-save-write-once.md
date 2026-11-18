# Save write once per game save — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** write `dqix.sav` once when the game has finished writing its save, not after every save-chip command, so the safe write of PR #162 (temp file + disk sync + replace) costs one write per save instead of ~450.

**Architecture:** `backup_xfer` (`runtime/card.c`) stops calling `backup_flush()` after each chip command; it only marks the save dirty, as now. A new `backup_frame()` in `card.c`, called once per DS frame from `time_advance_lines` (`runtime/io.c:187`, where `g_frame` goes up), calls `backup_flush()` once the save is dirty and the game has not written to the chip for `SETTLE` frames. Every other caller of `backup_flush()` (autosave, quit, restart, update, fatal error, session end, F10 report) is unchanged and still writes at once.

**Tech Stack:** C (runtime), cloud test (`docs/TESTS-CLOUD.md`).

**Spec:** follow-up of `docs/superpowers/plans/2026-11-18-safe-save-write.md`, Task 3, step 2. Measured in the cloud on f657ae4: 512 safe writes of 64 KB = 286-371 ms on ext4 (0.56-0.72 ms each), above the ~200 ms threshold. The game's save routine writes 2 x 0x6FE4 bytes (`autosave.c` header), i.e. ~450 page writes of 128 bytes, each followed today by a full-file write.

## Global Constraints

- Same branch and PR: `dev/safe-save-write`, PR #162 (not merged yet, so the slow version never ships).
- No shared file touched: `card.c`, `io.c`, `nds.h` belong to Dev (`runtime/` minus the shared list in `CLAUDE.md`).
- `SETTLE = 30` frames (0.5 s of DS time). Counted in DS frames, not wall time, so replays and sessions stay deterministic (file writes do not change the emulation anyway).
- Keep: `backup_copy()` and its 10-second rule, the warn-once log, `g_backup_dirty` cleared only on success, every existing `backup_flush()` caller.

## Review Focus

1. Quit, restart or update less than 0.5 s after the game's last chip write → those paths already call `backup_flush()`, the save is written. To verify by reading each caller, not assumed.
2. A crash during the 0.5 s window → `dqix.sav` keeps the previous whole save (before: a half-written new save, since the file followed each page). Better than today; say it in the PR.
3. A write that fails → dirty stays 1, log warns once, `backup_frame()` tries again 600 frames (10 s) later, not every frame (60 disk syncs a second on the emulation thread during a long failure). Quit and the other direct callers still try at once.
4. Erase commands (0xDB/0xD8) set dirty without touching `g_backup_written` → `backup_frame()` writes on the next frame; harmless (DQIX's EEPROM never erases).
5. `backup_copy()` "nothing written for 10 seconds" still means "a new game save starts": one flush per save keeps it true.

---

### Task 1: one log line per real file write (the measuring stick, red)

**Files:** Modify `runtime/card.c` (`backup_flush`).

- [ ] **Step 1:** After a successful `save_file_atomic`, `LOG_INFO("save: wrote %s\n", g_backup_path);` (replaces the "written again" line, which it covers).
- [ ] **Step 2 (cloud, red):** run the game with a save and an in-game save on f657ae4 + this commit: the church save if an input script reaching the priest exists in the private repo, else autosave with `DQIX_AUTOSAVE_FRAMES=600` and `settings.ini` autosave on (that one goes through `backup_store`, one flush). Expected for a game save: hundreds of `save: wrote` lines. Count them, and time the save (first to last line).
- [ ] **Step 3:** Commit `Save: log each file write`.

### Task 2: `backup_frame()` writes once the game is done (green)

**Files:** Modify `runtime/card.c` (`backup_xfer`, new `backup_frame`), `runtime/nds.h` (prototype), `runtime/io.c:187`.

**Interfaces:** Produces `void backup_frame(void);` — called once per DS frame.

- [ ] **Step 1:** In `backup_xfer`, remove the `backup_flush();` after a command ends (the dirty flag is already set by write and erase).
- [ ] **Step 2:** `void backup_frame(void)` in `card.c`: `if (g_backup_dirty && g_frame - g_backup_written >= 30) backup_flush();` with a one-line comment on why (one write per game save, the safe write costs a disk sync).
- [ ] **Step 3:** Call `backup_frame();` in `io.c:187` next to `wfc_frame();`.
- [ ] **Step 4 (cloud, green):** same run as Task 1 step 2. Expected: one `save: wrote` line per game save (two if the save spans more than 0.5 s of pauses, never hundreds); `dqix.sav` 65536 bytes, no `.tmp`; quitting right after a save still writes it; full cloud recipe PASS (build, `tests/semantics`, captures, `tests/savefile/run.sh`).
- [ ] **Step 5:** Commit `Save: write dqix.sav once the game has finished writing`.

### Task 3: review and report

- [ ] `pr-review-toolkit:code-simplifier`, `dqix-relecteur`, `/pr-review-toolkit:review-pr`, `/ponytail-review`; PR #162 body updated (one write per save, the crash window); cloud PASS on the exact head commit, report on `tests/safe-save-write`.
