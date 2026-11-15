"""Download tests of hd-pack.exe (plan Task 5), with a local HTTP server.

  python3 tests/hdpack/t_download.py [hd-pack.exe]

Linux cloud session only (run under Wine), like tests/hdpack/run.sh: this
program moves and deletes files. Everything happens in a tempfile folder.
Without an argument, hd-pack.exe is cross-built into that folder.
"""
import hashlib
import http.server
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import threading

if platform.system() != "Linux":
    print("t_download.py: refused, these tests run only on Linux (cloud session, Wine)")
    sys.exit(2)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CUT = 1 << 20

passed = failed = 0


def check(name, cond):
    global passed, failed
    if cond:
        passed += 1
        print("PASS " + name)
    else:
        failed += 1
        print("FAIL " + name)


class Server:
    """Serves self.files (name -> bytes) with Range support and a few faults."""

    def __init__(self):
        self.files = {}
        self.requests = []          # (path, Range header or None)
        self.cut_once = set()       # names: the first GET sends CUT bytes then closes
        self.ignore_range = False   # answer 200 with the whole file even with Range
        outer = self

        class H(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *a):
                pass

            def do_GET(self):
                name = self.path.lstrip("/")
                rng = self.headers.get("Range")
                outer.requests.append((self.path, rng))
                data = outer.files.get(name)
                if data is None:
                    self.send_response(404)
                    self.send_header("Content-Length", "0")
                    self.end_headers()
                    return
                start = 0
                if rng and not outer.ignore_range:
                    start = int(rng.split("=")[1].split("-")[0])
                    if start >= len(data):
                        self.send_response(416)
                        self.send_header("Content-Range", "bytes */%d" % len(data))
                        self.send_header("Content-Length", "0")
                        self.end_headers()
                        return
                    self.send_response(206)
                    self.send_header("Content-Range", "bytes %d-%d/%d" % (start, len(data) - 1, len(data)))
                else:
                    self.send_response(200)
                body = data[start:]
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                if name in outer.cut_once:
                    outer.cut_once.discard(name)
                    self.wfile.write(body[:CUT])
                    self.wfile.flush()
                    self.close_connection = True
                    self.connection.shutdown(2)
                    return
                self.wfile.write(body)

        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def url(self):
        return "http://127.0.0.1:%d/" % self.port

    def zip_requests(self):
        return [r for r in self.requests if r[0].endswith(".zip")]


def wine_env(url):
    env = {k: v for k, v in os.environ.items() if "proxy" not in k.lower()}
    env["WINEDEBUG"] = "-all"
    env["HDPACK_URL"] = url
    return env


def run(game, url):
    r = subprocess.run(["wine", os.path.join(game, "hd-pack.exe")], env=wine_env(url),
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    return r.returncode


def read(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError:
        return None


def make_pack(tmp, name, textures):
    src = os.path.join(tmp, name + "-src")
    tex, mov, out = os.path.join(src, "tex"), os.path.join(src, "mov"), os.path.join(tmp, name)
    os.makedirs(tex)
    os.makedirs(mov)
    for n, data in textures.items():
        with open(os.path.join(tex, n), "wb") as f:
            f.write(data)
    with open(os.path.join(mov, "a.1.mpg"), "wb") as f:
        f.write(b"M" * 3000)
    with open(os.path.join(mov, "a.sig"), "wb") as f:
        f.write(b"S" * 40)
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "make_hd_pack.py"), tex, mov, out],
                   check=True, stdout=subprocess.DEVNULL)
    files = {n: read(os.path.join(out, n)) for n in os.listdir(out)}
    return files


def new_game(tmp, name, exe):
    g = os.path.join(tmp, name)
    os.makedirs(os.path.join(g, "textures", "hd"))
    os.makedirs(os.path.join(g, "textures-hd-outil"))
    with open(os.path.join(g, "textures", "hd", "0000000000000009.png"), "wb") as f:
        f.write(b"real-esrgan")
    shutil.copy(exe, os.path.join(g, "hd-pack.exe"))
    return g


def no_bar(g):
    p = read(os.path.join(g, "hd-pack-progress.txt"))
    return p is None or not p.startswith(b"working")


