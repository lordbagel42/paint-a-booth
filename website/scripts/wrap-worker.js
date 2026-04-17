import { readFileSync, writeFileSync } from 'node:fs';

const path = '.svelte-kit/cloudflare/_worker.js';
let content = readFileSync(path, 'utf-8');
content += "\nexport { BoothRoom } from '../../src/durable-objects/booth-room.js';\n";
writeFileSync(path, content);
console.log('wrap-worker: exported BoothRoom from _worker.js');
