#!/bin/sh
# The launcher's work (launcher/lnch_work.c) on Windows, under Wine, with
# DQIX_LAUNCHER_HEADLESS=1 (no window) and DQIX_UPDATE_SOURCE (no network):
#   sh tests/launcher/t_work_wine.sh <SDL2 mingw folder: include/SDL2, lib, bin/SDL2.dll>
# Linux cloud session only (Wine), on folders made by mktemp -d: never on a
# player's PC. A fake BUILD.bat stands for the build.
set -eu
[ "$(uname -s)" = Linux ] || { echo "t_work_wine.sh: Linux cloud session only" >&2; exit 2; }
SDL=${1:?usage: t_work_wine.sh <SDL2 mingw folder>}
for t in x86_64-w64-mingw32-gcc wine; do command -v $t >/dev/null || { echo "$t missing" >&2; exit 2; }; done
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d)
case "$T" in /tmp/?*) ;; *) echo "unexpected temp folder $T" >&2; exit 2 ;; esac
trap 'rm -rf -- "$T"' EXIT INT TERM
export WINEDEBUG=-all
unset TMPDIR   # Wine 9.0 aborts at start (free(): invalid pointer) with it set in the cloud container
x86_64-w64-mingw32-gcc -std=c99 -O1 -I "$ROOT/launcher" -I "$ROOT/runtime" -I "$SDL/include/SDL2" -I "$SDL/include" \
    -o "$T/dqix-launcher.exe" "$ROOT"/launcher/*.c "$ROOT/runtime/update.c" -L "$SDL/lib" -lSDL2 -lwinhttp -lm
fails=0
check() { if [ "$1" = 0 ]; then echo "ok   $2"; else echo "FAIL $2"; sed 's/^/     /' "$T/out.txt"; fails=$((fails + 1)); fi; }

# a project: runtime/version.h, BUILD.bat (fake), game/ with the launcher
project() {   # project <name> <local version> <BUILD.bat exit code>
    P="$T/$1"; mkdir -p "$P/runtime" "$P/game/data" "$P/game/notes"
    printf '#pragma once\n#define DQIX_VERSION "%s"\n' "$2" > "$P/runtime/version.h"
    printf '@echo off\r\necho progress 0.1 Checking the ROM\r\necho progress 0.5 Compiling\r\nif "%s"=="0" (echo x> game\\dqix.exe& echo progress 1 Done& exit /b 0)\r\necho BUILD FAILED (compiling): gcc missing\r\nexit /b 1\r\n' "$3" > "$P/BUILD.bat"
    cp "$T/dqix-launcher.exe" "$SDL/bin/SDL2.dll" "$P/game/"
    echo rom > "$P/game/data/dqix.nds"
    printf 'hd_pack_download = 0\r\n' > "$P/game/settings.ini"
    printf 'Dragon Quest IX Modern v3.2: install fixed.\n' > "$P/game/notes/v3.2.md"
}
source_at() {  # the "GitHub" the launcher sees: stable at <version>
    S="$T/src-$1"; mkdir -p "$S/stable/runtime" "$S/main/runtime"
    printf '#define X\n#define DQIX_VERSION "%s"\n' "$1" > "$S/stable/runtime/version.h"
    printf '#define X\n#define DQIX_VERSION "%s-nightly1"\n' "$1" > "$S/main/runtime/version.h"
    printf '[{"tag_name":"v3.2","name":"v3.2","draft":false,"prerelease":false,"published_at":"2026-11-22T10:00:00Z"}]' > "$S/releases.json"
}
launch() { (cd "$1/game" && DQIX_LAUNCHER_HEADLESS=1 DQIX_UPDATE_SOURCE=$(winepath -w "$2" 2>/dev/null) wine ./dqix-launcher.exe > "$T/out.txt" 2>&1); }

# not built yet, nothing newer: the build runs, PLAY lights up, the news comes from "GitHub"
source_at v3.2; project p1 v3.2 0
set +e; launch "$T/p1" "$T/src-v3.2"; r=$?; set -e
[ "$r" = 0 ] && grep -q "update=skipped build=done pack=skipped online=1 can_play=1" "$T/out.txt" && grep -q "news: v3.2 2026-11-22 install fixed." "$T/out.txt"
check $? work_builds_when_not_built

# the build fails: its BUILD FAILED line is shown, PLAY stays off
project p2 v3.2 1
set +e; launch "$T/p2" "$T/src-v3.2"; r=$?; set -e
[ "$r" = 1 ] && grep -q "build=failed" "$T/out.txt" && grep -q "BUILD FAILED (compiling): gcc missing" "$T/out.txt"
check $? work_build_failure_shown

# offline (no source): a built game can be played
project p3 v3.2 0; echo x > "$T/p3/game/dqix.exe"
set +e; launch "$T/p3" "$T/nowhere"; r=$?; set -e
[ "$r" = 0 ] && grep -q "online=0 can_play=1" "$T/out.txt"
check $? work_offline_complete_can_play

# a newer stable: downloaded and put in place (update\launcher-apply.bat: robocopy and tar,
# which Wine may not have: then the test says so instead of failing)
source_at v3.3
mkdir -p "$T/zip/DQIX-PC/runtime"; cp "$T/p1/BUILD.bat" "$T/zip/DQIX-PC/"; printf '#define DQIX_VERSION "v3.3"\n' > "$T/zip/DQIX-PC/runtime/version.h"
(cd "$T/zip" && zip -qr "$T/src-v3.3/stable.zip" DQIX-PC)
project p4 v3.2 0; echo x > "$T/p4/game/dqix.exe"
set +e; launch "$T/p4" "$T/src-v3.3"; r=$?; set -e
if grep -qi "tar\|robocopy" "$T/out.txt" && grep -q "update=failed" "$T/out.txt"; then
    echo "skip work_update_then_build (Wine has no tar/robocopy: check on Windows)"
else
    [ "$r" = 0 ] && grep -q "update=done build=done" "$T/out.txt" && [ -f "$T/p4/update/previous/BUILD.bat" ] && [ -f "$T/p4/update/rollback.bat" ] && grep -q v3.3 "$T/p4/runtime/version.h"
    check $? work_update_then_build
fi

[ "$fails" = 0 ] && echo "all passed" || { echo "$fails failed"; exit 1; }
