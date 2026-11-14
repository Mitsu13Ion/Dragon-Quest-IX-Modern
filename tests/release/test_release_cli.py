"""Tests for tools/release.py with a fake run(): no real git, gh or network.

    python -m unittest discover -s tests/release -v"""
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
import urllib.error
from contextlib import redirect_stdout, redirect_stderr
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

import release  # noqa: E402

SHA = "a" * 40
OTHER = "b" * 40
REPORT = "commit: %s\nresult: PASS\n"
SIGNATURE = "\U0001F916 Generated with [Claude Code](https://claude.com/claude-code)"


def good_notes(version):
    files = {"release-notes/%s.md" % version: "**How to install**\n- a\n"}
    for lang in ("fr", "de", "it", "es"):
        files["release-notes/%s/%s.md" % (lang, version)] = "- a\n"
    return files


class Fake:
    """Records every command. `rules` is a list of (prefix, returncode, stdout);
    the first rule whose words start the command answers it. Unmatched
    commands succeed with no output. A stdout that is an exception is raised."""

    def __init__(self, rules=()):
        self.rules = list(rules)
        self.cmds = []
        self.timeouts = []

    def __call__(self, cmd, check=True, timeout=None):
        self.cmds.append(list(cmd))
        self.timeouts.append(timeout)
        rc, out = 0, ""
        for prefix, r, o in self.rules:
            if cmd[:len(prefix)] == list(prefix):
                rc, out = r, o
                break
        if isinstance(out, Exception):
            raise out
        if check and rc:
            raise subprocess.CalledProcessError(rc, cmd, out, "boom")
        return subprocess.CompletedProcess(cmd, rc, out, "")

    def joined(self):
        return [" ".join(c) for c in self.cmds]

    def has(self, *words):
        return any(c[:len(words)] == list(words) for c in self.cmds)


def run_main(fake, argv, env=None):
    out, err = io.StringIO(), io.StringIO()
    with mock.patch.object(release, "run", fake), \
         mock.patch.object(release, "load_notes", good_notes), \
         mock.patch.dict(os.environ, env or {}), \
         redirect_stdout(out), redirect_stderr(err):
        code = release.main(argv)
    return code, out.getvalue() + err.getvalue()


def good_rules(**over):
    rules = {
        "branch": (["git", "rev-parse", "--abbrev-ref", "HEAD"], 0, "integ/release-v3\n"),
        "head": (["git", "rev-parse", "HEAD"], 0, SHA + "\n"),
        "ls": (["git", "ls-files"], 0, "README.md\nruntime/cpu.c\n"),
        "report": (["gh", "api"], 0, REPORT % SHA),
    }
    rules.update(over)
    return list(rules.values())


class Check(unittest.TestCase):
    def test_report_of_another_commit_is_refused(self):
        fake = Fake(good_rules(report=(["gh", "api"], 0, REPORT % OTHER)))
        code, out = run_main(fake, ["check", "v3"])
        self.assertEqual(code, 1)
        self.assertIn("relancer le test cloud sur " + SHA, out)

    def test_failed_report_is_refused(self):
        fake = Fake(good_rules(report=(["gh", "api"], 0, "commit: %s\nresult: FAIL\n" % SHA)))
        self.assertEqual(run_main(fake, ["check", "v3"])[0], 1)

    def test_good_check_passes_and_reads_the_rc_base_report(self):
        fake = Fake(good_rules())
        code, out = run_main(fake, ["check", "v3-rc1"])
        self.assertEqual(code, 0, out)
        api = [c for c in fake.joined() if c.startswith("gh api")][0]
        self.assertIn("captures/release-v3/report.md?ref=tests/release-v3", api)

    def test_tracked_forbidden_file_stops(self):
        fake = Fake(good_rules(ls=(["git", "ls-files"], 0, "rom/game.nds\n")))
        code, out = run_main(fake, ["check", "v3"])
        self.assertEqual(code, 1)
        self.assertIn("rom/game.nds", out)

    def test_wrong_branch_stops(self):
        fake = Fake(good_rules(branch=(["git", "rev-parse", "--abbrev-ref", "HEAD"], 0, "main\n")))
        self.assertEqual(run_main(fake, ["check", "v3"])[0], 1)

    def test_failing_unit_tests_stop(self):
        fake = Fake(good_rules(unit=([sys.executable, "-m", "unittest"], 1, "")))
        self.assertEqual(run_main(fake, ["check", "v3"])[0], 1)


