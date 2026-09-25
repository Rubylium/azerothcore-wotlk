const fs = require('fs');
const os = require('os');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

// Builds the client interface patches from clientPatcher/interface and clientPatcher/vendor:
// - Data/<locale>/patch-<locale>-R.MPQ: the retail glue package (clientPatcher/vendor/retail-glue, Noa-1995's
//   "WoW Retail Interface", used and adapted with the author's permission: the login, realm, character select
//   and character creation screens), then every file under clientPatcher/interface on top of it (RetailUI window
//   chrome, and the glue files adapted for Evolutions), plus the client's own FrameXML.toc with our files added.
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
    'DungeonTracker.lua', 'DungeonFinderLocks.lua', 'RaidFinder.lua', 'MythicPlus.lua', 'GroundIndicators.lua',
    'CustomClasses.lua', 'CustomClassesUI.lua', 'TalentReset.lua',
    // The talent tree window: its generated trees and art first (it needs CustomClasses and RetailUI too)
    'TalentTreeData.lua', 'TalentTreeArt.lua', 'TalentTree.lua',
    // ParagonBoard.lua is the generated node table and must load before the frame that draws it
    'ParagonBoard.lua', 'Paragon.lua', 'Prestige.lua', 'ChallengeBoard.lua',
    // Loads after the CompactRaidFrame addon has run: it wraps that addon's UnitGetTotalAbsorbs
    'PestifereShield.lua',
    // Tags Mythic+ loot in its tooltip; needs GameTooltip, so it loads at the end of FrameXML
    'MythicItemTag.lua'];

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

// The archive takes one file per path: later lists win, so our own files override the vendor packages
function mergeByArchive(...lists) {
    const merged = new Map();
    for (const list of lists) {
        for (const file of list) {
            merged.set(file.archive.toLowerCase(), file);
        }
    }
    return [...merged.values()];
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

// --- the retail glue package (login, realm select, character select, character creation) ---
// clientPatcher/vendor/retail-glue, Noa-1995's "WoW Retail Interface", used and adapted with the author's
// permission. Its own GlueXML and art are packed as they are; the files we had to adapt for Evolutions (the
// Pestiféré class, our branding, French) live in clientPatcher/interface and override them by path.
const glueVendorRoot = path.join(vendorRoot, 'retail-glue');
const GLUE_VENDOR_FOLDERS = {
    GlueXML: 'Interface\\GlueXML',
    Glues: 'Interface\\Glues',
    GluesVideo: 'Interface\\GluesVideo',
    Loginscreen: 'Interface\\Loginscreen',
    tooltips: 'Interface\\Tooltips',
    cinematics: 'Interface\\cinematics',
};
// Art for races and classes this client has no models for, and the source image of the login background
// (see .agents/plans/retail-glue)
const GLUE_VENDOR_SKIPPED = ['Glues\\Models\\ui_demonhunter', 'Glues\\Models\\ui_dracthyr',
    'Loginscreen\\Background.png'];

function vendorGlueFiles() {
    if (!fs.existsSync(glueVendorRoot)) {
        throw new Error(`Missing the retail glue package: ${glueVendorRoot}`);
    }
    const files = [];
    for (const [folder, prefix] of Object.entries(GLUE_VENDOR_FOLDERS)) {
        const root = path.join(glueVendorRoot, folder);
        if (!fs.existsSync(root)) {
            continue;
        }
        for (const file of walk(root)) {
            const relative = path.join(folder, file.archive.split('\\').join(path.sep));
            if (GLUE_VENDOR_SKIPPED.some((skipped) => relative.toLowerCase()
                .startsWith(skipped.split('\\').join(path.sep).toLowerCase()))) {
                continue;
            }
            files.push({ source: file.source, archive: `${prefix}\\${file.archive}` });
        }
    }
    return files;
}

const frameXmlToc = stageToc('FrameXML', tocName, '## add new modules above here', frameXmlFiles);
// The glue package brings its own toc (clientPatcher/interface/Interface/GlueXML/GlueXML.toc), which already
// loads our custom-class files last, after CharacterCreate.xml has declared the class button template
// The Evolutions logo: the glue package carries the author's own emblem at this path, and the locale patch is
// what the client reads first, so ours goes in here rather than only in patch-L
const logoName = 'Interface\\Glues\\Common\\Glues-WoW-WotLKLogo.blp';
const logoSource = path.join(repoRoot, 'clientPatcher', 'assets', 'logo', 'Glues-WoW-WotLKLogo.blp');
if (!fs.existsSync(logoSource)) {
    throw new Error(`Missing compiled Evolutions logo: ${logoSource}`);
}

// The client cannot read a PNG. Art is authored as one and converted to BLP next to it (buildParagonArt.py
// keeps the source beside its output), so without this every source image ships too and nothing can read them.
const shippable = (files) => files.filter((file) => !/\.png$/i.test(file.archive));

const vendorGlue = vendorGlueFiles();
console.log(`Retail glue package: ${vendorGlue.length} files`);
const interfaceFiles = mergeByArchive(vendorGlue, [{ source: frameXmlToc, archive: tocName }],
    shippable(walk(interfaceRoot)), [{ source: logoSource, archive: logoName }]);
writeArchive(path.join(dataPath, locale, `patch-${locale}-R.MPQ`), interfaceFiles);
// What this patch owns. The client reads patch-L before it, so anything it also packs would win over ours --
// that is how the mix's character creation art ended up under the retail screens.
const interfaceOwned = new Set(interfaceFiles.map((file) => file.archive.toLowerCase()));

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
    // The glue layer as a whole is the retail package's now, so none of the mix's own glue code is packed
    const replacedNames = new Set([classIconName.toLowerCase(), logoName.toLowerCase()]);
    if (fs.existsSync(musicSource)) {
        replacedNames.add(musicName.toLowerCase());
    }
    const replaced = (lower) => replacedNames.has(lower) || lower.startsWith('interface\\gluexml\\')
        || interfaceOwned.has(lower);

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
            // The mix's loading bar carries Way of Elendil branding. Skipping it is only half the job -
            // the retail bar is packed explicitly further down, or the client falls through to Patch-D's
            // copy of the same branding.
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
            if (names.has(lower) && !reachable.has(lower) && !replaced(lower)) {
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
        // The retail HD loading bar, from the same retail glue package as the rest of the screens. It already
        // ships in patch-<locale>-R, but a base patch outranks a locale one, so the leftover Patch-D of the
        // client image - a full "Way of Elendil" skin, its own logo and bar included - was winning that path
        // and painting the server's old name across the progress bar. The logo is only ours on screen because
        // it is packed here too; the bar needs the same treatment.
        const loadingBarRoot = path.join(glueVendorRoot, 'Glues', 'LoadingBar');
        for (const leaf of ['Loading-BarBorder.blp', 'Loading-BarFill.blp']) {
            const loadingBarSource = path.join(loadingBarRoot, leaf);
            if (!fs.existsSync(loadingBarSource)) {
                throw new Error(`Missing retail loading bar texture: ${loadingBarSource}`);
            }
            files.push({ source: loadingBarSource, archive: `Interface\\Glues\\LoadingBar\\${leaf}` });
        }
        console.log(`Shadowlands assets: ${reachable.size} of ${names.size} files are used by the client`);
        writeArchive(path.join(dataPath, 'patch-L.MPQ'), files);
    } finally {
        archive.close();
        fs.rmSync(stage, { recursive: true, force: true });
    }
}

buildShadowlandsPatch();
