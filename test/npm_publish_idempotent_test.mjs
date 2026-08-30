import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { chmod, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import test from "node:test";

const exec = promisify(execFile);
const root = resolve(import.meta.dirname, "..");
const script = join(root, "scripts/publish-npm-idempotent.sh");
const version = "0.2.0-snapshot.g0123456789ab";
const sourceSha = "0123456789abcdef0123456789abcdef01234567";

async function runScenario(mode) {
  const directory = await mkdtemp(join(tmpdir(), "iox-npm-publish-test-"));
  try {
    const tarball = join(directory, "package.tgz");
    const statePath = join(directory, "state.json");
    const npmPath = join(directory, "npm");
    await writeFile(tarball, "deterministic tarball contents");
    await writeFile(statePath, JSON.stringify({ views: 0, publishes: 0 }));
    await writeFile(
      npmPath,
      `#!/usr/bin/env node
const fs = require("node:fs");
const state = JSON.parse(fs.readFileSync(process.env.FAKE_NPM_STATE, "utf8"));
const command = process.argv[2];
if (command === "view") {
  state.views += 1;
  fs.writeFileSync(process.env.FAKE_NPM_STATE, JSON.stringify(state));
  const visible = process.env.FAKE_NPM_MODE === "existing" ||
    process.env.FAKE_NPM_MODE === "conflict" ||
    ((process.env.FAKE_NPM_MODE === "race" || process.env.FAKE_NPM_MODE === "publish") && state.publishes > 0);
  if (!visible) process.exit(1);
  process.stdout.write(JSON.stringify({
    version: process.env.EXPECTED_VERSION,
    gitHead: process.env.FAKE_NPM_MODE === "conflict" ? "f".repeat(40) : process.env.EXPECTED_SHA,
    "dist.integrity": process.env.FAKE_INTEGRITY,
  }));
  process.exit(0);
}
if (command === "publish") {
  state.publishes += 1;
  fs.writeFileSync(process.env.FAKE_NPM_STATE, JSON.stringify(state));
  process.exit(process.env.FAKE_NPM_MODE === "race" ? 1 : 0);
}
process.exit(2);
`,
    );
    await chmod(npmPath, 0o755);
    const integrity = `sha512-${createHash("sha512").update(await readFile(tarball)).digest("base64")}`;
    let result;
    try {
      result = await exec("bash", [script], {
        env: {
          ...process.env,
          PATH: `${directory}:${process.env.PATH}`,
          TARBALL: tarball,
          PACKAGE_NAME: "@ilic/iox-wasm",
          EXPECTED_VERSION: version,
          EXPECTED_SHA: sourceSha,
          NPM_TAG: "snapshot",
          NPM_VERIFY_ATTEMPTS: "3",
          NPM_VERIFY_DELAY_SECONDS: "0",
          FAKE_NPM_MODE: mode,
          FAKE_NPM_STATE: statePath,
          FAKE_INTEGRITY: integrity,
        },
      });
    } catch (error) {
      result = error;
    }
    return {
      result,
      state: JSON.parse(await readFile(statePath, "utf8")),
    };
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
}

test("an identical published version is an idempotent success", async () => {
  const { result, state } = await runScenario("existing");
  assert.equal(result.code ?? 0, 0, result.stderr);
  assert.equal(state.publishes, 0);
});

test("a concurrent identical bootstrap converts a publish failure into success", async () => {
  const { result, state } = await runScenario("race");
  assert.equal(result.code ?? 0, 0, result.stderr);
  assert.equal(state.publishes, 1);
  assert.match(result.stdout, /idempotent duplicate/);
});

test("a newly published version is verified after publication", async () => {
  const { result, state } = await runScenario("publish");
  assert.equal(result.code ?? 0, 0, result.stderr);
  assert.equal(state.publishes, 1);
});

test("conflicting immutable metadata fails without publishing", async () => {
  const { result, state } = await runScenario("conflict");
  assert.notEqual(result.code ?? 0, 0);
  assert.equal(state.publishes, 0);
  assert.match(result.stderr, /conflicts with the immutable local artifact/);
});
