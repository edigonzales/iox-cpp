import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import {
  prepareRegistryPort,
  rewriteManifest,
  rewritePortfile,
  validateRegistryVersion,
} from "../../scripts/prepare-vcpkg-registry-port.mjs";

const sourceSha = "0123456789abcdef0123456789abcdef01234567";
const sha512 = "a".repeat(128);

const portfile = `vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO edigonzales/iox-cpp
    REF aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
    SHA512 ${"b".repeat(128)}
    HEAD_REF codex-port
)
`;

const manifest = `${JSON.stringify(
  {
    name: "iox-cpp",
    "version-string": "0.1.0-snapshot.deadbeef",
    description: "Native INTERLIS event I/O library",
    license: "MIT",
    features: {
      ilic: {
        description: "Build ilic model integration",
        dependencies: ["ilic"],
      },
    },
  },
  null,
  2,
)}\n`;

test("validates stable and immutable snapshot versions", () => {
  assert.deepEqual(validateRegistryVersion("0.1.0", sourceSha), {
    kind: "stable",
    version: "0.1.0",
  });
  assert.deepEqual(validateRegistryVersion("0.1.0-snapshot.01234567", sourceSha), {
    kind: "snapshot",
    version: "0.1.0-snapshot.01234567",
    baseVersion: "0.1.0",
  });
  assert.throws(
    () => validateRegistryVersion("0.1.0-snapshot.89abcdef", sourceSha),
    /does not match source SHA/,
  );
});

test("rewrites exactly the source revision and archive digest", () => {
  const rewritten = rewritePortfile(portfile, sourceSha, sha512);
  assert.match(rewritten, new RegExp(`REF ${sourceSha}`));
  assert.match(rewritten, new RegExp(`SHA512 ${sha512}`));
  assert.doesNotMatch(rewritten, /REF a{40}/);
});

test("rewrites the manifest version while preserving the ilic feature", () => {
  const rewritten = JSON.parse(
    rewriteManifest(manifest, "0.1.0-snapshot.01234567", sourceSha),
  );
  assert.equal(rewritten.name, "iox-cpp");
  assert.equal(rewritten["version-string"], "0.1.0-snapshot.01234567");
  assert.deepEqual(rewritten.features.ilic.dependencies, ["ilic"]);
});

test("renders a registry port while preserving auxiliary files", async () => {
  const root = await mkdtemp(join(tmpdir(), "iox-cpp-vcpkg-port-"));
  const templateDir = join(root, "template");
  const outputDir = join(root, "output");
  try {
    await mkdir(templateDir, { recursive: true });
    await Promise.all([
      writeFile(join(templateDir, "portfile.cmake"), portfile),
      writeFile(join(templateDir, "vcpkg.json"), manifest),
      writeFile(join(templateDir, "usage"), "usage text\n"),
    ]);

    await prepareRegistryPort({
      templateDir,
      outputDir,
      version: "0.1.0-snapshot.01234567",
      sourceSha,
      sha512,
    });

    assert.match(
      await readFile(join(outputDir, "portfile.cmake"), "utf8"),
      new RegExp(`REF ${sourceSha}`),
    );
    assert.equal(await readFile(join(outputDir, "usage"), "utf8"), "usage text\n");
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("rejects ambiguous port recipes instead of silently rewriting them", () => {
  assert.throws(
    () => rewritePortfile(`${portfile}\n    REF another-ref\n`, sourceSha, sha512),
    /exactly one REF line/,
  );
});
