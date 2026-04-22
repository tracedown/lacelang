#!/usr/bin/env node
/**
 * Bump the spec version across VERSION file and all conformance vectors.
 *
 * Usage:
 *   node testkit/bump-version.js          # patch bump (0.9.1 -> 0.9.2)
 *   node testkit/bump-version.js 1.0.0    # explicit new version
 */
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const VERSION_FILE = path.join(ROOT, "VERSION");
const VECTORS_DIR = path.join(__dirname, "vectors");

const oldVersion = fs.readFileSync(VERSION_FILE, "utf8").trim();

const arg = process.argv[2];

if (arg === "--help" || arg === "-h") {
  console.log("Usage: node testkit/bump-version.js [new-version]");
  console.log("  No args: patch bump (e.g. 0.9.1 -> 0.9.2)");
  console.log("  With arg: set explicit version (e.g. 1.0.0)");
  process.exit(0);
}

let newVersion;
if (arg) {
  if (!/^\d+\.\d+\.\d+$/.test(arg)) {
    console.error(`Invalid version "${arg}" — must be semver (X.Y.Z)`);
    process.exit(1);
  }
  newVersion = arg;
} else {
  const parts = oldVersion.split(".").map(Number);
  if (parts.length !== 3 || parts.some(isNaN)) {
    console.error(`Cannot parse VERSION "${oldVersion}" as semver`);
    process.exit(1);
  }
  parts[2]++;
  newVersion = parts.join(".");
}

if (newVersion === oldVersion) {
  console.error(`New version "${newVersion}" is the same as current`);
  process.exit(1);
}

console.log(`${oldVersion} -> ${newVersion}`);

// 1. Update VERSION file
fs.writeFileSync(VERSION_FILE, newVersion + "\n", "utf8");
console.log(`  VERSION file updated`);

// 2. Update all conformance vectors
let updated = 0;
let skipped = 0;

function walk(dir) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walk(full);
    } else if (entry.name.endsWith(".json")) {
      const content = fs.readFileSync(full, "utf8");
      if (!content.includes(`"${oldVersion}"`)) {
        skipped++;
        continue;
      }
      try {
        const obj = JSON.parse(content);
        let changed = false;

        // AST version in expected parse output
        if (obj.expected?.ast?.version === oldVersion) {
          obj.expected.ast.version = newVersion;
          changed = true;
        }

        // specVersion at vector root (if present)
        if (obj.specVersion === oldVersion) {
          obj.specVersion = newVersion;
          changed = true;
        }

        if (changed) {
          fs.writeFileSync(full, JSON.stringify(obj, null, 2) + "\n", "utf8");
          updated++;
        } else {
          skipped++;
        }
      } catch {
        skipped++;
      }
    }
  }
}

walk(VECTORS_DIR);
console.log(`  Vectors: ${updated} updated, ${skipped} unchanged`);
