const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

const { addOptions, getPatchFiles, packOf, PACKS } = require('./patchFiles');
const { archiveCache } = require('./archiveCache');

// Builds our base patches, patch-X, -Y and -Z.MPQ (patchFiles.js packOf), each cached on its own files: a spell edit
// repacks patch-Z alone, a new ground indicator patch-Y alone.
// Usage: node buildPatch.js [<output folder>] [--verify]
// --verify reads every file back out of a rebuilt archive and compares its bytes (seconds per hundred MB); without
// it, a rebuilt archive is only checked to list every file.
const argv = process.argv.slice(2);
const verify = argv.includes('--verify');
const outputRoot = argv.find((arg) => !arg.startsWith('--')) || __dirname;

const repoRoot = path.resolve(__dirname, '..', '..');
const allFiles = getPatchFiles(repoRoot);
for (const file of allFiles) {
    if (!fs.existsSync(file.source)) {
        throw new Error(`Missing patch source: ${file.source}`);
    }
}

function buildArchive(outputPath, files) {
    const cache = archiveCache(outputPath, files, __filename);
    if (cache.current) {
        console.log(`Cached ${path.basename(outputPath)} (${files.length} files)`);
        return;
    }
    const started = Date.now();
    if (fs.existsSync(outputPath)) {
        fs.unlinkSync(outputPath);
    }
    const archive = new Archive();
    archive.create(outputPath, { maxFileCount: Math.max(64, files.length * 2), flags: 0 });
    for (const file of files) {
        archive.addFile(file.source, file.archive, addOptions(file.archive));
    }
    archive.close();

    const check = Archive.open(outputPath);
    try {
        for (const file of files) {
            if (!check.hasFile(file.archive)) {
                throw new Error(`${path.basename(outputPath)} is missing ${file.archive}`);
            }
            if (!verify) {
                continue;
            }
            const archived = check.openFile(file.archive);
            try {
                const archivedHash = crypto.createHash('sha256').update(archived.readAll()).digest('hex');
                const sourceHash = crypto.createHash('sha256').update(fs.readFileSync(file.source)).digest('hex');
                if (archivedHash !== sourceHash) {
                    throw new Error(`Hash mismatch for ${file.archive}`);
                }
            } finally {
                archived.close();
            }
        }
    } finally {
        check.close();
    }
    const seconds = ((Date.now() - started) / 1000).toFixed(1);
    console.log(`Built ${path.basename(outputPath)} (${files.length} files, ${seconds}s)`);
    cache.save();
}

fs.mkdirSync(outputRoot, { recursive: true });
for (const pack of PACKS) {
    buildArchive(path.join(outputRoot, `patch-${pack}.MPQ`), allFiles.filter((file) => packOf(file) === pack));
}
