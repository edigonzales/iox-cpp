/**
 * Smoke tests for @interlis/iox-wasm module initialization.
 */
import { createIoxModule } from '../index.js';
import { strict as assert } from 'node:assert';
import { test } from 'node:test';

test('createIoxModule returns the ABI module', async () => {
  const mod = await createIoxModule();
  assert.ok(mod);
  assert.equal(mod.abiVersion(), 2);
  assert.equal(mod.version(), '0.2.0');
});

test('createIoxModule accepts options', async () => {
  const mod = await createIoxModule({ locateFile: (p) => p });
  assert.ok(mod);
  assert.equal(mod.abiVersion(), 2);
});
