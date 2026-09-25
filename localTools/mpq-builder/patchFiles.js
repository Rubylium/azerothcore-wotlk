const fs = require('fs');
const path = require('path');

// Everything shipped in patch-Z.MPQ: the patched DBCs and every compiled custom icon
function getPatchFiles(repoRoot) {
    const dbcRoot = path.join(repoRoot, 'server', 'Data', 'dbc');
    const iconRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled');
    const pestifereIconRoot = path.join(repoRoot, 'modules', 'mod-pestifere', 'client-assets', 'compiled', 'icons');
    const necromancerIconRoot = path.join(repoRoot, 'modules', 'mod-necromancer', 'client-assets', 'compiled', 'icons');
    const oathbladeSoundRoot = path.join(repoRoot, 'modules', 'mod-oathblade', 'client-assets', 'sounds');
    const oathbladeCompiledSoundRoot = path.join(repoRoot, 'modules', 'mod-oathblade', 'client-assets', 'compiled', 'sounds');
    const pestifereTalentRoot = path.join(repoRoot, 'modules', 'mod-pestifere', 'client-assets', 'compiled',
        'talentframe');

    const files = [
        'Spell.dbc', 'SkillLineAbility.dbc', 'SpellIcon.dbc', 'SpellVisual.dbc', 'SpellVisualKit.dbc', 'SoundEntries.dbc',
        'SpellVisualEffectName.dbc',
    ].map((name) => ({
        source: path.join(dbcRoot, name),
        archive: `DBFilesClient\\${name}`,
    }));

    for (const name of fs.readdirSync(iconRoot).filter((file) => file.toLowerCase().endsWith('.tga')).sort()) {
        files.push({ source: path.join(iconRoot, name), archive: `Interface\\Icons\\${name}` });
    }
    for (const name of fs.readdirSync(pestifereIconRoot)
        .filter((file) => file.toLowerCase().endsWith('.tga')).sort()) {
        files.push({ source: path.join(pestifereIconRoot, name), archive: `Interface\\Icons\\${name}` });
    }
    for (const name of fs.readdirSync(necromancerIconRoot)
        .filter((file) => file.toLowerCase().endsWith('.tga')).sort()) {
        files.push({ source: path.join(necromancerIconRoot, name), archive: `Interface\\Icons\\${name}` });
    }
    for (const name of fs.readdirSync(pestifereTalentRoot)
        .filter((file) => file.toLowerCase().endsWith('.tga')).sort()) {
        files.push({ source: path.join(pestifereTalentRoot, name), archive: `Interface\\TalentFrame\\${name}` });
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
    const addOathbladeSounds = (directory) => {
        for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
            const source = path.join(directory, entry.name);
            if (entry.isDirectory()) {
                addOathbladeSounds(source);
                continue;
            }
            if (!entry.name.toLowerCase().endsWith('.ogg')) {
                continue;
            }
            const relative = path.relative(oathbladeSoundRoot, source).replace(/\.ogg$/i, '.wav');
            files.push({
                source: path.join(oathbladeCompiledSoundRoot, relative),
                archive: `Sound\\Spells\\Custom\\Oathblade\\${relative.split(path.sep).join('\\')}`,
            });
        }
    };
    addOathbladeSounds(oathbladeSoundRoot);

    // The Oathblade's blue copies of the effect models its spells play (localTools/oathblade/buildBlueEffects.py)
    const oathbladeEffectRoot = path.join(repoRoot, 'modules', 'mod-oathblade', 'client-assets', 'compiled', 'spells');
    const addOathbladeEffects = (directory) => {
        for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
            const source = path.join(directory, entry.name);
            if (entry.isDirectory()) {
                addOathbladeEffects(source);
                continue;
            }
            const relative = path.relative(oathbladeEffectRoot, source).split(path.sep).join('\\');
            files.push({ source, archive: `Spells\\Oathblade\\${relative}` });
        }
    };
    if (fs.existsSync(oathbladeEffectRoot)) {
        addOathbladeEffects(oathbladeEffectRoot);
    }

    // The red ground indicators of enemy abilities (localTools/groundIndicators/buildGroundIndicators.py)
    const indicatorRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'indicators');
    for (const name of fs.readdirSync(indicatorRoot).sort()) {
        files.push({ source: path.join(indicatorRoot, name), archive: `Spells\\Evolutions\\${name}` });
    }

    return files;
}

module.exports = { getPatchFiles };
