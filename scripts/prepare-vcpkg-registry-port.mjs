#!/usr/bin/env node

import { cp, readFile, rm, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

function replaceSingleLine(text, pattern, replacement, label) {
  const matches = [...text.matchAll(pattern)];
  if (matches.length !== 1) {
    throw new Error(`Expected exactly one ${label} line, found ${matches.length}`);
  }
  return text.replace(pattern, replacement);
}

export function validateRegistryVersion(version, sourceSha) {
  if (!/^[0-9a-f]{40}$/.test(sourceSha ?? "")) {
    throw new Error("source SHA must be a lowercase 40-character Git SHA");
  }

  if (/^\d+\.\d+\.\d+$/.test(version ?? "")) {
    return { kind: "stable", version };
  }

  const snapshot = /^(\d+\.\d+\.\d+)-snapshot\.g([0-9a-f]{12})$/.exec(
    version ?? "",
  );
  if (!snapshot) {
    throw new Error(
      "vcpkg version must be X.Y.Z or X.Y.Z-snapshot.g<12-character-source-sha>",
    );
  }
  if (snapshot[2] !== sourceSha.slice(0, 12)) {
    throw new Error(
      `snapshot suffix ${snapshot[2]} does not match source SHA ${sourceSha.slice(0, 12)}`,
    );
  }
  return { kind: "snapshot", version, baseVersion: snapshot[1] };
}

export function rewritePortfile(portfileText, sourceSha, sha512) {
  if (!/^[0-9a-f]{40}$/.test(sourceSha ?? "")) {
    throw new Error("source SHA must be a lowercase 40-character Git SHA");
  }
  if (!/^[0-9a-f]{128}$/.test(sha512 ?? "")) {
    throw new Error("SHA512 must be a lowercase 128-character hexadecimal digest");
  }

  const withRef = replaceSingleLine(
    portfileText,
    /^(\s*)REF\s+\S+\s*$/gm,
    `$1REF ${sourceSha}`,
    "REF",
  );
  return replaceSingleLine(
    withRef,
    /^(\s*)SHA512\s+\S+\s*$/gm,
    `$1SHA512 ${sha512}`,
    "SHA512",
  );
}

export function rewriteManifest(manifestText, version, sourceSha) {
  validateRegistryVersion(version, sourceSha);
  const manifest = JSON.parse(manifestText);
  if (manifest.name !== "iox-cpp") {
    throw new Error(`Expected iox-cpp port manifest, got ${String(manifest.name)}`);
  }

  for (const field of ["version", "version-semver", "version-date"]) {
    delete manifest[field];
  }
  manifest["version-string"] = version;
  return `${JSON.stringify(manifest, null, 2)}\n`;
}

export async function prepareRegistryPort({
  templateDir,
  outputDir,
  version,
  sourceSha,
  sha512,
}) {
  templateDir = resolve(templateDir);
  outputDir = resolve(outputDir);
  validateRegistryVersion(version, sourceSha);

  await rm(outputDir, { recursive: true, force: true });
  await cp(templateDir, outputDir, { recursive: true });

  const portfilePath = resolve(outputDir, "portfile.cmake");
  const manifestPath = resolve(outputDir, "vcpkg.json");
  const [portfileText, manifestText] = await Promise.all([
    readFile(portfilePath, "utf8"),
    readFile(manifestPath, "utf8"),
  ]);

  await Promise.all([
    writeFile(portfilePath, rewritePortfile(portfileText, sourceSha, sha512)),
    writeFile(manifestPath, rewriteManifest(manifestText, version, sourceSha)),
  ]);
}

function parseArguments(argv) {
  const result = {};
  const supported = new Map([
    ["--template-dir", "templateDir"],
    ["--output-dir", "outputDir"],
    ["--version", "version"],
    ["--source-sha", "sourceSha"],
    ["--sha512", "sha512"],
  ]);

  for (let index = 0; index < argv.length; index += 2) {
    const argument = argv[index];
    const key = supported.get(argument);
    const value = argv[index + 1];
    if (!key || !value) {
      throw new Error(`Invalid or incomplete argument ${String(argument)}`);
    }
    result[key] = value;
  }

  for (const key of supported.values()) {
    if (!result[key]) throw new Error(`Missing required argument ${key}`);
  }
  return result;
}

async function main() {
  await prepareRegistryPort(parseArguments(process.argv.slice(2)));
}

const invokedPath = process.argv[1]
  ? pathToFileURL(resolve(process.argv[1])).href
  : "";
if (invokedPath === import.meta.url) {
  main().catch((error) => {
    console.error(error instanceof Error ? error.message : String(error));
    process.exitCode = 1;
  });
}