def installed(g, textures):
    hp = os.path.join(g, "textures", "hd-pack")
    return (all(read(os.path.join(hp, n)) == d for n, d in textures.items())
            and read(os.path.join(g, "movies", "a.1.mpg")) == b"M" * 3000
            and read(os.path.join(hp, "hd-pack.txt")) is not None)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = sys.argv[1] if len(sys.argv) > 1 else None
        if not exe:
            exe = os.path.join(tmp, "hd-pack.exe")
            subprocess.run(["x86_64-w64-mingw32-gcc", "-O2", "-municode", "-I", os.path.join(ROOT, "tools"),
                            "-o", exe, os.path.join(ROOT, "tools", "hd-pack.c"), "-lbcrypt", "-lwinhttp"], check=True)
        rnd = os.urandom
        textures = {"%016x.png" % (0x1000 + i): rnd(900_000) for i in range(3)}
        pack = make_pack(tmp, "pack", textures)
        texzip = [n for n in pack if n.startswith("hd-pack-textures")][0]
        check("setup: textures zip bigger than the cut", len(pack[texzip]) > 2 * CUT)

        # offline_leaves_no_bar
        g = new_game(tmp, "g-offline", exe)
        rc = run(g, "http://127.0.0.1:9/")
        check("offline_leaves_no_bar: exit 0, no bar, nothing changed", rc == 0 and no_bar(g)
              and not os.path.exists(os.path.join(g, "textures", "hd-pack"))
              and not os.path.exists(os.path.join(g, "hd-pack-tmp"))
              and read(os.path.join(g, "textures", "hd", "0000000000000009.png")) == b"real-esrgan")

        # resume_after_cut
        srv = Server()
        srv.files = dict(pack)
        srv.cut_once = {texzip}
        g = new_game(tmp, "g-resume", exe)
        rc = run(g, srv.url())
        part = os.path.join(g, "hd-pack-tmp", texzip + ".part")
        have = len(read(part) or b"")
        check("resume_after_cut: first run stops, nothing installed, no bar, Real-ESRGAN kept",
              rc != 0 and not os.path.exists(os.path.join(g, "textures", "hd-pack")) and no_bar(g)
              and read(os.path.join(g, "textures", "hd", "0000000000000009.png")) == b"real-esrgan")
        check("resume_after_cut: the .part holds what came (%d bytes)" % have,
              0 < have <= CUT and read(part) == pack[texzip][:have])
        srv.requests.clear()
        rc = run(g, srv.url())
        zr = [r for r in srv.zip_requests() if texzip in r[0]]
        check("resume_after_cut: second run asks only for the rest (Range)", zr == [("/" + texzip, "bytes=%d-" % have)])
        check("resume_after_cut: installed, files match", rc == 0 and installed(g, textures)
              and read(os.path.join(g, "hd-pack-progress.txt")) == b"done 0 100"
              and not os.path.exists(os.path.join(g, "hd-pack-tmp")))

        # installed_is_noop (network): only the manifest is fetched
        srv.requests.clear()
        rc = run(g, srv.url())
        check("installed_is_noop: only the manifest is asked for", rc == 0 and srv.requests == [("/hd-pack.txt", None)])

        # a server that ignores Range: the .part starts over, still right
        srv2 = Server()
        srv2.files = dict(pack)
        srv2.cut_once = {texzip}
        srv2.ignore_range = True
        g = new_game(tmp, "g-norange", exe)
        run(g, srv2.url())
        rc = run(g, srv2.url())
        check("Range ignored by the server: starts over, installed right", rc == 0 and installed(g, textures))

        # bytes that don't match the SHA-256: nothing kept, nothing installed
        srv3 = Server()
        srv3.files = dict(pack)
        bad = bytearray(pack[texzip])
        bad[100] ^= 0xFF
        srv3.files[texzip] = bytes(bad)
        g = new_game(tmp, "g-corrupt", exe)
        rc = run(g, srv3.url())
        check("corrupt download: fails, nothing installed, bad zip not kept, no bar",
              rc != 0 and not os.path.exists(os.path.join(g, "textures", "hd-pack")) and no_bar(g)
              and not os.path.exists(os.path.join(g, "hd-pack-tmp", texzip))
              and not os.path.exists(os.path.join(g, "hd-pack-tmp", texzip + ".part"))
              and read(os.path.join(g, "textures", "hd", "0000000000000009.png")) == b"real-esrgan")
        srv3.files[texzip] = pack[texzip]
        rc = run(g, srv3.url())
        check("corrupt download: next run with the right bytes installs", rc == 0 and installed(g, textures))

        # a .part longer than the zip (not ours to trust): starts over
        srv4 = Server()
        srv4.files = dict(pack)
        g = new_game(tmp, "g-long", exe)
        os.makedirs(os.path.join(g, "hd-pack-tmp"))
        with open(os.path.join(g, "hd-pack-tmp", texzip + ".part"), "wb") as f:
            f.write(b"x" * (len(pack[texzip]) + 10))
        rc = run(g, srv4.url())
        zr = [r for r in srv4.zip_requests() if texzip in r[0]]
        check("a .part too long: dropped, downloaded again from 0", rc == 0 and installed(g, textures)
              and zr == [("/" + texzip, None)])

        # a zip missing on the server
        srv5 = Server()
        srv5.files = dict(pack)
        del srv5.files[texzip]
        g = new_game(tmp, "g-404", exe)
        rc = run(g, srv5.url())
        check("missing zip (404): fails cleanly, no bar, nothing installed", rc != 0 and no_bar(g)
              and not os.path.exists(os.path.join(g, "textures", "hd-pack"))
              and not os.path.exists(os.path.join(g, "hd-pack-tmp", texzip + ".part")))

        # a manifest that isn't one (an HTML error page): nothing done
        srv6 = Server()
        srv6.files = {"hd-pack.txt": b"<html>rate limited</html>"}
        g = new_game(tmp, "g-badman", exe)
        rc = run(g, srv6.url())
        check("bad manifest: nothing done, no bar", no_bar(g) and not os.path.exists(os.path.join(g, "hd-pack-tmp"))
              and not os.path.exists(os.path.join(g, "textures", "hd-pack")))

        # every file outside the game folders is as it was
        check("source pack untouched", all(hashlib.sha256(read(os.path.join(tmp, "pack", n))).digest()
                                           == hashlib.sha256(d).digest() for n, d in pack.items()))
        for s in (srv, srv2, srv3, srv4, srv5, srv6):
            s.httpd.shutdown()
    print("t_download: %d passed, %d failed" % (passed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
