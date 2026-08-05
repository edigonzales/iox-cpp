#!/usr/bin/env node

import { mkdir, mkdtemp, readFile, readdir, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const exec = promisify(execFile);

function argument(name, fallback) {
  const index = process.argv.indexOf(`--${name}`);
  return index === -1 ? fallback : process.argv[index + 1];
}

const staging = resolve(argument("staging-root", "build/release"));
const expectedVersion = argument("expected-version");
if (!expectedVersion) throw new Error("--expected-version is required");

const packageJsonPath = join(staging, "package", "package.json");
const packageJson = JSON.parse(await readFile(packageJsonPath, "utf8"));
if (packageJson.name !== "@interlis/iox-wasm") {
  throw new Error(`Unexpected package name: ${packageJson.name}`);
}
if (packageJson.version !== expectedVersion) {
  throw new Error(
    `Expected ${expectedVersion}, found ${packageJson.version} in staged package`,
  );
}

const consumer = await mkdtemp(join(tmpdir(), "iox-release-consumer-"));
try {
  await exec("npm", ["pack", "--ignore-scripts", "--json", "--pack-destination", consumer], {
    cwd: join(staging, "package"),
  });
  const files = await readdir(consumer);
  if (files.length !== 1 || !files[0].endsWith(".tgz")) {
    throw new Error(`Expected exactly one npm tarball, found ${files.join(", ")}`);
  }
  const installDir = join(consumer, "installed");
  await mkdir(installDir);
  await exec("npm", ["init", "--yes", "--no-workspaces"], { cwd: installDir });
  await exec("npm", ["install", "--ignore-scripts", join(consumer, files[0])], {
    cwd: installDir,
  });
  const installed = JSON.parse(
    await readFile(join(installDir, "node_modules/@interlis/iox-wasm/package.json"), "utf8"),
  );
  if (installed.version !== expectedVersion) {
    throw new Error(`Installed package has version ${installed.version}`);
  }
} finally {
  await rm(consumer, { recursive: true, force: true });
}

console.log(`Verified @interlis/iox-wasm@${expectedVersion}`);
