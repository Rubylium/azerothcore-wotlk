const fs = require('fs');
const os = require('os');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

// Builds the client interface patches from clientPatcher/interface and clientPatcher/vendor:
// - Data/<locale>/patch-<locale>-R.MPQ: every file under clientPatcher/interface (RetailUI window chrome, the
//   Shadowlands character creation screen with retail round icons), plus the client's own FrameXML.toc with the
//   RetailUI files added.
// - Data/patch-L.MPQ: the Shadowlands login screen assets (Patch-C.mpq of "Shadowlands character creator mix" by
//   gongel, based on warfoll02's work, modification permitted) without its GlueXML (replaced by ours) and with
//   the custom menu music from clientPatcher/vendor/music.
// Custom classes come from localTools/customClasses (run buildCustomClasses.py first).
// Usage: node buildInterfacePatch.js [--client <path>]

const argv = process.argv.slice(2);
const option = (name) => {
    const index = argv.indexOf(name);
    return index >= 0 ? argv[index + 1] : undefined;
};

const repoRoot = path.resolve(__dirname, '..', '..');
const interfaceRoot = path.join(repoRoot, 'clientPatcher', 'interface');
const vendorRoot = path.join(repoRoot, 'clientPatcher', 'vendor');
const clientPath = option('--client') || 'C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK';
const dataPath = path.join(clientPath, 'Data');

// The installed locale is the folder holding its locale archive (clients can carry other, empty locale folders)
const locale = fs.readdirSync(dataPath).find((name) => /^[a-z]{2}[A-Z]{2}$/.test(name)
    && fs.existsSync(path.join(dataPath, name, `locale-${name}.MPQ`)));
if (!locale) {
    throw new Error(`No locale folder found in ${dataPath}`);
}

const tocName = 'Interface\\FrameXML\\FrameXML.toc';
const frameXmlFiles = ['RetailUIAtlas.lua', 'RetailUI.lua', 'RetailWindows.lua', 'DungeonTrackerNames.lua',
    'DungeonTracker.lua', 'DungeonFinderLocks.lua', 'CustomClasses.lua', 'CustomClassesUI.lua'];
const glueXmlFiles = ['CustomClasses.lua', 'CustomClassesGlue.lua'];
const glueTocName = 'Interface' + String.fromCharCode(92) + 'GlueXML' + String.fromCharCode(92) + 'GlueXML.toc';

function readArchiveFile(archivePath, name) {
    const archive = Archive.open(archivePath);
    try {
        if (!archive.hasFile(name)) {
            return null;
        }
        const file = archive.openFile(name);
        try {
            return file.readAll();
        } finally {
            file.close();
        }
    } finally {
        archive.close();
    }
}

// The stock toc as the client loads it: the highest-priority stock locale archive that has one
function readStockToc(name = tocName) {
    const archives = [`patch-${locale}-3.MPQ`, `patch-${locale}-2.MPQ`, `patch-${locale}.MPQ`, `locale-${locale}.MPQ`];
    for (const archiveName of archives) {
        const archivePath = path.join(dataPath, locale, archiveName);
        const data = fs.existsSync(archivePath) ? readArchiveFile(archivePath, name) : null;
        if (data) {
            return data.toString('utf8');
        }
    }
    throw new Error(`${name} not found in the ${locale} archives`);
}

function walk(root, relative = '') {
    const out = [];
    for (const name of fs.readdirSync(path.join(root, relative)).sort()) {
        const child = path.join(relative, name);
        if (fs.statSync(path.join(root, child)).isDirectory()) {
            out.push(...walk(root, child));
        } else {
            out.push({ source: path.join(root, child), archive: child.split(path.sep).join('\\') });
        }
    }
    return out;
}

function writeArchive(outputPath, files) {
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
    try {
        for (const file of files) {
            if (!verification.hasFile(file.archive)) {
                throw new Error(`${outputPath} is missing ${file.archive}`);
            }
        }
    } finally {
        verification.close();
    }
    console.log(`Built ${outputPath} (${files.length} files)`);
}

