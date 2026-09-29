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
        // The paragon glyphs' icons and the retail import test items (localTools/patchSinisterStrike.ps1)
        'Item.dbc',
        // The looks imported from the retail client (localTools/retailImport)
        'ItemDisplayInfo.dbc',
        // The Celestial Planetarium's room music (L'Infini, patchSinisterStrike.ps1)
        'ZoneMusic.dbc',
        // L'Infini's gear: its bonuses and lore lines (patchSinisterStrike.ps1)
        'SpellItemEnchantment.dbc',
    ].map((name) => ({
        source: path.join(dbcRoot, name),
        archive: `DBFilesClient\\${name}`,
    }));
    // L'Infini's display (localTools/infiniteBoss): the client's own copies, built on Patch-D's rather than the
    // server's (patchSinisterStrike.ps1)
    for (const name of ['CreatureModelData.dbc', 'CreatureDisplayInfo.dbc']) {
        files.push({ source: path.join(dbcRoot, 'client-only', name), archive: `DBFilesClient\\${name}` });
    }

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

    // Item models, textures and icons imported from the retail client (localTools/retailImport), stored at their
    // archive path: Item\ObjectComponents\..., Interface\Icons\...
    const retailItemRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled',
        'retail-items');
    const addRetailItems = (directory) => {
        for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
            const source = path.join(directory, entry.name);
            if (entry.isDirectory()) {
                addRetailItems(source);
                continue;
            }
            files.push({ source, archive: path.relative(retailItemRoot, source).split(path.sep).join('\\') });
        }
    };
    if (fs.existsSync(retailItemRoot)) {
        addRetailItems(retailItemRoot);
    }

    // L'Infini, Algalon recoloured (localTools/infiniteBoss/buildInfiniModel.py), stored at its archive path:
    // Creature\Evolutions\Infini\...
    const infiniteBossRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled',
        'infinite-boss');
    const addInfiniteBoss = (directory) => {
        for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
            const source = path.join(directory, entry.name);
            if (entry.isDirectory()) {
                addInfiniteBoss(source);
                continue;
            }
            files.push({ source, archive: path.relative(infiniteBossRoot, source).split(path.sep).join('\\') });
        }
    };
    if (fs.existsSync(infiniteBossRoot)) {
        addInfiniteBoss(infiniteBossRoot);
    }
    // Its music (SoundEntries 30100, patchSinisterStrike.ps1), in this base patch rather than a locale one
    files.push({
        source: path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'music', 'LInfini.mp3'),
        archive: 'Sound\\Music\\Evolutions\\LInfini.mp3',
    });
    // Its room music before the pull (SoundEntries 30102, the Planetarium's zone music)
    files.push({
        source: path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'music',
            'LInfiniRoom.mp3'),
        archive: 'Sound\\Music\\Evolutions\\LInfiniRoom.mp3',
    });
    // What ends it (SoundEntries 30101): 2 s of silence
    files.push({
        source: path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'music',
            'LInfiniSilence.mp3'),
        archive: 'Sound\\Music\\Evolutions\\LInfiniSilence.mp3',
    });

    // The red ground indicators of enemy abilities (localTools/groundIndicators/buildGroundIndicators.py)
    const indicatorRoot = path.join(repoRoot, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'indicators');
    for (const name of fs.readdirSync(indicatorRoot).sort()) {
        files.push({ source: path.join(indicatorRoot, name), archive: `Spells\\Evolutions\\${name}` });
    }

    return files;
}

// How a file goes into an archive. Sound (music above all) is stored as it is, neither compressed nor encrypted, as
// the game's own archives store it: the client streams music straight out of the archive, and StormLib's default
// (compressed and encrypted) left a track it would not play. Anything else takes the default.
const AUDIO = /\.(mp3|ogg|wav)$/i;
function addOptions(archiveName) {
    return AUDIO.test(archiveName) ? { flags: 0 } : undefined;
}

module.exports = { getPatchFiles, addOptions };
