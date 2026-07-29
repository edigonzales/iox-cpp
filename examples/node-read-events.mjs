/**
 * Example: Read XTF events using @interlis/iox-wasm
 *
 * Usage: node node-read-events.mjs <input.xtf>
 *
 * Note: Requires the WASM module to be built first.
*/

import { createIoxModule, XtfReader } from '../packages/iox-wasm/index.js';
import { readFileSync } from 'node:fs';

async function main() {
    const args = process.argv.slice(2);
    if (args.length < 1) {
        console.error('Usage: node node-read-events.mjs <input.xtf>');
        process.exit(1);
    }

    const inputPath = args[0];
    const data = readFileSync(inputPath);

    console.error(`Reading ${data.length} bytes from ${inputPath}`);

    const mod = await createIoxModule();
    const reader = new XtfReader(mod, data);

    let count = 0;
    for (const event of reader) {
        ++count;
        console.log(`[${count}] ${event.event}`);
    }

    console.error(`\nTotal: ${count} events`);
}

main().catch(err => {
    console.error(err);
    process.exit(1);
});