class Publish(unittest.TestCase):
    def test_stable_push_failure_creates_no_release(self):
        fake = Fake(good_rules(stable=(["git", "push", "origin", "origin/main:stable"], 1, "")))
        code, out = run_main(fake, ["publish", "v3", "--title", "v3: t"])
        self.assertEqual(code, 1)
        self.assertTrue(fake.has("git", "push", "origin", "origin/main:stable"))
        self.assertFalse(fake.has("gh", "release", "create"))
        self.assertNotIn("--force", " ".join(fake.joined()))

    def test_different_tree_on_main_stops_before_stable(self):
        fake = Fake(good_rules(diff=(["git", "diff", "--quiet"], 1, "")))
        code, _ = run_main(fake, ["publish", "v3", "--title", "t"])
        self.assertEqual(code, 1)
        self.assertFalse(fake.has("git", "push", "origin", "origin/main:stable"))
        self.assertFalse(fake.has("gh", "release", "create"))

    def test_real_release_sequence(self):
        fake = Fake(good_rules())
        code, out = run_main(fake, ["publish", "v3", "--title", "v3: t"])
        self.assertEqual(code, 0, out)
        seq = fake.joined()
        keys = ["git push -u origin integ/release-v3", "gh pr create", "gh pr merge",
                "git fetch origin", "git diff --quiet " + SHA + " origin/main",
                "git push origin origin/main:stable", "gh release create v3"]
        pos = [next(i for i, c in enumerate(seq) if c.startswith(k)) for k in keys]
        self.assertEqual(pos, sorted(pos))
        create = fake.cmds[pos[-1]]
        for word in ("--target", "stable", "--latest", "--notes-file", "release-notes/v3.md"):
            self.assertIn(word, create)
        self.assertEqual(create[create.index("--title") + 1], "v3: t")
        pr = fake.cmds[pos[1]]
        self.assertTrue(pr[pr.index("--body") + 1].endswith(SIGNATURE))

    def test_report_mismatch_publishes_nothing(self):
        fake = Fake(good_rules(report=(["gh", "api"], 0, REPORT % OTHER)))
        code, _ = run_main(fake, ["publish", "v3", "--title", "t"])
        self.assertEqual(code, 1)
        for c in (("git", "push"), ("gh", "pr"), ("gh", "release")):
            self.assertFalse(fake.has(*c))

    def test_dry_run_runs_no_mutating_command(self):
        fake = Fake(good_rules())
        code, out = run_main(fake, ["publish", "v3", "--title", "t", "--dry-run"])
        self.assertEqual(code, 0, out)
        for c in (("git", "push"), ("gh", "pr"), ("gh", "release"), ("git", "fetch")):
            self.assertFalse(fake.has(*c), c)
        self.assertIn("[dry-run] gh release create v3", out)
        self.assertIn("[dry-run] git push origin origin/main:stable", out)

    def test_rc_publish(self):
        fake = Fake(good_rules())
        code, out = run_main(fake, ["publish", "v3-rc1", "--prerelease", "--title", "rc"])
        self.assertEqual(code, 0, out)
        create = [c for c in fake.cmds if c[:3] == ["gh", "release", "create"]][0]
        self.assertEqual(create[3], "v3-rc1")
        self.assertIn("--prerelease", create)
        self.assertEqual(create[create.index("--target") + 1], "integ/release-v3")
        self.assertNotIn("--latest", create)
        self.assertIn("release-notes/v3.md", create)
        for c in (("gh", "pr"), ("git", "fetch")):
            self.assertFalse(fake.has(*c))
        self.assertFalse(any(c[:2] == ["git", "push"] and "stable" in " ".join(c) for c in fake.cmds))

    def test_prerelease_flag_rules(self):
        fake = Fake(good_rules())
        self.assertEqual(run_main(fake, ["publish", "v3", "--prerelease", "--title", "t"])[0], 1)
        self.assertEqual(run_main(fake, ["publish", "v3-rc1", "--title", "t"])[0], 1)
        self.assertEqual(fake.cmds, [])


