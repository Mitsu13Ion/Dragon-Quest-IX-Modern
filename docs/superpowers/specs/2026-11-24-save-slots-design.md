# Save slots — design

## Why
A player asked on Discord for several save files. DQIX on DS has one adventure
log; on PC a player wants several playthroughs (a second hero, a friend on the
same PC) and to switch between them from the game's menu.

## Decided (Romuald, 2026-11-24)
- Slot 1 stays `game/dqix.sav` (nothing changes for existing players, the
  launcher or the in-game updater, which never touch `game/`). Slots 2..3 are
  `game/saves/slot<N>.sav`, each with its own copies in `game/saves/save-backups`.
- Switching restarts the game on the chosen save (like Game > Reset).
- At most 3 slots (Romuald, changed from 10 the same day).
- Each slot has a free name the player can change; default "Adventure <N>".

## How (builder's calls)
- **Menu** Game > Saves: three items, one per slot, a check on the active one.
  A used slot shows its name; an unused one shows "<N>. (empty: a new adventure)".
  Then "Rename this save...". Clicking another slot asks to confirm ("the game
  restarts on that save; anything since you last saved in the game is lost"),
  an empty one first asks a name. The slots are greyed in a multiplayer session
  (like Reset).
- **Settings** (`settings.ini`): `save_slot = 1..3` (missing = 1, so old
  settings files keep `dqix.sav`), `save_name_<N> = ...` for named slots.
  A slot is "used" when its file exists or it has a name.
- **Start**: `main.c` still opens the save given on the command line
  (`dqix.sav`). Once settings are read, if `save_slot` is not 1 the save chip is
  loaded again from `saves/slot<N>.sav` next to it (before the game reads it,
  before the DLC unlock). `backup_init` resets its state so it can run twice.
  `saves/` is made when needed.
- **Never lose a save**: the switch is refused while the game is writing its
  save (written to the chip less than half a second ago, as `backup_frame`
  waits); otherwise `restart_with` writes the current save (`backup_flush`)
  before starting again. Backups per slot come for free: `backup_copy` names
  copies after the save file and keeps 20 per name.
- **Linux**: no text entry there (`ask_text` returns nothing): new slots keep
  the default name; rename does nothing.
- Texts in English in the code, fr/de/it/es by `dqix-langues` (`lang/*.txt`).

## Not done (ask if wanted)
- Deleting a slot (risk of losing a save; the file can be removed by hand).
- Copying a slot into another.

## Tests
- `tests/savefile`: the slot file path (slot 1 = the given save, N = `saves/slotN.sav`
  next to it, out-of-range refused).
- Cloud: build, start on slot 1 with the test save, switch to an empty slot and
  back (settings.ini), check `dqix.sav` is byte-identical, captures of the menu.
