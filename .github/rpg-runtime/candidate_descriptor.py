#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def git_bytes(*args: str) -> bytes:
    result = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, check=False)
    if result.returncode != 0:
        raise SystemExit("PFB_WORKTREE_INVALID")
    return result.stdout


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def checked_output(raw: str, empty: bool) -> Path:
    output = Path(raw)
    if not output.is_absolute() or output.is_symlink() or not output.is_dir():
        raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
    if empty and any(output.iterdir()):
        raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
    return output


def source_paths() -> list[bytes]:
    paths = git_bytes("ls-files", "--cached", "--others", "--exclude-standard", "-z")
    return sorted({raw for raw in paths.split(b"\0") if raw and os.path.lexists(ROOT / os.fsdecode(raw))})


def source_digest(root=ROOT) -> str:
    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args])
    tracked = {}
    for raw in git("ls-files", "--stage", "-z").split(b"\0"):
        if not raw:
            continue
        details, path = raw.split(b"\t", 1)
        mode, commit, stage = details.decode().split()
        if stage != "0":
            raise SystemExit("PFB_WORKTREE_INVALID")
        tracked[path] = (mode, commit)
    paths = sorted(set(git("ls-files", "--cached", "--others", "--exclude-standard", "-z").split(b"\0")) - {b""})
    canonical = lambda value: json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=True).encode()
    records = []
    for raw in paths:
        path = root / os.fsdecode(raw)
        if not os.path.lexists(path):
            continue
        info = path.lstat()
        tracked_mode, commit = tracked.get(raw, (None, None))
        if tracked_mode == "160000" and path.is_dir():
            actual = subprocess.check_output(["git", "-C", str(path), "rev-parse", "--show-toplevel"], text=True).strip()
            nested = {"indexCommit": commit, "worktreeCommit": None, "sourceTreeSha256": None}
            if Path(actual).resolve() == path.resolve():
                nested["worktreeCommit"] = subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
                nested["sourceTreeSha256"] = source_digest(path)
            sha = hashlib.sha256(canonical(nested)).hexdigest()
            mode = "160000"
        elif stat.S_ISLNK(info.st_mode):
            mode = "120000"
            sha = hashlib.sha256(os.readlink(path).encode()).hexdigest()
        elif stat.S_ISREG(info.st_mode):
            mode = tracked_mode or ("100755" if info.st_mode & stat.S_IXUSR else "100644")
            sha = digest(path)
        else:
            raise SystemExit("PFB_WORKTREE_INVALID")
        records.append({"mode": mode, "path": raw.decode("utf-8"), "sha256": sha})
    return hashlib.sha256(canonical(records)).hexdigest()


def finalize(output: Path, core_id: str) -> None:
    fork = json.loads((ROOT / "retrom-fork.json").read_text())
    expected = sorted(name for name in fork["releaseAssets"] if name != "rpg-runtime-release.json")
    actual = sorted(path.name for path in output.iterdir())
    if actual != expected:
        raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
    files = [{"filename": name, "sizeBytes": (output / name).stat().st_size,
              "sha256": digest(output / name)} for name in expected]
    descriptor = {
        "adapterAbi": fork["adapterAbi"],
        "branch": git_bytes("rev-parse", "--abbrev-ref", "HEAD").decode().strip(),
        "commit": git_bytes("rev-parse", "HEAD").decode().strip(),
        "coreId": core_id,
        "dirty": bool(git_bytes("status", "--porcelain=v1", "-z")),
        "files": files,
        "kind": "RETROM_CORE_CANDIDATE_V1",
        "repository": fork["forkRepository"],
        "schemaVersion": 1,
        "sourceTreeSha256": source_digest(),
    }
    (output / "retrom-core-candidate.json").write_text(
        json.dumps(descriptor, separators=(",", ":"), sort_keys=True) + "\n")


parser = argparse.ArgumentParser()
parser.add_argument("action", choices=("prepare", "finalize", "paths", "digest"))
parser.add_argument("output")
parser.add_argument("--core-id")
args = parser.parse_args()
if args.action == "paths":
    import sys
    sys.stdout.buffer.write(b"\0".join(source_paths()) + b"\0")
    raise SystemExit(0)
if args.action == "digest":
    print(source_digest())
    raise SystemExit(0)
destination = checked_output(args.output, args.action == "prepare")
if args.action == "finalize":
    if not args.core_id:
        raise SystemExit("PFB_CANDIDATE_OUTPUT_INVALID")
    finalize(destination, args.core_id)