def runs(**kw):
    run = {"databaseId": 77, "headBranch": "v3", "event": "release",
           "status": "completed", "conclusion": "success"}
    run.update(kw)
    return json.dumps([run])


def release_json(tag="v3", latest=True, pre=False, zips=("Windows", "Linux")):
    names = ["Dragon-Quest-IX-Modern-%s-%s-x64.zip" % (tag, z) for z in zips]
    return json.dumps({"assets": [{"name": n} for n in names],
                       "isLatest": latest, "isPrerelease": pre})


class Verify(unittest.TestCase):
    def verify(self, rules, tag="v3"):
        fake = Fake(rules)
        with mock.patch.object(release.time, "sleep", lambda s: None):
            code, out = run_main(fake, ["verify", tag])
        return fake, code, out

    def test_failed_workflow_exits_1_with_message(self):
        fake, code, out = self.verify([
            (["gh", "run", "list"], 0, runs()),
            (["gh", "run", "watch"], 1, ""),
        ])
        self.assertEqual(code, 1)
        self.assertIn("workflow", out.lower())
        self.assertIn("reste publiée", out)
        self.assertEqual(sum(c[:3] == ["gh", "run", "watch"] for c in fake.cmds), 1)
        self.assertFalse(fake.has("gh", "release", "view"))

    def test_watch_has_a_15_minute_timeout(self):
        fake, code, out = self.verify([
            (["gh", "run", "list"], 0, runs()),
            (["gh", "release", "view"], 0, release_json()),
        ])
        self.assertEqual(code, 0, out)
        i = [c[:3] for c in fake.cmds].index(["gh", "run", "watch"])
        self.assertEqual(fake.timeouts[i], 900)
        self.assertIn("--exit-status", fake.cmds[i])

    def test_timeout_exits_1(self):
        exc = subprocess.TimeoutExpired(["gh"], 900)
        fake, code, out = self.verify([
            (["gh", "run", "list"], 0, runs()),
            (["gh", "run", "watch"], 0, exc),
        ])
        self.assertEqual(code, 1)
        self.assertIn("15 min", out)

    def test_no_run_found_exits_1(self):
        fake, code, out = self.verify([(["gh", "run", "list"], 0, "[]")])
        self.assertEqual(code, 1)

    def test_missing_zip_exits_1(self):
        fake, code, out = self.verify([
            (["gh", "run", "list"], 0, runs()),
            (["gh", "release", "view"], 0, release_json(zips=("Windows",))),
        ])
        self.assertEqual(code, 1)
        self.assertIn("Linux-x64.zip", out)

    def test_real_release_must_be_latest(self):
        fake, code, out = self.verify([
            (["gh", "run", "list"], 0, runs()),
            (["gh", "release", "view"], 0, release_json(latest=False)),
        ])
        self.assertEqual(code, 1)
        self.assertIn("latest", out)

    def test_rc_must_be_prerelease_and_not_latest(self):
        ok = [(["gh", "run", "list"], 0, runs(headBranch="v3-rc1")),
              (["gh", "release", "view"], 0, release_json("v3-rc1", latest=False, pre=True))]
        self.assertEqual(self.verify(ok, "v3-rc1")[1], 0)
        bad = [(["gh", "run", "list"], 0, runs(headBranch="v3-rc1")),
               (["gh", "release", "view"], 0, release_json("v3-rc1", latest=True, pre=True))]
        self.assertEqual(self.verify(bad, "v3-rc1")[1], 1)


