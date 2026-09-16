const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

const repoRoot = path.resolve(__dirname, '..', '..');
const dbcRoot = path.join(repoRoot, 'server', 'Data', 'dbc');
const iconRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled');
const outputPath = process.argv[2] || path.join(__dirname, 'patch-Z.MPQ');
const files = [
    { source: path.join(dbcRoot, 'Spell.dbc'), archive: 'DBFilesClient\\Spell.dbc' },
    { source: path.join(dbcRoot, 'SkillLineAbility.dbc'), archive: 'DBFilesClient\\SkillLineAbility.dbc' },
    { source: path.join(dbcRoot, 'SpellIcon.dbc'), archive: 'DBFilesClient\\SpellIcon.dbc' },
    { source: path.join(iconRoot, 'RogueMomentum_QuickCut.tga'), archive: 'Interface\\Icons\\RogueMomentum_QuickCut.tga' },
    { source: path.join(iconRoot, 'RogueMomentum_ShadowLunge.tga'), archive: 'Interface\\Icons\\RogueMomentum_ShadowLunge.tga' },
    { source: path.join(iconRoot, 'RogueMomentum_Riposte.tga'), archive: 'Interface\\Icons\\RogueMomentum_Riposte.tga' },
    { source: path.join(iconRoot, 'RogueMomentum_SanguineVeil.tga'), archive: 'Interface\\Icons\\RogueMomentum_SanguineVeil.tga' },
    { source: path.join(iconRoot, 'RogueMomentum_Opening.tga'), archive: 'Interface\\Icons\\RogueMomentum_Opening.tga' },
    { source: path.join(iconRoot, 'RogueMomentum_BattleTempo.tga'), archive: 'Interface\\Icons\\RogueMomentum_BattleTempo.tga' },
    { source: path.join(iconRoot, 'RogueMomentum_KillingMomentum.tga'), archive: 'Interface\\Icons\\RogueMomentum_KillingMomentum.tga' },
    { source: path.join(iconRoot, 'RogueMomentum_CrimsonSweep.tga'), archive: 'Interface\\Icons\\RogueMomentum_CrimsonSweep.tga' },
    { source: path.join(iconRoot, 'Ability_Warrior_GladiatorStance.tga'), archive: 'Interface\\Icons\\Ability_Warrior_GladiatorStance.tga' },
];

for (const file of files) {
    if (!fs.existsSync(file.source)) {
        throw new Error(`Missing source DBC: ${file.source}`);
    }
}

if (fs.existsSync(outputPath)) {
    fs.unlinkSync(outputPath);
}

const archive = new Archive();
archive.create(outputPath, { maxFileCount: 16, flags: 0 });
for (const file of files) {
    archive.addFile(file.source, file.archive);
}
archive.close();

const verification = Archive.open(outputPath);
const results = [];
try {
    for (const file of files) {
        if (!verification.hasFile(file.archive)) {
            throw new Error(`MPQ is missing ${file.archive}`);
        }
        const archivedFile = verification.openFile(file.archive);
        try {
            const archivedData = archivedFile.readAll();
            const sourceData = fs.readFileSync(file.source);
            const archivedHash = crypto.createHash('sha256').update(archivedData).digest('hex');
            const sourceHash = crypto.createHash('sha256').update(sourceData).digest('hex');
            if (archivedHash !== sourceHash) {
                throw new Error(`Hash mismatch for ${file.archive}`);
            }
            results.push({ path: file.archive, bytes: archivedData.length, sha256: archivedHash });
        } finally {
            archivedFile.close();
        }
    }
} finally {
    verification.close();
}

console.log(JSON.stringify({ outputPath, files: results }, null, 2));
