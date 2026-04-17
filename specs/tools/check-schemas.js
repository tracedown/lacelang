#!/usr/bin/env node
// Compile every JSON Schema in ../schemas/ and report failures.
// Cross-refs by $id resolve because we addSchema() everything before compiling.

const Ajv = require('ajv').default;
const addFormats = require('ajv-formats').default;
const fs = require('fs');
const path = require('path');

const SCHEMA_DIR = path.resolve(__dirname, '..', 'schemas');

const ajv = new Ajv({ strict: false, allErrors: true });
addFormats(ajv);

const files = fs.readdirSync(SCHEMA_DIR).filter(f => f.endsWith('.json'));
if (files.length === 0) {
  console.error(`No schemas found in ${SCHEMA_DIR}`);
  process.exit(1);
}

const schemas = {};
for (const f of files) {
  schemas[f] = JSON.parse(fs.readFileSync(path.join(SCHEMA_DIR, f), 'utf8'));
  ajv.addSchema(schemas[f]);
}

let allOk = true;
for (const f of files) {
  try {
    ajv.compile(schemas[f]);
    console.log(`  ok: ${f}`);
  } catch (e) {
    console.error(`FAIL: ${f}\n  ${e.message}`);
    allOk = false;
  }
}

// Also validate the error-codes registry (sibling of schemas/, not itself a schema).
const errorCodesPath = path.resolve(__dirname, '..', 'error-codes.json');
if (fs.existsSync(errorCodesPath)) {
  const data = JSON.parse(fs.readFileSync(errorCodesPath, 'utf8'));
  if (!Array.isArray(data.codes) || data.codes.length === 0) {
    console.error('FAIL: error-codes.json has no codes array');
    allOk = false;
  } else {
    const codes = data.codes.map(c => c.code);
    const dups = codes.filter((c, i, a) => a.indexOf(c) !== i);
    if (dups.length) {
      console.error(`FAIL: error-codes.json has duplicate codes: ${[...new Set(dups)].join(', ')}`);
      allOk = false;
    } else {
      const required = ['code', 'severity', 'blocks_activation', 'spec', 'description'];
      const bad = data.codes.find(c => required.some(k => !(k in c)));
      if (bad) {
        console.error(`FAIL: error-codes.json entry missing required key: ${JSON.stringify(bad)}`);
        allOk = false;
      } else {
        console.log(`  ok: error-codes.json (${data.codes.length} codes)`);
      }
    }
  }
}

process.exit(allOk ? 0 : 1);