class Announce(unittest.TestCase):
    def setUp(self):
        self.path = os.path.join(tempfile.mkdtemp(), "v3.txt")

    def write(self, text):
        with open(self.path, "w", encoding="utf-8") as f:
            f.write(text)

    def announce(self, env, opener, clear=False):
        with mock.patch.object(release.urllib.request, "urlopen", opener), \
             mock.patch.dict(os.environ, {}, clear=clear):
            return run_main(Fake(), ["announce", "v3", "--file", self.path], env)

    def test_too_long_is_refused_and_webhook_not_called(self):
        self.write("x" * 2000)
        opener = mock.Mock()
        code, out = self.announce({"DISCORD_WEBHOOK_URL": "https://example.invalid/tok"}, opener)
        self.assertEqual(code, 1)
        opener.assert_not_called()
        self.assertTrue(os.path.exists(self.path))

    def test_posts_json_without_printing_the_url(self):
        self.write("Salut")
        opener = mock.MagicMock()
        code, out = self.announce({"DISCORD_WEBHOOK_URL": "https://example.invalid/secret-token"}, opener)
        self.assertEqual(code, 0, out)
        req = opener.call_args[0][0]
        self.assertEqual(json.loads(req.data), {"content": "Salut"})
        self.assertEqual(req.get_header("User-agent"), "DQIX-Modern-release")
        self.assertNotIn("secret-token", out)

    def test_http_error_exits_1_and_keeps_file(self):
        self.write("Salut")
        err = urllib.error.HTTPError("https://example.invalid/secret-token", 400, "Bad", {}, None)
        code, out = self.announce({"DISCORD_WEBHOOK_URL": "https://example.invalid/secret-token"},
                                  mock.Mock(side_effect=err))
        self.assertEqual(code, 1)
        self.assertIn("400", out)
        self.assertIn(self.path, out)
        self.assertNotIn("secret-token", out)
        self.assertTrue(os.path.exists(self.path))

    def test_no_webhook_leaves_file(self):
        self.write("Salut")
        opener = mock.Mock()
        code, out = self.announce({}, opener, clear=True)
        self.assertEqual(code, 0, out)
        opener.assert_not_called()
        self.assertIn(self.path, out)


class DeleteRc(unittest.TestCase):
    def test_real_tag_refused_without_any_gh_call(self):
        fake = Fake()
        code, out = run_main(fake, ["delete-rc", "v3"])
        self.assertEqual(code, 1)
        self.assertEqual(fake.cmds, [])

    def test_non_prerelease_refused(self):
        fake = Fake([(["gh", "release", "view"], 0, json.dumps({"isPrerelease": False}))])
        self.assertEqual(run_main(fake, ["delete-rc", "v3-rc1"])[0], 1)
        self.assertFalse(fake.has("gh", "release", "delete"))

    def test_prerelease_deleted_with_cleanup_tag(self):
        fake = Fake([(["gh", "release", "view"], 0, json.dumps({"isPrerelease": True}))])
        self.assertEqual(run_main(fake, ["delete-rc", "v3-rc1"])[0], 0)
        self.assertIn(["gh", "release", "delete", "v3-rc1", "--cleanup-tag", "--yes"], fake.cmds)


class Prepare(unittest.TestCase):
    def test_prepare_dry_run(self):
        log = "Merge pull request #12 from x/y\nMerge pull request #11 from x/z\n"
        fake = Fake([
            (["git", "status", "--porcelain"], 0, ""),
            (["git", "tag"], 0, "v2.1\nv3-rc1\nv2.0\n"),
            (["git", "log"], 0, log),
        ])
        with mock.patch.object(release, "bump_file", lambda v: print("bump", v)):
            code, out = run_main(fake, ["prepare", "v3", "--dry-run"])
        self.assertEqual(code, 0, out)
        self.assertTrue(fake.has("gh", "auth", "status"))
        self.assertIn("[dry-run] git switch --no-track -c integ/release-v3 origin/main", out)
        self.assertIn("#12", out)
        self.assertIn("v2.1..origin/main", " ".join(fake.joined()))

    def test_dirty_tree_stops(self):
        fake = Fake([(["git", "status", "--porcelain"], 0, " M x\n")])
        self.assertEqual(run_main(fake, ["prepare", "v3"])[0], 1)


if __name__ == "__main__":
    unittest.main()
