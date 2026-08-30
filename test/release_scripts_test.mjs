import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { validateRegistryVersion } from "../scripts/prepare-vcpkg-registry-port.mjs";
import { stageRelease } from "../scripts/stage-release.mjs";

const sha = "0123456789abcdef0123456789abcdef01234567";
const version = "0.2.0-snapshot.g0123456789ab";

test("vcpkg accepts only the deterministic new snapshot form", () => {
  assert.equal(validateRegistryVersion(version, sha).kind, "snapshot");
  for (const legacy of [
    "0.2.0-snapshot.01234567",
    "0.2.0-SNAPSHOT.20260826043335.32930660314",
  ]) {
    assert.throws(() => validateRegistryVersion(legacy, sha));
  }
});

test("staged npm package contains gitHead and complete release provenance", async () => {
  const root = await mkdtemp(join(tmpdir(), "iox-release-test-"));
  const source = join(root, "source");
  const output = join(root, "output");
  await mkdir(source);
  await writeFile(
    join(source, "package.json"),
    `${JSON.stringify({ name: "@ilic/iox-wasm", version: "0.2.0", files: ["iox-wasm.mjs", "iox-wasm.wasm"] })}\n`,
  );
  await writeFile(join(source, "iox-wasm.mjs"), "export default {};\n");
  await writeFile(join(source, "iox-wasm.wasm"), "wasm");
  const releaseManifestPath = join(root, "interlis-release.json");
  await writeFile(
    releaseManifestPath,
    `${JSON.stringify({ project: "iox-cpp", artifactVersion: version, versionKind: "snapshot", sourceSha: sha })}\n`,
  );

  await stageRelease({
    channel: "snapshot",
    version,
    "source-sha": sha,
    "release-manifest": releaseManifestPath,
    package: source,
    output,
  });

  const packageJson = JSON.parse(
    await readFile(join(output, "package", "package.json"), "utf8"),
  );
  assert.equal(packageJson.version, version);
  assert.equal(packageJson.gitHead, sha);
  assert(packageJson.files.includes("interlis-release.json"));
  const provenance = JSON.parse(
    await readFile(join(output, "package", "interlis-release.json"), "utf8"),
  );
  assert.equal(provenance.sourceSha, sha);
});
