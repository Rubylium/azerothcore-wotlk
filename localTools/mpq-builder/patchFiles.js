const fs = require('fs');
const path = require('path');

// Everything shipped in patch-Z.MPQ: the patched DBCs and every compiled custom icon
function getPatchFiles(repoRoot) {
    const dbcRoot = path.join(repoRoot, 'server', 'Data', 'dbc');
    const iconRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled');

    const files = [
        'Spell.dbc', 'SkillLineAbility.dbc', 'SpellIcon.dbc', 'SpellVisual.dbc', 'SpellVisualKit.dbc', 'SoundEntries.dbc',
    ].map((name) => ({
        source: path.join(dbcRoot, name),
        archive: `DBFilesClient\\${name}`,
    }));

    for (const name of fs.readdirSync(iconRoot).filter((file) => file.toLowerCase().endsWith('.tga')).sort()) {
        files.push({ source: path.join(iconRoot, name), archive: `Interface\\Icons\\${name}` });
    }

    const soundRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'sounds');
    const addSoundFiles = (directory) => {
        for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
            const source = path.join(directory, entry.name);
            if (entry.isDirectory()) {
                addSoundFiles(source);
                continue;
            }
            const relative = path.relative(soundRoot, source).split(path.sep).join('\\');
            files.push({ source, archive: `Sound\\Spells\\Custom\\CombatRogue\\${path.basename(relative)}` });
        }
    };
    addSoundFiles(soundRoot);

    return files;
}

module.exports = { getPatchFiles };
