const fs = require('fs');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

// Extracts DBC files from the client archives, highest-priority copy last, so the custom class generator can
// build the client patch from the client's own (localized) data.
// Usage: node extractClientDbc.js <outputDir> <Name.dbc> [...] [--client <path>]

const argv = process.argv.slice(2);
const clientIndex = argv.indexOf('--client');
const clientPath = clientIndex >= 0 ? argv[clientIndex + 1] : 'C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK';
const args = clientIndex >= 0 ? argv.filter((_, i) => i !== clientIndex && i !== clientIndex + 1) : argv;
const [outputDir, ...names] = args;
if (!outputDir || !names.length) {
    throw new Error('Usage: node extractClientDbc.js <outputDir> <Name.dbc> [...] [--client <path>]');
}

const dataPath = path.join(clientPath, 'Data');
const locale = fs.readdirSync(dataPath).find((name) => /^[a-z]{2}[A-Z]{2}$/.test(name)
    && fs.existsSync(path.join(dataPath, name, `locale-${name}.MPQ`)));

// Lowest priority first: a later archive overwrites what an earlier one holds
const archives = ['common.MPQ', 'common-2.MPQ', 'expansion.MPQ', 'lichking.MPQ', 'patch.MPQ', 'patch-2.MPQ',
    'patch-3.MPQ', `${locale}/locale-${locale}.MPQ`, `${locale}/expansion-locale-${locale}.MPQ`,
    `${locale}/lichking-locale-${locale}.MPQ`, `${locale}/patch-${locale}.MPQ`, `${locale}/patch-${locale}-2.MPQ`,
    `${locale}/patch-${locale}-3.MPQ`];

fs.mkdirSync(outputDir, { recursive: true });
const found = new Set();
for (const archiveName of archives) {
    const archivePath = path.join(dataPath, archiveName);
    if (!fs.existsSync(archivePath)) {
        continue;
    }
    const archive = Archive.open(archivePath);
    try {
        for (const name of names) {
            const entry = `DBFilesClient\\${name}`;
            if (!archive.hasFile(entry)) {
                continue;
            }
            const file = archive.openFile(entry);
            try {
                fs.writeFileSync(path.join(outputDir, name), file.readAll());
                found.add(name);
            } finally {
                file.close();
            }
        }
    } finally {
        archive.close();
    }
}

const missing = names.filter((name) => !found.has(name));
if (missing.length) {
    throw new Error(`Not found in the ${locale} client archives: ${missing.join(', ')}`);
}
console.log(`Extracted ${found.size} DBC files from the ${locale} client into ${outputDir}`);
