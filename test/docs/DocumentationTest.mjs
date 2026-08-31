import assert from "node:assert/strict";
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

const root = resolve(process.argv[2] ?? ".");
const skippedDirectories = new Set([
  "artifacts",
  "build",
  "dist",
  "node_modules",
]);
const retired = [
  "docs/roadmap.md",
  "docs/phase-status.md",
  "docs/build-und-publikationspipeline.md",
  "iox-cpp-llm-coding-spec.md",
  "AGENTS.md",
  ".agents/skills/",
];

function markdownFiles(directory) {
  if (!existsSync(directory)) return [];
  return readdirSync(directory).flatMap((name) => {
    if (skippedDirectories.has(name)) return [];
    const path = join(directory, name);
    return statSync(path).isDirectory()
      ? markdownFiles(path)
      : path.endsWith(".md")
        ? [path]
        : [];
  });
}

const files = [join(root, "README.md"), ...markdownFiles(join(root, "docs")), ...markdownFiles(join(root, "packages")), join(root, "vcpkg/README.md")];
for (const path of files) {
  const text = readFileSync(path, "utf8");
  assert.equal((text.match(/^```/gmu) ?? []).length % 2, 0, `${path}: offener Codeblock`);
  for (const name of retired) assert.ok(!text.includes(name), `${path}: Verweis auf entfernte Altlast ${name}`);
  const prose = text.replace(/^```[\s\S]*?^```/gmu, "");
  for (const match of prose.matchAll(/\[[^\]]*\]\(([^)]+)\)/gu)) {
    let target = match[1].trim();
    if (target.startsWith("<") && target.endsWith(">")) target = target.slice(1, -1);
    if (!target || /^(?:https?:|mailto:)/u.test(target)) continue;
    const local = target.split("#", 1)[0];
    if (local) assert.ok(existsSync(resolve(dirname(path), decodeURIComponent(local))), `${path}: defekter Link ${target}`);
  }
}
const release = readFileSync(join(root, "docs/release.md"), "utf8");
assert.match(release, /X\.Y\.Z-snapshot\.g<erste 12 Zeichen/u, "release.md: SHA-Snapshot-Vertrag fehlt");
assert.doesNotMatch(release, /-SNAPSHOT\.\d{14}/u, "release.md: veraltete Zeitstempel-Version");
console.log(`validated ${files.length} documentation pages`);
