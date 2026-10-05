// Extracts files from the game client's own archives for localTools/audio/buildAudio.py ("client:" sources): each
// from the highest-priority archive that holds it - the lettered patches, the locale's patches, the base patches,
// then the locale's speech and locale archives (the voices: a French client speaks French), then the base ones.
//
//   node clientFiles.js <list.json> <outDir> [--client <path>]
// Writes each found file under outDir at its archive path, and outDir/missing.json (those found nowhere).
const fs = require('fs');
const path = require('path');
const { Archive, MPQ_OPEN_READ_ONLY } = require('../mpq-builder/node_modules/@jamiephan/stormlib');

const args = process.argv.slice(2);
const clientIndex = args.indexOf('--client');
const client = clientIndex >= 0 ? args[clientIndex + 1] : 'C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK';
const [listPath, outDir] = args.filter((_, index) => clientIndex < 0 || (index !== clientIndex &&
    index !== clientIndex + 1));
if (!listPath || !outDir) {
    console.error('usage: node clientFiles.js <list.json> <outDir> [--client <path>]');
    process.exit(2);
}

const data = path.join(client, 'Data');
const locale = fs.readdirSync(data).find((name) => /^[a-z]{2}[A-Z]{2}$/.test(name)
    && fs.existsSync(path.join(data, name, `locale-${name}.MPQ`)));
const localeData = path.join(data, locale);
const list = (directory, pattern) => fs.readdirSync(directory).filter((name) => pattern.test(name))
    .sort((a, b) => b.toLowerCase().localeCompare(a.toLowerCase())).map((name) => path.join(directory, name));

// Highest priority first
const archives = [
    ...list(data, /^patch-[a-z]\.mpq$/i),
    ...list(localeData, new RegExp(`^patch-${locale}-[a-z]\\.mpq$`, 'i')),
    ...list(localeData, new RegExp(`^patch-${locale}-[2-9]\\.mpq$`, 'i')),
    path.join(localeData, `patch-${locale}.MPQ`),
    ...list(data, /^patch-[2-9]\.mpq$/i),
    path.join(data, 'patch.MPQ'),
    path.join(localeData, `lichking-speech-${locale}.MPQ`),
    path.join(localeData, `expansion-speech-${locale}.MPQ`),
    path.join(localeData, `speech-${locale}.MPQ`),
    path.join(localeData, `lichking-locale-${locale}.MPQ`),
    path.join(localeData, `expansion-locale-${locale}.MPQ`),
    path.join(localeData, `locale-${locale}.MPQ`),
    path.join(data, 'lichking.MPQ'),
    path.join(data, 'expansion.MPQ'),
    path.join(data, 'common-2.MPQ'),
    path.join(data, 'common.MPQ'),
].filter((file) => fs.existsSync(file));

const wanted = JSON.parse(fs.readFileSync(listPath, 'utf8'));
const pending = new Set(wanted);
for (const archivePath of archives) {
    if (!pending.size) {
        break;
    }
    const archive = Archive.open(archivePath, MPQ_OPEN_READ_ONLY);
    try {
        for (const file of [...pending]) {
            if (!archive.hasFile(file)) {
                continue;
            }
            const target = path.join(outDir, ...file.split('\\'));
            fs.mkdirSync(path.dirname(target), { recursive: true });
            fs.writeFileSync(target, Buffer.from(archive.readFile(file)));
            pending.delete(file);
        }
    } finally {
        archive.close();
    }
}
fs.mkdirSync(outDir, { recursive: true });
fs.writeFileSync(path.join(outDir, 'missing.json'), JSON.stringify([...pending], null, 2));