// --- patch-<locale>-R: interface ---
function stageToc(label, stockName, marker, files) {
    const stock = readStockToc(stockName);
    if (!stock.includes(marker)) {
        throw new Error(`Unexpected ${stockName} layout`);
    }
    const staged = path.join(os.tmpdir(), `${label}-${locale}.toc`);
    fs.writeFileSync(staged, stock.replace(marker, `${files.join('\r\n')}\r\n\r\n${marker}`));
    return staged;
}

const frameXmlToc = stageToc('FrameXML', tocName, '## add new modules above here', frameXmlFiles);
// The glue marker sits at the end of the list, so these load after CharacterCreate.xml declares the
// class button template they build on
const glueToc = stageToc('GlueXML', glueTocName, '## This Always Runs Last, Add New Items above it.',
    glueXmlFiles);
writeArchive(path.join(dataPath, locale, `patch-${locale}-R.MPQ`),
    [{ source: frameXmlToc, archive: tocName }, { source: glueToc, archive: glueTocName },
        ...walk(interfaceRoot)]);

// --- patch-L: Shadowlands login screen assets + menu music ---
// The mod ships 430 MB, most of it never loaded by 3.3.5 (Worgen/Goblin/Pandaria/allied race scenes, older
// versions of its race backgrounds, PNG sources). Only reachable files are packed:
// - roots: files overriding a stock client path (the client loads those through its own references) and file
//   paths named by our interface code
// - closure: file paths embedded in reachable .m2/.wmo files, and each .m2's .skin / .anim companions
function listfile(archive) {
    if (!archive.hasFile('(listfile)')) {
        return [];
    }
    const file = archive.openFile('(listfile)');
    try {
        return file.readAll().toString('latin1').split(/\r?\n/).filter(Boolean);
    } finally {
        file.close();
    }
}

function stockFileNames() {
    const names = new Set();
    const archives = ['common.MPQ', 'common-2.MPQ', 'expansion.MPQ', 'lichking.MPQ', 'patch.MPQ', 'patch-2.MPQ',
        'patch-3.MPQ', `${locale}/locale-${locale}.MPQ`, `${locale}/expansion-locale-${locale}.MPQ`,
        `${locale}/lichking-locale-${locale}.MPQ`, `${locale}/patch-${locale}.MPQ`, `${locale}/patch-${locale}-2.MPQ`,
        `${locale}/patch-${locale}-3.MPQ`];
    for (const name of archives) {
        const archivePath = path.join(dataPath, name);
        if (!fs.existsSync(archivePath)) {
            continue;
        }
        const archive = Archive.open(archivePath);
        try {
            for (const entry of listfile(archive)) {
                names.add(entry.toLowerCase());
            }
        } finally {
            archive.close();
        }
    }
    return names;
}

