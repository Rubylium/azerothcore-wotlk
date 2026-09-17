const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

const { getPatchFiles } = require('./patchFiles');

const repoRoot = path.resolve(__dirname, '..', '..');
const outputPath = process.argv[2] || path.join(__dirname, 'patch-Z.MPQ');
const files = getPatchFiles(repoRoot);

for (const file of files) {
    if (!fs.existsSync(file.source)) {
        throw new Error(`Missing patch source: ${file.source}`);
    }
}

if (fs.existsSync(outputPath)) {
    fs.unlinkSync(outputPath);
}

const archive = new Archive();
archive.create(outputPath, { maxFileCount: Math.max(64, files.length * 2), flags: 0 });
for (const file of files) {
    archive.addFile(file.source, file.archive);
}
archive.close();

const verification = Archive.open(outputPath);
const results = [];
try {
    for (const file of files) {
        if (!verification.hasFile(file.archive)) {
            throw new Error(`MPQ is missing ${file.archive}`);
        }
        const archivedFile = verification.openFile(file.archive);
        try {
            const archivedData = archivedFile.readAll();
            const sourceData = fs.readFileSync(file.source);
            const archivedHash = crypto.createHash('sha256').update(archivedData).digest('hex');
            const sourceHash = crypto.createHash('sha256').update(sourceData).digest('hex');
            if (archivedHash !== sourceHash) {
                throw new Error(`Hash mismatch for ${file.archive}`);
            }
            results.push({ path: file.archive, bytes: archivedData.length, sha256: archivedHash });
        } finally {
            archivedFile.close();
        }
    }
} finally {
    verification.close();
}

console.log(JSON.stringify({ outputPath, files: results }, null, 2));
