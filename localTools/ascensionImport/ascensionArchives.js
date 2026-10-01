// Reads the Ascension client's archives (D:\ascension-live by default) for localTools/ascensionImport.
//
//   node ascensionArchives.js dbc <outDir> <name>...      DBFilesClient\<name>.dbc, the copy with the most rows
//   node ascensionArchives.js files <list.json> <outDir>  each archive path, the highest-priority copy, written
//                                                         under outDir at the same relative path; missing.json lists
//                                                         the paths found nowhere
//   node ascensionArchives.js stock <list.json> <out.json> which paths the stock 3.3.5 client (CleanWOTLK) already
//                                                         holds, so an import ships only what the game lacks
//
// Ascension's tables are stock-layout 3.3.5 DBCs spread over its patches; a table is taken whole from the archive
// holding the largest copy, since a locale patch may carry an older, smaller one. Files follow the game's own order:
// the base archives, the patches in name order, then the locale's.
const fs = require('fs');
const path = require('path');
const { Archive, MPQ_OPEN_READ_ONLY } = require('../mpq-builder/node_modules/@jamiephan/stormlib');

const ascensionRoot = process.env.ASCENSION_ROOT || 'D:\\ascension-live';
const stockRoot = process.env.STOCK_CLIENT || 'C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK';

function mpqs(directory) {
    if (!fs.existsSync(directory)) {
        return [];
    }
    return fs.readdirSync(directory).filter((name) => /\.mpq$/i.test(name)).sort()
        .map((name) => path.join(directory, name));
}

function ascensionArchives() {
    const data = path.join(ascensionRoot, 'Data');
    const base = ['common.MPQ', 'common-2.MPQ', 'expansion.MPQ', 'lichking.MPQ'].map((name) => path.join(data, name));
    const patches = mpqs(data).filter((file) => !base.includes(file));
    return [...base.filter((file) => fs.existsSync(file)), ...patches, ...mpqs(path.join(data, 'area-52')),
        ...mpqs(path.join(data, 'enUS'))];
}

function stockArchives() {
    const data = path.join(stockRoot, 'Data');
    const names = ['common.MPQ', 'common-2.MPQ', 'expansion.MPQ', 'lichking.MPQ', 'patch.MPQ', 'patch-2.MPQ',
        'patch-3.MPQ'];
    for (const locale of ['enUS', 'frFR']) {
        for (const name of ['locale', 'expansion-locale', 'lichking-locale', 'patch']) {
            names.push(`${locale}/${name}-${locale}.MPQ`);
        }
        names.push(`${locale}/patch-${locale}-2.MPQ`, `${locale}/patch-${locale}-3.MPQ`);
    }
    return names.map((name) => path.join(data, name)).filter((file) => fs.existsSync(file));
}

function withArchives(files, visit) {
    for (const file of files) {
        const archive = Archive.open(file, MPQ_OPEN_READ_ONLY);
        try {
            visit(archive, file);
        } finally {
            archive.close();
        }
    }
}

const [command, ...args] = process.argv.slice(2);
if (command === 'dbc') {
    const [outDir, ...names] = args;
    fs.mkdirSync(outDir, { recursive: true });
    const best = {};
    withArchives(ascensionArchives(), (archive) => {
        for (const name of names) {
            const file = `DBFilesClient\\${name}.dbc`;
            if (!archive.hasFile(file)) {
                continue;
            }
            const data = Buffer.from(archive.readFile(file));
            if (!best[name] || data.readUInt32LE(4) > best[name].readUInt32LE(4)) {
                best[name] = data;
            }
        }
    });
    for (const name of names) {
        if (!best[name]) {
            throw new Error(`${name}.dbc is in no Ascension archive`);
        }
        fs.writeFileSync(path.join(outDir, `${name}.dbc`), best[name]);
    }
}
else if (command === 'files') {
    const [listPath, outDir] = args;
    const wanted = JSON.parse(fs.readFileSync(listPath, 'utf8'));
    const found = new Map();
    withArchives(ascensionArchives(), (archive) => {
        for (const file of wanted) {
            if (archive.hasFile(file)) {
                found.set(file, Buffer.from(archive.readFile(file)));
            }
        }
    });
    const missing = [];
    for (const file of wanted) {
        const data = found.get(file);
        if (!data) {
            missing.push(file);
            continue;
        }
        const target = path.join(outDir, ...file.split('\\'));
        fs.mkdirSync(path.dirname(target), { recursive: true });
        fs.writeFileSync(target, data);
    }
    fs.mkdirSync(outDir, { recursive: true });
    fs.writeFileSync(path.join(outDir, 'missing.json'), JSON.stringify(missing, null, 2));
}
else if (command === 'stock') {
    const [listPath, outPath] = args;
    const wanted = JSON.parse(fs.readFileSync(listPath, 'utf8'));
    const stock = new Set();
    withArchives(stockArchives(), (archive) => {
        for (const file of wanted) {
            if (!stock.has(file) && archive.hasFile(file)) {
                stock.add(file);
            }
        }
    });
    fs.writeFileSync(outPath, JSON.stringify([...stock].sort(), null, 2));
}
else {
    console.error('usage: node ascensionArchives.js dbc|files|stock ...');
    process.exit(2);
}
