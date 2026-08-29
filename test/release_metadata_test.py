import importlib.util
import json
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("release_metadata", ROOT / "scripts/release_metadata.py")
release_metadata = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(release_metadata)


class DependencyLockTest(unittest.TestCase):
    def test_lock_renders_exact_ilic_override_and_baseline(self):
        data = release_metadata.load_lock(ROOT)
        files = release_metadata.rendered_files(data)
        manifest = json.loads(files[pathlib.Path("release/vcpkg-dependencies/vcpkg.json")])
        config = json.loads(files[pathlib.Path("release/vcpkg-dependencies/vcpkg-configuration.json")])
        self.assertEqual(manifest["overrides"][0]["version-string"], data["dependencies"]["ilic"]["version"])
        self.assertEqual(config["registries"][0]["baseline"], data["vcpkg"]["registry"]["baseline"])

    def test_check_detects_a_changed_generated_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "release").mkdir()
            (root / "release/dependencies.lock.json").write_text(
                (ROOT / "release/dependencies.lock.json").read_text(), encoding="utf-8"
            )
            release_metadata.sync(root, False)
            path = root / "release/vcpkg-dependencies/vcpkg.json"
            path.write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "stale"):
                release_metadata.sync(root, True)


if __name__ == "__main__":
    unittest.main()
