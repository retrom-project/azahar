import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class RevisionTests(unittest.TestCase):
    def test_container_worktree_uses_explicit_revision(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / 'revision.cmake'
            script.write_text(f'include("{ROOT}/CMakeModules/GenerateBuildInfo.cmake")\n'
                              f'set(SRC_DIR "{root}")\n'
                              'generate_build_info()\n'
                              f'file(WRITE "{root}/revision.txt" "${{GIT_REV}}")\n')
            revision = '1a' * 20
            subprocess.run(['cmake', f'-DRETROM_SOURCE_REV={revision}', '-P', str(script)], check=True)
            self.assertEqual((root / 'revision.txt').read_text(), revision)
            failed = subprocess.run(['cmake', '-DRETROM_SOURCE_REV=UNKNOWN', '-P', str(script)],
                                    capture_output=True, text=True)
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn('RETROM_SOURCE_REV_INVALID', failed.stderr)
