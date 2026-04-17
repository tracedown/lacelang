#!/usr/bin/env node
// Validate every .laceext file under ../../extensions/ against laceext.schema.json.
// TOML → JSON → AJV.

const Ajv = require('ajv').default;
const addFormats = require('ajv-formats').default;
const TOML = require('@iarna/toml');
const fs = require('fs');
const path = require('path');

const SCHEMA_DIR = path.resolve(__dirname, '..', 'schemas');
const EXT_ROOT   = path.resolve(__dirname, '..', '..', 'extensions');

const ajv = new Ajv({ strict: false, allErrors: true });
addFormats(ajv);

for (const f of fs.readdirSync(SCHEMA_DIR).filter(f => f.endsWith('.json'))) {
  ajv.addSchema(JSON.parse(fs.readFileSync(path.join(SCHEMA_DIR, f), 'utf8')));
}

const validate = ajv.getSchema('https://lacelang.dev/schemas/laceext/1.0.0');
if (!validate) {
  console.error('FAIL: laceext schema not found in ../schemas/');
  process.exit(1);
}

function findLaceextFiles(root) {
  if (!fs.existsSync(root)) return [];
  const out = [];
  for (const entry of fs.readdirSync(root, { withFileTypes: true })) {
    const full = path.join(root, entry.name);
    if (entry.isDirectory()) out.push(...findLaceextFiles(full));
    else if (entry.name.endsWith('.laceext')) out.push(full);
  }
  return out;
}

const files = findLaceextFiles(EXT_ROOT);
if (files.length === 0) {
  console.log('  (no .laceext files under extensions/)');
  process.exit(0);
}

let allOk = true;
for (const f of files) {
  let parsed;
  try {
    parsed = TOML.parse(fs.readFileSync(f, 'utf8'));
  } catch (e) {
    console.error(`FAIL: ${path.relative(EXT_ROOT, f)}\n  TOML parse error: ${e.message}`);
    allOk = false;
    continue;
  }
  if (!validate(parsed)) {
    console.error(`FAIL: ${path.relative(EXT_ROOT, f)}\n  ${JSON.stringify(validate.errors, null, 2)}`);
    allOk = false;
  } else {
    console.log(`  ok: ${path.relative(EXT_ROOT, f)}`);
  }
}

process.exit(allOk ? 0 : 1);
