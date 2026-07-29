/**
 * Smoke test for @interlis/iox-wasm Phase 0 stub.
 */
import { createIoxModule } from '../index.js';
import { strict as assert } from 'node:assert';
import { test } from 'node:test';

test('createIoxModule returns stub module', async () => {
  const mod = await createIoxModule();
  assert.ok(mod);
  assert.equal(mod.abiVersion(), 1);
  assert.equal(mod.version(), '0.1.0');
});

test('createIoxModule accepts options', async () => {
  const mod = await createIoxModule({ locateFile: (p) => p });
  assert.ok(mod);
  assert.equal(mod.abiVersion(), 1);
});
