-- Pestiféré (class 12): binds the mod-pestifere scripts to their spells, and gives its threat strike its bonus threat.
--
-- Spell rows live in the client patch and server/Data/dbc (localTools/patchSinisterStrike.ps1):
--   90203 Odeur de charogne, 90204 Crachat bilieux, 90206 (Détonation en chaîne's jump), 90217 (Pandémie's extra
--   strikes), 90228 Puanteur insoutenable and 90229 Pandémie are pure data and carry no script. 90210/90212/90214
--   only trigger the plague they inoculate. Talent ranks that only change a number are read by the scripts below;
--   Mains putrides (90243-90245) is a proc of its own.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
    'PestifereFrappePutrideSpellScript',
    'PestiferePourritureAuraScript',
    'PestifereMorsureFetideSpellScript',
    'PestifereRipostePurulenteSpellScript',
    'PestifereFlaqueDeBileAuraScript',
    'PestifereCarapaceSuintanteAuraScript',
    'PestifereBondPutrideSpellScript',
    'PestifereVomissureSpellScript',
    'PestifereAvatarDeLaPesteAuraScript',
    'PestifereContagionSpellScript',
    'PestifereCharnierAmbulantSpellScript',
    'PestifereDetonationSpellScript',
    'PestiferePurgeCathartiqueSpellScript',
    'PestifereSepulcreAuraScript',
    'PestifereSepulcreStoredAuraScript',
    'PestifereMainsPutridesAuraScript',
    'PestifereCarapaceNecroseeAuraScript',
    'PestifereChairPutrideAuraScript',
    'PestiferePesteVirulenteAuraScript',
    'PestifereSangsueAuraScript',
    'PestifereAbsorptionMorbideSpellScript',
    'PestifereDonDeSangSpellScript',
    'PestifereSymbioteSpellScript',
    'PestifereSymbioteAuraScript',
    'PestifereCoagulationAuraScript',
    'PestifereSporesAuraScript',
    'PestifereBrumeSpellScript',
    'PestifereBrumeAuraScript'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90200, 'PestifereFrappePutrideSpellScript'),
(90205, 'PestiferePourritureAuraScript'),
(90224, 'PestifereMorsureFetideSpellScript'),
(90225, 'PestifereRipostePurulenteSpellScript'),
(90223, 'PestifereFlaqueDeBileAuraScript'),
(90226, 'PestifereCarapaceSuintanteAuraScript'),
(90227, 'PestifereBondPutrideSpellScript'),
(90284, 'PestifereVomissureSpellScript'),
(90287, 'PestifereAvatarDeLaPesteAuraScript'),
(90201, 'PestifereContagionSpellScript'),
(90256, 'PestifereCharnierAmbulantSpellScript'),
(90202, 'PestifereDetonationSpellScript'),
(90265, 'PestiferePurgeCathartiqueSpellScript'),
(90268, 'PestifereSepulcreAuraScript'),
(90207, 'PestifereSepulcreStoredAuraScript'),
(90208, 'PestifereSepulcreStoredAuraScript'),
(90243, 'PestifereMainsPutridesAuraScript'),
(90244, 'PestifereMainsPutridesAuraScript'),
(90245, 'PestifereMainsPutridesAuraScript'),
(90211, 'PestifereCarapaceNecroseeAuraScript'),
(90213, 'PestifereChairPutrideAuraScript'),
(90215, 'PestiferePesteVirulenteAuraScript'),
-- The healer tree "Sangsue" (PestifereHealer.cpp). Transfusion itself is the damage hook, and Saignée and
-- Pestilence salvatrice are read by it: no script of their own.
(90302, 'PestifereSangsueAuraScript'),
(90304, 'PestifereAbsorptionMorbideSpellScript'),
(90305, 'PestifereDonDeSangSpellScript'),
(90306, 'PestifereSymbioteSpellScript'),
(90307, 'PestifereSymbioteAuraScript'),
(90309, 'PestifereCoagulationAuraScript'),
(90312, 'PestifereSporesAuraScript'),
(90315, 'PestifereBrumeSpellScript'),
(90316, 'PestifereBrumeAuraScript');

-- Morsure fétide is the single-target threat button: flat bonus threat on top of its damage, like Shield Slam's
-- (Carapace nécrosée then doubles all of it)
DELETE FROM `spell_threat` WHERE `entry` = 90224;
INSERT INTO `spell_threat` (`entry`, `flatMod`, `pctMod`, `apPctMod`) VALUES
(90224, 650, 1, 0);
