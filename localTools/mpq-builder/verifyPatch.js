const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

const { getPatchFiles } = require('./patchFiles');

const repoRoot = path.resolve(__dirname, '..', '..');
const archivePath = process.argv[2];
if (!archivePath || !fs.existsSync(archivePath)) {
    throw new Error(`Patch MPQ not found: ${archivePath || '<missing argument>'}`);
}

const expected = getPatchFiles(repoRoot).map((file) => ({ path: file.archive, source: file.source }));
const archive = Archive.open(archivePath);
const results = [];
try {
    for (const entry of expected) {
        if (!archive.hasFile(entry.path)) {
            throw new Error(`Missing archive entry: ${entry.path}`);
        }
        const file = archive.openFile(entry.path);
        try {
            const archivedData = file.readAll();
            const sourceData = fs.readFileSync(entry.source);
            const archivedHash = crypto.createHash('sha256').update(archivedData).digest('hex');
            const sourceHash = crypto.createHash('sha256').update(sourceData).digest('hex');
            if (archivedHash !== sourceHash) {
                throw new Error(`Hash mismatch: ${entry.path}`);
            }
            results.push({ path: entry.path, bytes: archivedData.length, sha256: archivedHash });
        } finally {
            file.close();
        }
    }
} finally {
    archive.close();
}
console.log(JSON.stringify({ archivePath, files: results }, null, 2));
