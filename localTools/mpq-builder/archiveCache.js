const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const cacheRoot = path.resolve(__dirname, '../../clientPatcher/.build/cache/mpq');
const hash = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');

// A file's SHA-256, remembered with its size and write time (as the PowerShell side's file-hashes.tsv): a cache check
// read every source whole each run - the interface's 370 MB and the archive itself. As git does, a hash taken within
// RACY_MS of the file's last write is not trusted again (rewritten in that moment at the same size, it keeps its time).
const RACY_MS = 2000;
const statePath = path.join(cacheRoot, 'file-hashes.json');
let known;
let dirty = false;
function knownHashes() {
    if (!known) {
        try { known = JSON.parse(fs.readFileSync(statePath, 'utf8')); } catch { known = {}; }
        process.on('exit', saveHashes);
    }
    return known;
}
function saveHashes() {
    if (!dirty) {
        return;
    }
    fs.mkdirSync(cacheRoot, { recursive: true });
    for (const file of Object.keys(known)) {
        if (!fs.existsSync(file)) {
            delete known[file];
        }
    }
    fs.writeFileSync(`${statePath}.tmp`, JSON.stringify(known));
    fs.renameSync(`${statePath}.tmp`, statePath);
    dirty = false;
}
function fileHash(file) {
    const full = path.resolve(file);
    const stat = fs.statSync(full);
    const entry = knownHashes()[full];
    if (entry && entry[0] === stat.size && entry[1] === stat.mtimeMs && entry[3] - stat.mtimeMs >= RACY_MS) {
        return entry[2];
    }
    const value = hash(fs.readFileSync(full));
    known[full] = [stat.size, stat.mtimeMs, value, Date.now()];
    dirty = true;
    return value;
}

// files: { source, archive } (or { key, archive }: a value standing for content not on disk); identity: what the record
// is kept under (another check of the same archive, made before its files are known, uses its own)
function archiveCache(outputPath, files, builderPath, extra = '', identity = path.resolve(outputPath)) {
    const archiveState = path.join(cacheRoot, `${hash(identity)}.json`);
    const inputs = hash(JSON.stringify({
        version: 1,
        tools: [__filename, builderPath, path.join(__dirname, 'patchFiles.js'),
            path.join(__dirname, 'package-lock.json')].map(fileHash),
        node: process.version,
        extra,
        files: files.map((file) => [file.archive, file.source ? fileHash(file.source) : file.key]),
    }));
    let state;
    try { state = JSON.parse(fs.readFileSync(archiveState, 'utf8')); } catch { /* First build or invalid cache. */ }
    const current = process.env.EVOLUTIONS_FORCE_REBUILD !== '1' && state && state.inputs === inputs
        && fs.existsSync(outputPath) && state.output === fileHash(outputPath);
    return {
        current: Boolean(current),
        save() {
            fs.mkdirSync(cacheRoot, { recursive: true });
            fs.writeFileSync(`${archiveState}.tmp`, JSON.stringify({ inputs, output: fileHash(outputPath) }));
            fs.renameSync(`${archiveState}.tmp`, archiveState);
            saveHashes();
        },
    };
}

module.exports = { archiveCache, fileHash };
