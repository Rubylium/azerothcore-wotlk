// Extracts files from the game client's archives, the highest-priority copy of each (patches over the base game,
// the locale archives over the common ones, our own patch-Z last).
//
//   node extractClientFiles.js <list.json> <outDir> [--client <path>]
//
// list.json is an array of archive paths (backslashes, case as the game writes them). Each is written under outDir at
// the same relative path. A file found nowhere is reported in missing.json in outDir rather than failing the run,
// because a model may name optional companions (a .skin or .anim) that only some of them have.
const fs = require('fs');
const path = require('path');
const { Archive, MPQ_OPEN_READ_ONLY } = require('@jamiephan/stormlib');

const args = process.argv.slice(2);
const clientIndex = args.indexOf('--client');
const client = clientIndex >= 0 ? args[clientIndex + 1] : 'C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK';
const [listPath, outDir] = args.filter((_, index) => clientIndex < 0 || (index !== clientIndex && index !== clientIndex + 1));
if (!listPath || !outDir) {
    console.error('usage: node extractClientFiles.js <list.json> <outDir> [--client <path>]');
    process.exit(2);
}

const data = path.join(client, 'Data');
const locale = fs.readdirSync(data).find((name) => /^[a-z]{2}[A-Z]{2}$/.test(name)
    && fs.existsSync(path.join(data, name, `locale-${name}.MPQ`)));
// Lowest priority first: a later archive's copy replaces an earlier one's
const archives = ['common.MPQ', 'common-2.MPQ', 'expansion.MPQ', 'lichking.MPQ', 'patch.MPQ', 'patch-2.MPQ',
    'patch-3.MPQ', `${locale}/locale-${locale}.MPQ`, `${locale}/expansion-locale-${locale}.MPQ`,
    `${locale}/lichking-locale-${locale}.MPQ`, `${locale}/patch-${locale}.MPQ`, `${locale}/patch-${locale}-2.MPQ`,
    `${locale}/patch-${locale}-3.MPQ`, 'patch-Z.MPQ']
    .map((name) => path.join(data, name))
    .filter((file) => fs.existsSync(file));

const wanted = JSON.parse(fs.readFileSync(listPath, 'utf8'));
const found = new Map();
for (const archivePath of archives) {
    const archive = Archive.open(archivePath, MPQ_OPEN_READ_ONLY);
    try {
        for (const file of wanted) {
            if (archive.hasFile(file)) {
                found.set(file, archive.readFile(file));
            }
        }
    } finally {
        archive.close();
    }
}

const missing = [];
for (const file of wanted) {
    const bytes = found.get(file);
    if (!bytes) {
        missing.push(file);
        continue;
    }
    const target = path.join(outDir, ...file.split('\\'));
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, bytes);
}
fs.mkdirSync(outDir, { recursive: true });
fs.writeFileSync(path.join(outDir, 'missing.json'), JSON.stringify(missing, null, 2));
console.log(`extracted ${found.size} of ${wanted.length} files${missing.length ? `, ${missing.length} missing` : ''}`);
