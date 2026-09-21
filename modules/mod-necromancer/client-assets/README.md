# Necromancer client art

`localTools/buildNecromancerClientAssets.ps1` validates every custom icon referenced by
`localTools/necromancer/Spells.ps1`, downsizes its PNG source to a WotLK-compatible 64x64 TGA, and writes
the result under `compiled/icons`. `clientPatcher/Build-FriendPatch.ps1` runs this compiler before rebuilding
`patch-Z.MPQ`.

The set contains 16 leveling abilities, 3 active talent abilities, and 20 passive Legion talent icons.
Hidden combat-log spells reuse their visible parent ability's icon.

## Generation direction

All sources use the same prompt direction, followed by the individual spell or talent subject:

> Square World of Warcraft: Wrath of the Lich King ability icon. Immediately readable at 64x64, with one
> centered dominant silhouette, thick painterly shapes, strong rim lighting, and a deep near-black background.
> Hand-painted 2008 dark-fantasy game-icon aesthetic, dramatic high contrast, emerald and icy-cyan necromancy
> with restrained violet accents, and slightly chunky brushwork. Full bleed, without border, frame, text, UI,
> watermark, photorealism, or flat-vector styling.
