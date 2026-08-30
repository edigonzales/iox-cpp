#!/usr/bin/env node

import { cp, mkdir, readFile, rm, stat, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

function required(options, name) {
  const value = options[name];
  if (!value) throw new Error(`--${name} is required`);
  return value;
}

function parseArgs(argv) {
  const options = {};
  for (let index = 0; index < argv.length; index += 1) {
    const key = argv[index]?.replace(/^--/, "");
    const value = argv[index + 1];
    if (!key || value === undefined || value.startsWith("--")) {
      throw new Error(`Expected --name value, received ${argv[index] ?? "<end>"}`);
    }
    options[key] = value;
    index += 1;
  }
  return options;
}

function validate(options) {
  if (!/^(snapshot|stable)$/.test(required(options, "channel"))) {
    throw new Error("--channel must be snapshot or stable");
  }
  if (!/^\d+\.\d+\.\d+(?:-snapshot\.g[0-9a-f]{12})?$/.test(required(options, "version"))) {
    throw new Error(`Invalid release version: ${options.version}`);
  }
  if (!/^[0-9a-f]{40}$/.test(required(options, "source-sha"))) {
    throw new Error("--source-sha must be a full lowercase commit SHA");
  }
  required(options, "release-manifest");
}

export async function stageRelease(options) {
  validate(options);
  const source = resolve(options.package ?? "packages/iox-wasm");
  const output = resolve(required(options, "output"));
  const packageDir = resolve(output, "package");
  await rm(output, { recursive: true, force: true });
  await mkdir(output, { recursive: true });
  await cp(source, packageDir, { recursive: true });
  for (const file of ["iox-wasm.mjs", "iox-wasm.wasm"]) {
    const path = resolve(packageDir, file);
    try {
      if (!(await stat(path)).isFile()) throw new Error("not a file");
    } catch {
      throw new Error(`WASM release file is missing: ${file}`);
    }
  }

  const manifestPath = resolve(packageDir, "package.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  if (manifest.name !== "@ilic/iox-wasm") {
    throw new Error(`Unexpected package name: ${manifest.name}`);
  }
  const baseVersion = manifest.version;
  manifest.version = options.version;
  manifest.gitHead = options["source-sha"];
  manifest.files = [...new Set([...(manifest.files ?? []), "interlis-release.json"])];
  await writeFile(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);

  const releaseManifest = JSON.parse(
    await readFile(resolve(options["release-manifest"]), "utf8"),
  );
  if (
    releaseManifest.project !== "iox-cpp" ||
    releaseManifest.artifactVersion !== options.version ||
    releaseManifest.sourceSha !== options["source-sha"] ||
    releaseManifest.versionKind !== options.channel
  ) {
    throw new Error("release manifest does not match the staged package contract");
  }
  if (baseVersion !== options.version.split("-", 1)[0]) {
    throw new Error(`Package base version ${baseVersion} does not match ${options.version}`);
  }
  const serializedManifest = `${JSON.stringify(releaseManifest, null, 2)}\n`;
  await writeFile(
    resolve(output, "interlis-release.json"),
    serializedManifest,
  );
  await writeFile(resolve(packageDir, "interlis-release.json"), serializedManifest);
  return releaseManifest;
}

const invokedPath = process.argv[1]
  ? pathToFileURL(resolve(process.argv[1])).href
  : "";
if (invokedPath === import.meta.url) {
  stageRelease(parseArgs(process.argv.slice(2))).catch((error) => {
    console.error(error instanceof Error ? error.message : String(error));
    process.exitCode = 1;
  });
}
