import importlib.util
import json
import os
import pathlib
import tempfile
import unittest
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("release_metadata", ROOT / "scripts/release_metadata.py")
release_metadata = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(release_metadata)


class DependencyLockTest(unittest.TestCase):
    sha = "0123456789abcdef0123456789abcdef01234567"

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

    def test_lock_rejects_a_runtime_version_from_another_release_line(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "release").mkdir()
            lock = json.loads(
                (ROOT / "release/dependencies.lock.json").read_text(encoding="utf-8")
            )
            lock["dependencies"]["ilic"]["runtimeVersion"] = "0.11.0-SNAPSHOT"
            (root / "release/dependencies.lock.json").write_text(
                json.dumps(lock), encoding="utf-8"
            )
            with self.assertRaisesRegex(ValueError, "share a base version"):
                release_metadata.load_lock(root)

    def test_snapshot_version_is_deterministic_and_uses_twelve_sha_characters(self):
        version = release_metadata.snapshot_version("0.2.0", self.sha)
        self.assertEqual(version, "0.2.0-snapshot.g0123456789ab")
        self.assertEqual(
            release_metadata.validate_artifact_version(version, self.sha), "snapshot"
        )

    def test_old_new_publication_schemas_are_rejected(self):
        for version in (
            "0.2.0-SNAPSHOT.20260826043335.32930660314",
            "0.2.0-snapshot.01234567",
            "0.2.0-snapshot.gffffffffffff",
        ):
            with self.subTest(version=version):
                with self.assertRaises(ValueError):
                    release_metadata.validate_artifact_version(version, self.sha)

    def test_manifest_contains_full_locked_provenance(self):
        manifest = release_metadata.release_manifest(
            root=ROOT,
            artifact_version="0.2.0-snapshot.g0123456789ab",
            source_sha=self.sha,
            run_id="42",
            published_at="2026-08-29T12:00:00Z",
            toolchain="emscripten-3.1.64",
        )
        self.assertEqual(manifest["sourceSha"], self.sha)
        self.assertEqual(
            manifest["dependencies"]["ilic"]["sourceSha"],
            release_metadata.load_lock(ROOT)["dependencies"]["ilic"]["sourceSha"],
        )
        self.assertEqual(manifest["build"]["githubRunId"], "42")

    def test_github_environment_export_does_not_depend_on_the_job_shell(self):
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "github-env"
            with mock.patch.dict(os.environ, {"GITHUB_ENV": str(output)}):
                result = release_metadata.main(
                    ["--project-root", str(ROOT), "export-github-env"]
                )
            self.assertEqual(result, 0)
            exported = dict(
                line.split("=", 1)
                for line in output.read_text(encoding="utf-8").splitlines()
            )
            lock = release_metadata.load_lock(ROOT)
            self.assertEqual(exported["IOX_VCPKG_REF"], lock["vcpkg"]["toolRef"])
            self.assertEqual(
                exported["IOX_ILIC_RUNTIME_VERSION"],
                lock["dependencies"]["ilic"]["runtimeVersion"],
            )
            self.assertEqual(
                exported["IOX_ILIC_SOURCE_SHA"],
                lock["dependencies"]["ilic"]["sourceSha"],
            )


if __name__ == "__main__":
    unittest.main()
