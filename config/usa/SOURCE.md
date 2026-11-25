# Source of `config/usa`

The symbol and relocation files under `config/usa/arm9/` (`symbols.txt`,
`relocs.txt` for the main binary, ITCM, DTCM and each overlay) come from the
dqix-decomp project, which is released into the public domain (Unlicense):

- Repository: https://github.com/DQIX/dqix-decomp
- Commit: `8c0513fa4a3e3fc0260e58b154ed8d13c003cdbf` (2026-10-10)
- Path in that repository: `config/usa/arm9/`

All 76 files are byte-identical to that commit (same git blob hashes). To
update them, copy the same paths from a newer dqix-decomp commit and change the
commit above.
