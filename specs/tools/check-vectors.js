#!/usr/bin/env node
// Validate every conformance vector under ../../testkit/vectors/ against
// the conformance-vector schema, plus cross-check that all referenced
// error codes exist in the canonical registry.

const Ajv = require('ajv').default;
const addFormats = require('ajv-formats').default;
const fs = require('fs');
const path = require('path');

const SCHEMA_DIR = path.resolve(__dirname, '..', 'schemas');
const VEC_ROOT   = path.resolve(__dirname, '..', '..', 'testkit', 'vectors');
const ERROR_CODES = path.resolve(__dirname, '..', 'error-codes.json');

const ajv = new Ajv({ strict: false, allErrors: true });
addFormats(ajv);

for (const f of fs.readdirSync(SCHEMA_DIR).filter(f => f.endsWith('.json'))) {
  ajv.addSchema(JSON.parse(fs.readFileSync(path.join(SCHEMA_DIR, f), 'utf8')));
}

const validate = ajv.getSchema('https://lacelang.dev/schemas/conformance-vector/1.0.0');
if (!validate) {
  console.error('FAIL: conformance-vector schema not found');
  process.exit(1);
}

const validCodes = new Set(JSON.parse(fs.readFileSync(ERROR_CODES, 'utf8')).codes.map(c => c.code));

function findVectors(root) {
  if (!fs.existsSync(root)) return [];
  const out = [];
  for (const entry of fs.readdirSync(root, { withFileTypes: true })) {
    const full = path.join(root, entry.name);
    if (entry.isDirectory()) out.push(...findVectors(full));
    else if (entry.name.endsWith('.json')) out.push(full);
  }
  return out;
}

function collectErrorCodes(vector) {
  const codes = [];
  if (vector.expected) {
    for (const k of ['errors', 'warnings']) {
      if (Array.isArray(vector.expected[k])) {
        for (const e of vector.expected[k]) if (e.code) codes.push(e.code);
      }
    }
  }
  return codes;
}

const files = findVectors(VEC_ROOT);
if (files.length === 0) {
  console.log('  (no vectors)');
  process.exit(0);
}

let allOk = true;
for (const f of files) {
  const rel = path.relative(VEC_ROOT, f);
  let v;
  try {
    v = JSON.parse(fs.readFileSync(f, 'utf8'));
  } catch (e) {
    console.error(`FAIL: ${rel} (JSON parse: ${e.message})`);
    allOk = false;
    continue;
  }

  if (!validate(v)) {
    console.error(`FAIL: ${rel}\n  ${JSON.stringify(validate.errors, null, 2)}`);
    allOk = false;
    continue;
  }

  // Cross-check error codes are in the canonical registry.
  const used = collectErrorCodes(v);
  const unknown = used.filter(c => !validCodes.has(c));
  if (unknown.length) {
    console.error(`FAIL: ${rel} references unknown error codes: ${unknown.join(', ')}`);
    allOk = false;
    continue;
  }

  console.log(`  ok: ${rel}`);
}

process.exit(allOk ? 0 : 1);
