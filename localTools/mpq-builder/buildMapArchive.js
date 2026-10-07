// The maps edited with Noggit (clientPatcher/maps, stored at their archive path) as one MPQ of their own: what the
// server's extractors read them from (localTools/mapEditing/rebuildServerMaps.ps1). The extractors only open the
// stock archive names (patch.MPQ to patch-5.MPQ), never our lettered patches; the client gets the same files in
// patch-Z (patchFiles.js).
//
// Usage: node buildMapArchive.js <maps folder> <output MPQ>
const fs = require('fs');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

const { addOptions } = require('./patchFiles');

const [root, outputPath] = process.argv.slice(2);
if (!root || !outputPath) {
    throw new Error('Usage: node buildMapArchive.js <maps folder> <output MPQ>');
}

const files = [];
const walk = (directory) => {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
        const source = path.join(directory, entry.name);
        if (entry.isDirectory()) {
            walk(source);
        } else if (!entry.name.startsWith('.')) {
            files.push({ source, archive: path.relative(root, source).split(path.sep).join('\\') });
        }
    }
};
walk(root);

if (fs.existsSync(outputPath)) {
    fs.unlinkSync(outputPath);
}
const archive = new Archive();
archive.create(outputPath, { maxFileCount: Math.max(64, files.length * 2), flags: 0 });
for (const file of files) {
    archive.addFile(file.source, file.archive, addOptions(file.archive));
}
archive.close();
console.log(`${files.length} map files packed into ${outputPath}`);