function buildShadowlandsPatch() {
    const source = path.join(vendorRoot, 'shadowlands-charcreate', 'extracted', 'Shadowlands login screen mix',
        'Patch-C.mpq');
    const musicSource = path.join(vendorRoot, 'music', 'WotLK_main_title.mp3');
    const musicName = 'Sound\\Music\\GlueScreenMusic\\WotLK_main_title.mp3';
    const classIconName = 'Interface\\Glues\\CharacterCreate\\UI-CharacterCreate-Classes.blp';
    const classIconSource = path.join(interfaceRoot, ...classIconName.split('\\'));
    const logoName = 'Interface\\Glues\\Common\\Glues-WoW-WotLKLogo.blp';
    const logoSource = path.join(repoRoot, 'clientPatcher', 'assets', 'logo', 'Glues-WoW-WotLKLogo.blp');
    const replaced = new Set(['interface\\gluexml\\charactercreate.lua', 'interface\\gluexml\\charactercreate.xml',
        'interface\\gluexml\\glueparent.lua', classIconName.toLowerCase(), logoName.toLowerCase()]);
    if (fs.existsSync(musicSource)) {
        replaced.add(musicName.toLowerCase());
    }

    const stock = stockFileNames();
    const archive = Archive.open(source);
    const stage = fs.mkdtempSync(path.join(os.tmpdir(), 'patch-L-'));
    try {
        const names = new Map();
        for (const name of listfile(archive)) {
            if (archive.hasFile(name)) {
                names.set(name.toLowerCase(), name);
            }
        }
        const read = (lower) => {
            const file = archive.openFile(names.get(lower));
            try {
                return file.readAll();
            } finally {
                file.close();
            }
        };

        // Kept stock: the mod's Shadowlands login scene (the classic WotLK login screen stays)
        const excludedFolders = [
            'interface\\glues\\models\\ui_mainmenu_northrend\\',
            // Restore the original 3.3.5 loading bar; the vendor version carries Way of Elendil branding.
            'interface\\glues\\loadingbar\\',
            // Restore all regular in-game buttons instead of globally replacing them with blue Glue art.
            'interface\\buttons\\',
        ];
        const reachable = new Set();
        const queue = [];
        const mark = (lower) => {
            const isGlueButton = lower.startsWith('interface\\glues\\common\\') && lower.includes('button');
            const isSharedGlueButton = lower.startsWith('interface\\common\\glues-bigbutton-');
            if (excludedFolders.some((folder) => lower.startsWith(folder)) || isGlueButton || isSharedGlueButton) {
                return;
            }
            if (names.has(lower) && !reachable.has(lower) && !replaced.has(lower)) {
                reachable.add(lower);
                queue.push(lower);
            }
        };
        for (const lower of names.keys()) {
            if (stock.has(lower)) {
                mark(lower);
            }
        }
        for (const file of walk(interfaceRoot).filter((entry) => /\.(lua|xml)$/i.test(entry.source))) {
            const text = fs.readFileSync(file.source, 'latin1');
            for (const match of text.matchAll(/Interface[\\/]+[A-Za-z0-9_\-\\/ ().']+/g)) {
                const base = match[0].replace(/[\\/]+/g, '\\').toLowerCase();
                for (const extension of ['', '.blp', '.m2', '.tga']) {
                    mark(base + extension);
                }
            }
        }
        // Converted models name their textures with either slash ("interface/glues/models/...")
        const embeddedPath = /[A-Za-z0-9_\- ().']+(?:[\\/][A-Za-z0-9_\- ().']+)+\.(?:blp|m2|mdx|mdl|wmo|skin|anim)/gi;
        while (queue.length) {
            const lower = queue.pop();
            const extension = lower.split('.').pop();
            if (extension === 'm2') {
                const stem = lower.slice(0, -3);
                for (const other of names.keys()) {
                    if (other.startsWith(stem) && /(\d\d\.skin|_lod\d\d\.skin|\d{4}-\d\d\.anim)$/.test(other)) {
                        mark(other);
                    }
                }
            }
            if (extension === 'm2' || extension === 'wmo') {
                for (const match of read(lower).toString('latin1').matchAll(embeddedPath)) {
                    mark(match[0].toLowerCase().replace(/\//g, '\\').replace(/\.(mdx|mdl)$/, '.m2'));
                }
            }
        }

        const files = [];
        for (const lower of [...reachable].sort()) {
            const staged = path.join(stage, `${files.length}.bin`);
            fs.writeFileSync(staged, read(lower));
            files.push({ source: staged, archive: names.get(lower) });
        }
        if (fs.existsSync(musicSource)) {
            files.push({ source: musicSource, archive: musicName });
        }
        if (!fs.existsSync(classIconSource)) {
            throw new Error(`Missing custom class icon atlas: ${classIconSource}`);
        }
        files.push({ source: classIconSource, archive: classIconName });
        if (!fs.existsSync(logoSource)) {
            throw new Error(`Missing compiled Evolutions logo: ${logoSource}`);
        }
        files.push({ source: logoSource, archive: logoName });
        console.log(`Shadowlands assets: ${reachable.size} of ${names.size} files are used by the client`);
        writeArchive(path.join(dataPath, 'patch-L.MPQ'), files);
    } finally {
        archive.close();
        fs.rmSync(stage, { recursive: true, force: true });
    }
}

buildShadowlandsPatch();
