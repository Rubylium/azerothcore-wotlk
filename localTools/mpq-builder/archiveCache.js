const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const cacheRoot = path.resolve(__dirname, '../../clientPatcher/.build/cache/mpq');
const hash = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');
const fileHash = (file) => hash(fs.readFileSync(file));

function archiveCache(outputPath, files, builderPath) {
    const statePath = path.join(cacheRoot, `${hash(path.resolve(outputPath))}.json`);
    const inputs = hash(JSON.stringify({
        version: 1,
        tools: [__filename, builderPath, path.join(__dirname, 'patchFiles.js'),
            path.join(__dirname, 'package-lock.json')].map(fileHash),
        node: process.version,
        files: files.map((file) => [file.archive, fileHash(file.source)]),
    }));
    let state;
    try { state = JSON.parse(fs.readFileSync(statePath, 'utf8')); } catch { /* First build or invalid cache. */ }
    const current = process.env.EVOLUTIONS_FORCE_REBUILD !== '1' && state && state.inputs === inputs
        && fs.existsSync(outputPath) && state.output === fileHash(outputPath);
    return {
        current: Boolean(current),
        save() {
            fs.mkdirSync(cacheRoot, { recursive: true });
            fs.writeFileSync(`${statePath}.tmp`, JSON.stringify({ inputs, output: fileHash(outputPath) }));
            fs.renameSync(`${statePath}.tmp`, statePath);
        },
    };
}

module.exports = { archiveCache };
