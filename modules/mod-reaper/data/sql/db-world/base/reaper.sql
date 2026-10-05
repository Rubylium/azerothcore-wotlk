-- The Faucheur (modules/mod-reaper): its aura scripts, the proc of its hidden passive, and the coefficients of its spells
-- that do not strike with the weapon. A carrier whose amount mod-reaper works out (a heal from damage dealt, an echo of
-- a blow, a shield) carries an explicit zero row (the core would add a default coefficient on top).
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_reaper_passive', 'spell_reaper_jailers_bargain',
    'spell_reaper_doomrend_shield', 'spell_reaper_darkrend', 'spell_reaper_companions');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(98055, 'spell_reaper_passive'),
(98036, 'spell_reaper_jailers_bargain'),
(98103, 'spell_reaper_doomrend_shield'),
(98114, 'spell_reaper_darkrend'),
(98213, 'spell_reaper_companions'),
(98301, 'spell_reaper_companions'),
(98303, 'spell_reaper_companions'),
(98315, 'spell_reaper_companions');

-- The hidden passive (98055): its own auto attacks that land, and the melee attacks it parries or dodges
-- ProcFlags 0x2C = DONE_MELEE_AUTO_ATTACK | TAKEN_MELEE_AUTO_ATTACK | TAKEN_SPELL_MELEE_DMG_CLASS
-- HitMask 0x33 = NORMAL | CRITICAL | DODGE | PARRY
DELETE FROM `spell_proc` WHERE `SpellId` = 98055;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`,
    `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`,
    `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(98055, 0, 0, 0, 0, 0, 44, 0, 0, 51, 0, 0, 0, 100, 0, 0);

-- Attack power coefficients of the spells that are not weapon strikes (Ascension's own, sized for level 80)
DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 98000 AND 98999;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(98001, 0, 0, 0.45, 0, 'Faucheur - Meurtre'),
(98002, 0, 0, 0.45, 0, 'Faucheur - Dechirure d''ame'),
(98006, 0, 0, 0.5, 0, 'Faucheur - Trait d''ame (each enemy)'),
(98010, 0, 0, 0, 0.12, 'Faucheur - Vent de mort (each 2 s tick)'),
(98011, 0, 0, 0.35, 0, 'Faucheur - Fracas d''ames (each enemy, before the souls it consumed)'),
(98024, 0, 0, 0.5, 0, 'Faucheur - Foulee spectrale'),
(98035, 0, 0, 0.25, 0, 'Faucheur - Hurlement spectral'),
(98040, 0, 0, 0.3, 0, 'Faucheur - Lame spectrale (shadow part)'),
(98046, 0, 0, 0.1, 0, 'Faucheur - Moisson d''ame'),
(98105, 0, 0, 0.3, 0, 'Faucheur - Sectionner'),
(98112, 0, 0, 0, 0.1, 'Faucheur - Frenesie sanglante (each 0.75 s tick)'),
(98113, 0, 0, 0.45, 0, 'Faucheur - Frenesie sanglante (burst)'),
(98202, 0, 0, 0, 0.12, 'Faucheur - Chasse-mort (each 2 s tick)'),
(98209, 0, 0, 0.36, 0, 'Faucheur - Apparition'),
(98215, 0, 0, 0.25, 0, 'Faucheur - La fin est proche'),
(98218, 0, 0, 0.18, 0, 'Faucheur - Appel du Geolier'),
(98302, 0, 0, 0.15, 0, 'Faucheur - Faux spectrale'),
(98306, 0, 0, 0.4, 0, 'Faucheur - Requiem (each enemy)'),
(98324, 0, 0, 0.2, 0, 'Faucheur - Gardien spectral'),
-- Amounts set by mod-reaper
(98036, 0, 0, 0, 0, 'Faucheur - Pacte du Geolier (30% of maximum health)'),
(98047, 0, 0, 0, 0, 'Faucheur - Forme spectrale (shield)'),
(98053, 0, 0, 0, 0, 'Faucheur - Ame tourmentee (heal)'),
(98054, 0, 0, 0, 0, 'Faucheur - Revigoration d''essence (heal)'),
(98056, 0, 0, 0, 0, 'Faucheur - Moisson d''ame (heal)'),
(98103, 0, 0, 0, 0, 'Faucheur - Laceration funeste (healing absorbed)'),
(98114, 0, 0, 0, 0, 'Faucheur - Faux dechirante (tick)'),
(98120, 0, 0, 0, 0, 'Faucheur - Ames pour le massacre (share of the blow)'),
(98121, 0, 0, 0, 0, 'Faucheur - Moissonneur (heal)'),
(98127, 0, 0, 0, 0, 'Faucheur - Soif cramoisie (heal)'),
(98205, 0, 0, 0, 0, 'Faucheur - Arme fantomatique (share of the blow)'),
(98211, 0, 0, 0, 0, 'Faucheur - Pourriture d''ame (burst)'),
(98212, 0, 0, 0, 0, 'Faucheur - Embuscade d''anima (tick)'),
(98219, 0, 0, 0, 0, 'Faucheur - Garde fantomatique (copy of the blow)'),
(98220, 0, 0, 0, 0, 'Faucheur - Lamentation (heal)'),
(98221, 0, 0, 0, 0, 'Faucheur - Porteur de fin (heal)'),
(98304, 0, 0, 0, 0, 'Faucheur - Gardien spectral (shield)'),
(98323, 0, 0, 0, 0, 'Faucheur - Ponction de vie (heal)'),
(98325, 0, 0, 0, 0, 'Faucheur - Echardes d''ame (tick)'),
(98326, 0, 0, 0, 0, 'Faucheur - Siphon d''anima (heal)'),
(98328, 0, 0, 0, 0, 'Faucheur - Frappe d''ame (heal)');
