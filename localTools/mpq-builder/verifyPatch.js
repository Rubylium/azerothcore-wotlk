const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { Archive } = require('@jamiephan/stormlib');

const repoRoot = path.resolve(__dirname, '..', '..');
const iconRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled');
const archivePath = process.argv[2];
if (!archivePath || !fs.existsSync(archivePath)) {
    throw new Error(`Patch MPQ not found: ${archivePath || '<missing argument>'}`);
}

const expected = [
    { path: 'DBFilesClient\\Spell.dbc', source: path.join(repoRoot, 'server', 'Data', 'dbc', 'Spell.dbc') },
    { path: 'DBFilesClient\\SkillLineAbility.dbc', source: path.join(repoRoot, 'server', 'Data', 'dbc', 'SkillLineAbility.dbc') },
    { path: 'DBFilesClient\\SpellIcon.dbc', source: path.join(repoRoot, 'server', 'Data', 'dbc', 'SpellIcon.dbc') },
    { path: 'Interface\\Icons\\RogueMomentum_QuickCut.tga', source: path.join(iconRoot, 'RogueMomentum_QuickCut.tga') },
    { path: 'Interface\\Icons\\RogueMomentum_ShadowLunge.tga', source: path.join(iconRoot, 'RogueMomentum_ShadowLunge.tga') },
    { path: 'Interface\\Icons\\RogueMomentum_Riposte.tga', source: path.join(iconRoot, 'RogueMomentum_Riposte.tga') },
    { path: 'Interface\\Icons\\RogueMomentum_SanguineVeil.tga', source: path.join(iconRoot, 'RogueMomentum_SanguineVeil.tga') },
    { path: 'Interface\\Icons\\RogueMomentum_Opening.tga', source: path.join(iconRoot, 'RogueMomentum_Opening.tga') },
    { path: 'Interface\\Icons\\RogueMomentum_BattleTempo.tga', source: path.join(iconRoot, 'RogueMomentum_BattleTempo.tga') },
    { path: 'Interface\\Icons\\RogueMomentum_KillingMomentum.tga', source: path.join(iconRoot, 'RogueMomentum_KillingMomentum.tga') },
    { path: 'Interface\\Icons\\RogueMomentum_CrimsonSweep.tga', source: path.join(iconRoot, 'RogueMomentum_CrimsonSweep.tga') },
    { path: 'Interface\\Icons\\Ability_Warrior_GladiatorStance.tga', source: path.join(iconRoot, 'Ability_Warrior_GladiatorStance.tga') },
];
const archive = Archive.open(archivePath);
const results = [];
try {
    for (const entry of expected) {
        if (!archive.hasFile(entry.path)) {
            throw new Error(`Missing archive entry: ${entry.path}`);
        }
        const file = archive.openFile(entry.path);
        try {
            const archivedData = file.readAll();
            const sourceData = fs.readFileSync(entry.source);
            const archivedHash = crypto.createHash('sha256').update(archivedData).digest('hex');
            const sourceHash = crypto.createHash('sha256').update(sourceData).digest('hex');
            if (archivedHash !== sourceHash) {
                throw new Error(`Hash mismatch: ${entry.path}`);
            }
            results.push({ path: entry.path, bytes: archivedData.length, sha256: archivedHash });
        } finally {
            file.close();
        }
    }
} finally {
    archive.close();
}
console.log(JSON.stringify({ archivePath, files: results }, null, 2));
