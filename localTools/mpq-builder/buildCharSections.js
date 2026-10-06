// Rebuilds server/Data/dbc/client-only/CharSections.dbc from the CharSections the game
// loads before patch-Z (Patch-D on this client). Hair rows flagged for only one of
// character creation (0x01) or the barber (0x02) get both bits. Death knight (0x04)
// and mundane (0x10) stay. The create screen was skipping the barber-only styles.
//
//   node buildCharSections.js [client path]
const fs = require('fs');
const path = require('path');
const { Archive, MPQ_OPEN_READ_ONLY } = require('@jamiephan/stormlib');

const repoRoot = path.resolve(__dirname, '..', '..');
const client = process.argv[2] || 'C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK';
const outFile = path.join(repoRoot, 'server', 'Data', 'dbc', 'client-only', 'CharSections.dbc');
const wanted = 'DBFilesClient\\CharSections.dbc';
const HAIR = 3;
const CREATE = 0x01;
const BARBER = 0x02;

const data = path.join(client, 'Data');
const locale = fs.readdirSync(data).find((name) => /^[a-z]{2}[A-Z]{2}$/.test(name)
    && fs.existsSync(path.join(data, name, `locale-${name}.MPQ`)));
const list = (directory, pattern) => fs.readdirSync(directory).filter((name) => pattern.test(name))
    .sort((a, b) => b.toLowerCase().localeCompare(a.toLowerCase())).map((name) => path.join(directory, name));
const archives = [
    ...list(data, /^patch-[a-y]\.mpq$/i),
    ...list(path.join(data, locale), new RegExp(`^patch-${locale}-[a-z]\\.mpq$`, 'i')),
    ...list(path.join(data, locale), new RegExp(`^patch-${locale}-[2-9]\\.mpq$`, 'i')),
    path.join(data, locale, `patch-${locale}.MPQ`),
    ...list(data, /^patch-[2-9]\.mpq$/i),
    path.join(data, 'patch.MPQ'),
    path.join(data, locale, `lichking-locale-${locale}.MPQ`),
    path.join(data, locale, `expansion-locale-${locale}.MPQ`),
    path.join(data, locale, `locale-${locale}.MPQ`),
    path.join(data, 'lichking.MPQ'),
    path.join(data, 'expansion.MPQ'),
    path.join(data, 'common-2.MPQ'),
    path.join(data, 'common.MPQ'),
].filter((file) => fs.existsSync(file));

let source = null;
let buf = null;
for (const archivePath of archives) {
    const archive = Archive.open(archivePath, MPQ_OPEN_READ_ONLY);
    try {
        if (archive.hasFile(wanted)) {
            buf = Buffer.from(archive.readFile(wanted));
            source = archivePath;
            break;
        }
    } finally {
        archive.close();
    }
}
if (!buf) {
    throw new Error(`${wanted} is in none of the client archives`);
}
if (buf.toString('ascii', 0, 4) !== 'WDBC' || buf.readUInt32LE(8) !== 10 || buf.readUInt32LE(12) !== 40) {
    throw new Error(`Unexpected CharSections layout in ${source}`);
}

const records = buf.readUInt32LE(4);
const recSize = 40;
const styles = (race, gender, flag) => {
    const ids = new Set();
    for (let row = 0; row < records; row++) {
        const base = 20 + row * recSize;
        if (buf.readUInt32LE(base + 4) !== race || buf.readUInt32LE(base + 8) !== gender) continue;
        if (buf.readUInt32LE(base + 12) !== HAIR) continue;
        if ((buf.readUInt32LE(base + 28) & flag) === 0) continue;
        ids.add(buf.readUInt32LE(base + 32));
    }
    return [...ids].sort((a, b) => a - b);
};

const before = {
    male: styles(3, 0, CREATE),
    female: styles(3, 1, CREATE),
};

let changed = 0;
for (let row = 0; row < records; row++) {
    const base = 20 + row * recSize;
    if (buf.readUInt32LE(base + 12) !== HAIR) continue;
    const flags = buf.readUInt32LE(base + 28);
    if ((flags & (CREATE | BARBER)) === 0) continue;
    const next = flags | CREATE | BARBER;
    if (next === flags) continue;
    buf.writeUInt32LE(next, base + 28);
    changed++;
}

const after = {
    male: styles(3, 0, CREATE),
    female: styles(3, 1, CREATE),
};

fs.mkdirSync(path.dirname(outFile), { recursive: true });
fs.writeFileSync(outFile, buf);
console.log(`CharSections <- ${path.relative(data, source)}`);
console.log(`Updated ${changed} hair rows -> ${outFile}`);
console.log(`Dwarf male create ${before.male.length} -> ${after.male.length}: ${after.male.join(',')}`);
console.log(`Dwarf female create ${before.female.length} -> ${after.female.length}: ${after.female.join(',')}`);
