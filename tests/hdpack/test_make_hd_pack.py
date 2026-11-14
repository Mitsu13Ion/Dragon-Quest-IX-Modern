"""Test tools/make_hd_pack.py: stored zips + manifest, split by size."""
import hashlib
import os
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "..", "tools", "make_hd_pack.py")

with tempfile.TemporaryDirectory() as tmp:
    tex = os.path.join(tmp, "tex")
    mov = os.path.join(tmp, "mov")
    out = os.path.join(tmp, "out")
    os.makedirs(tex)
    os.makedirs(mov)
    names = ["%016x.png" % (0xabc0 + i) for i in range(5)]
    for i, n in enumerate(names):
        with open(os.path.join(tex, n), "wb") as f:
            f.write(bytes([i]) * 600)
    with open(os.path.join(tex, "readme.txt"), "wb") as f:
        f.write(b"stray")
    with open(os.path.join(tex, "123.png"), "wb") as f:
        f.write(b"short name, not a texture")
    with open(os.path.join(mov, "a.mpg"), "wb") as f:
        f.write(b"M" * 300)
    with open(os.path.join(mov, "a.sig"), "wb") as f:
        f.write(b"S" * 20)
    with open(os.path.join(mov, "notes.txt"), "wb") as f:
        f.write(b"stray movie dir file")

    # ~730 bytes per entry; 2400 bytes allows 3 per zip -> 2 texture zips
    r = subprocess.run([sys.executable, TOOL, tex, mov, out,
                        "--max-gb", str(2400 / 1024 ** 3)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr

    with open(os.path.join(out, "hd-pack.txt"), "rb") as f:
        raw = f.read()
    assert b"\r" not in raw
    lines = raw.decode().split("\n")
    assert lines[-1] == "", lines
    lines = lines[:-1]
    assert lines[0] == "hd-pack 1", lines
    zips = lines[1:]
    assert len(zips) == 3, zips

    seen = []
    for line in zips:
        kind, name, size, sha = line.split(" ")
        assert kind == "zip"
        path = os.path.join(out, name)
        data = open(path, "rb").read()
        assert len(data) == int(size), name
        assert hashlib.sha256(data).hexdigest() == sha, name
        assert len(data) <= 2400 or name == "hd-pack-movies.zip", name
        with zipfile.ZipFile(path) as z:
            for info in z.infolist():
                assert info.compress_type == zipfile.ZIP_STORED, info.filename
                seen.append(info.filename)
    assert sorted(os.listdir(out)) == sorted(
        ["hd-pack.txt", "hd-pack-textures-1.zip", "hd-pack-textures-2.zip",
         "hd-pack-movies.zip"]), os.listdir(out)
    assert sorted(seen) == sorted(
        ["textures/" + n for n in names] + ["movies/a.mpg", "movies/a.sig"]), seen

    # --version
    out2 = os.path.join(tmp, "out2")
    r = subprocess.run([sys.executable, TOOL, tex, mov, out2, "--version", "7"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    assert open(os.path.join(out2, "hd-pack.txt")).read().startswith("hd-pack 7\n")
print("OK")
