// Extracts the copy of one file the game client actually reads, before our own base patches: the highest-priority
// archive that holds it. 3.3.5 ranks the lettered base patches (Data/patch-?.MPQ, later letters first) above the
// locale patches, which rank above the numbered base patches and the base game; ours (patch-X, -Y and -Z) are skipped.
//
//   node extractEffectiveClientFile.js <archive path> <outFile> [--client <path>]
//
// Example: DBFilesClient\ItemDisplayInfo.dbc comes from the client image's Patch-D (HD weapon remakes), not
// from the locale patches, so building a new ItemDisplayInfo.dbc on the stock copy would undo those.
const fs = require('fs');
const path = require('path');
const { Archive, MPQ_OPEN_READ_ONLY } = require('@jamiephan/stormlib');

const args = process.argv.slice(2);
const clientIndex = args.indexOf('--client');
const client = clientIndex >= 0 ? args[clientIndex + 1] : 'C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK';
const [wanted, outFile] = args.filter((_, index) => clientIndex < 0
    || (index !== clientIndex && index !== clientIndex + 1));
if (!wanted || !outFile) {
    console.error('usage: node extractEffectiveClientFile.js <archive path> <outFile> [--client <path>]');
    process.exit(2);
}

const data = path.join(client, 'Data');
const locale = fs.readdirSync(data).find((name) => /^[a-z]{2}[A-Z]{2}$/.test(name)
    && fs.existsSync(path.join(data, name, `locale-${name}.MPQ`)));
const list = (directory, pattern) => fs.readdirSync(directory).filter((name) => pattern.test(name))
    .sort((a, b) => b.toLowerCase().localeCompare(a.toLowerCase())).map((name) => path.join(directory, name));

// Highest priority first
const archives = [
    ...list(data, /^patch-[a-w]\.mpq$/i),
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

for (const archivePath of archives) {
    const archive = Archive.open(archivePath, MPQ_OPEN_READ_ONLY);
    try {
        if (archive.hasFile(wanted)) {
            fs.mkdirSync(path.dirname(path.resolve(outFile)), { recursive: true });
            fs.writeFileSync(outFile, archive.readFile(wanted));
            console.log(`${wanted} <- ${path.relative(data, archivePath)}`);
            process.exit(0);
        }
    } finally {
        archive.close();
    }
}
console.error(`${wanted} is in none of the client archives`);
process.exit(1);
