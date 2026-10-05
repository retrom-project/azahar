import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("core_release", Path(__file__).with_name("release.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.candidate = self.root / "candidate"
        self.candidate.mkdir()
        self.config = json.loads((ROOT / "retrom-fork.json").read_text())
        self.commit = "a" * 40
        self.tag = self.config["defaultBranch"].replace("retrom/", "retrom-core-") + "-r1"
        records = []
        for name in self.config["releaseAssets"]:
            if name == "rpg-runtime-release.json":
                continue
            data = ("contract fixture: " + name).encode()
            (self.candidate / name).write_bytes(data)
            records.append({"filename": name, "sizeBytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        self.descriptor = {"dirty": False, "commit": self.commit, "repository": self.config["forkRepository"],
                           "adapterAbi": self.config["adapterAbi"], "files": records}
        self.write_descriptor()

    def write_descriptor(self):
        (self.candidate / "retrom-core-candidate.json").write_text(json.dumps(self.descriptor))

    def publish(self):
        module.release(self.candidate, self.root / "release", self.tag, self.commit)

    def test_closed_verified_release(self):
        self.publish()
        metadata = json.loads((self.root / "release/rpg-runtime-release.json").read_text())
        self.assertEqual(metadata["commit"], self.commit)
        self.assertEqual({p.name for p in (self.root / "release").iterdir()}, set(self.config["releaseAssets"]))
        with self.assertRaisesRegex(ValueError, "OUTPUT_NOT_EMPTY"):
            self.publish()

    def test_dirty_or_unrelated_source_rejected(self):
        for key, value in [("dirty", True), ("commit", "b" * 40), ("adapterAbi", "old")]:
            with self.subTest(key=key):
                original = self.descriptor[key]
                self.descriptor[key] = value
                self.write_descriptor()
                with self.assertRaisesRegex(ValueError, "SOURCE_INVALID"):
                    self.publish()
                self.descriptor[key] = original

    def test_corrupted_and_extra_files_rejected(self):
        first = self.candidate / self.descriptor["files"][0]["filename"]
        first.write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "INTEGRITY_INVALID"):
            self.publish()
        (self.candidate / "undeclared.dat").write_bytes(b"unexpected")
        with self.assertRaisesRegex(ValueError, "FILES_INVALID"):
            self.publish()

    def test_noncanonical_tag_rejected(self):
        self.tag = "latest"
        with self.assertRaisesRegex(ValueError, "TAG_INVALID"):
            self.publish()
