# mod-pestifere

Server-side behaviour of the **Pestiféré** (ChrClasses id 12), a plague tank with a melee healer tree. Design:
`.agents/plans/pestifere/pestifere.DESIGN.md` (tank) and `.agents/plans/pestifere/pestifere-healer.DESIGN.md`
(healer).

The class itself is declared by `modules/mod-custom-classes` (world table `custom_class`), and its spell rows
are generated into the client patch and `server/Data/dbc` by `localTools/patchSinisterStrike.ps1`. This module
owns only what the spell data cannot express.

## The spine

A Pestiféré inoculates itself with up to three plagues. **Virulence** is how many it carries (0-3), and it
scales all three. Contagion hands the *enemy versions* to the pack and refreshes the carrier's own;
Détonation spends the rot the pack has accumulated.

| Plague | Self aura | Enemy version |
|---|---|---|
| Carapace nécrosée | 90211 | 90220 |
| Chair putride | 90213 | 90221 |
| Peste virulente | 90215 | 90222 |
| Sépulcre (held-back damage) | 90207 | 90208 |

Self and enemy versions never share an id, so a plague spread onto an enemy is never a gift to it. Sépulcre's
plague is not part of Virulence: it is spread and detonated like the others, but it only exists while there is
held-back damage to deal.

## Spell family

Every Pestiféré spell is spell family **16** (no stock spell uses it), each ability with its own flag, and
ChrClasses gives class 12 the same family. A talent modifier therefore reaches exactly the abilities its class
mask names, and no Warrior or Death Knight talent or script can reach the clones those abilities started from.

## Threat

Carrying **Carapace nécrosée** makes the Pestiféré a tank: while it is carried the carrier also has 90209, a hidden
+100% threat aura (x2 threat, like Defensive Stance), applied and removed with the plague. Morsure fétide adds flat
bonus threat through `spell_threat`.

## Scripts

- **`PestifereFrappePutrideSpellScript`** (90200) — Fossoyeur: with a two-hand weapon, extra damage and a second
  stack of Pourriture. Contagion galopante: a chance to reset Contagion's cooldown. Pandémie: the strike also lands
  on 3 nearby enemies (90217).
- **`PestiferePourritureAuraScript`** (90205) — Fièvre: each tick may make the next Morsure fétide free (90216,
  a -100% cost modifier) and reset its cooldown.
- **`PestifereMorsureFetideSpellScript`** (90224) — +10% damage per Pourriture stack on the target (not consumed);
  spends Fièvre.
- **`PestifereRipostePurulenteSpellScript`** (90225) — usable after a dodge, parry or block: rots its target and
  2 nearby enemies (more with Riposte fétide).
- **`PestifereFlaqueDeBileAuraScript`** (90223) — the pool at the caster's feet: damage and a Pourriture stack per
  tick, Bile corrosive's damage reduction.
- **`PestifereCarapaceSuintanteAuraScript`** (90226) — absorb of 4% maximum health + 3% per plague (Pus épais).
- **`PestifereBondPutrideSpellScript`** (90227) — the leap lands with a Pourriture stack.
- **`PestifereVomissureSpellScript`** (90284) — cone damage and 2 Pourriture stacks per enemy.
- **`PestifereAvatarDeLaPesteAuraScript`** (90287) — while it lasts the plagues count one more (on top of at least
  one carried) and Détonation keeps the Pourriture.
- **`PestifereContagionSpellScript`** (90201) — applies the enemy version of each carried plague (Sépulcre's
  included) to every enemy within the spell's radius (10 yd, widened by Miasme), refreshes the Pourriture
  already on them, adds threat per plague spread, then refreshes the carrier's own plagues.
- **`PestifereDetonationSpellScript`** (90202) — per enemy hit by the spell's area damage effect:
  `base + (stacks × perStack) × (1 + 0.5 × plaguesConsumed) + whatever Sépulcre still owed there`. Consumes the
  Pourriture stacks and the enemy plagues, refunds rage for each enemy detonated at 6 stacks, and heals the
  caster per enemy detonated. Détonation en chaîne then detonates the ripest enemies within 8 yd of one that
  exploded, outside the blast, through 90206.
- **`PestifereCharnierAmbulantSpellScript`** (90256) — every enemy within 10 yd receives every carried plague
  and 6 stacks of Pourriture, plus heavy threat.
- **`PestiferePurgeCathartiqueSpellScript`** (90265) — removes the three carried plagues and heals 12% of
  maximum health per plague removed. Fails without a plague. Sépulcre's damage stays owed.
- **`PestifereSepulcreAuraScript`** (90268) — an unlimited absorb that takes half of every hit for 8 sec and
  adds it to the carrier's stored plague.
- **`PestifereSepulcreStoredAuraScript`** (90207, 90208) — holds the damage owed and deals it over the ticks
  left, dealing each tick itself so the amount is exact (it was mitigated once already).
- **`PestifereMainsPutridesAuraScript`** (90243-90245) — the talent's proc (off-hand hits, chance per rank in
  the spell data) adds a stack of Pourriture.
- **`PestifereCarapaceNecroseeAuraScript`** (90211) — EFFECT_0 damage-taken reduction scaling with Virulence,
  plus Croûte nécrosée (per plague) and Résilience du porteur (at Virulence 3).
- **`PestifereChairPutrideAuraScript`** (90213) — EFFECT_0 periodic self-heal scaling with Virulence, plus
  Symbiose morbide.
- **`PestiferePesteVirulenteAuraScript`** (90215) — EFFECT_0 damage done, scaling with Virulence; EFFECT_1
  periodic self-damage that escalates while Peste virulente is the only plague carried, reduced by Porteur
  endurci.
- **`PestifereUnitScript`** — Rage fielleuse (extra rage from damage taken), Métabolisme nécrotique (rage from
  the carrier's own plague ticks, which normally give none), Menace contagieuse (extra threat from plague
  ticks), Chair putride's healing penalty (on its carrier, and on an enemy it was spread to), Charognard (a
  dying enemy's Pourriture jumps to the nearest enemy), Rigor mortis (a killing blow leaves the carrier at 1 health,
  once per 3 min: 90286) and Hôte parfait (parry per plague carried).
- **`PestiferePlayerScript`** — teaches the kit by level on login and level-up (class 12 only), starts and
  stops the plagues' out-of-combat countdown, and rescales the plagues when talents are learned or reset.

Every plague recalculates the whole set when one is added or removed, so a change in Virulence is felt at once.

All damage, healing and scaling coefficients are named constants at the top of `src/PestifereScripts.cpp`,
grouped per spell and meant to be tuned on a training dummy. Talent values per rank are in `src/Pestifere.h`.

## Talents ("Charnier")

11 tiers, capstone at 50 points, 65 ranks (grid: `localTools/customClasses/classes.json`).

| Tier | Talents |
|---|---|
| 0 | Peau coriace (data), Chair putride (teaches), Rage fielleuse |
| 1 | Peste virulente (teaches), Miasme (data), Inoculation rapide (data) |
| 2 | Mains putrides (proc), Contagion galopante, Symbiose morbide, Fossoyeur |
| 3 | Métabolisme nécrotique, Crocs infectés (data), Charnier ambulant (teaches) |
| 4 | Croûte nécrosée, Menace contagieuse, Porteur endurci |
| 5 | Fièvre, Résilience du porteur, Purge cathartique (teaches), Bile corrosive |
| 6 | Riposte fétide, Hôte parfait, Détonation en chaîne |
| 7 | Pus épais, Vomissure (teaches), Charognard |
| 8 | Rigor mortis, Pandémie prolongée (data) |
| 9 | Sépulcre (teaches, needs Rigor mortis) |
| 10 | Avatar de la peste (teaches, needs Sépulcre) |

"data" talents are spell modifiers or auras the core applies on its own; the others are read by the scripts through
the rank tables in `src/Pestifere.h`.

## Healer ("Sangsue", `src/PestifereHealer.cpp`)

The second tree (TalentTab 901, tab page 1; 11 tiers, capstone at 50 points, 70 ranks, spell ids 90300-90399).
The healer never casts. **Transfusion** (90300) trades the damage of its melee hits for healing. Once a hit's
damage is final, the core hook `UnitScript::ModifyFinalDamage` runs and does three things:

- it finds the most injured group member within 40 yd (the healer included);
- it moves the share of the hit that covers what that member is missing from the damage into the absorb;
- it heals the member for that share, logged as 90301.

A healthy group takes nothing, so the hit is pure damage. Rage is paid on the whole hit. Transfusion sleeps while
the Pestiféré carries Carapace nécrosée.

| Spell | Script | What the script does |
|---|---|---|
| 90302 Sangsue | `PestifereSangsueAuraScript` | drain from attack power, heals the most injured ally, jumps on death (Sangsue prolifère) |
| 90303 Saignée | (the damage hook) | its trade reaches 3 allies |
| 90304 Absorption morbide | `PestifereAbsorptionMorbideSpellScript` | draws a disease and a poison out of the most injured ally carrying one |
| 90305 Don de sang | `PestifereDonDeSangSpellScript` | caster's health for the most injured ally |
| 90306 / 90307 Symbiote | `PestifereSymbioteSpellScript`, `PestifereSymbioteAuraScript` | binds to the friendly target or the member with the most attackers; copies Transfusion healing |
| 90308 Pestilence salvatrice | (the damage hook) | the trade reaches 5 allies, double healing |
| 90309 Coagulation | `PestifereCoagulationAuraScript` | the talent's damage reduction on an ally healed |
| 90312 Spores | `PestifereSporesAuraScript` | Transfusion proc: heal over time sized from attack power |
| 90313 / 90314 | (the damage hook) | Pustule éclatante and Essaim procs: burst with splash, bouncing heal |
| 90315 / 90316 Brume pestilentielle | `PestifereBrumeSpellScript`, `PestifereBrumeAuraScript` | group heal over time, % of each member's health |

Contagion bénigne, Détonation salvatrice and Carapace partagée live in the Contagion, Détonation and Carapace
suintante scripts. Every heal goes through `HealAlly` (Triage, healing-taken modifiers, heal threat).

## Levelling

The class uses no trainer. `PestiferePlayerScript` grants 90200 and 90210 at level 1, 90201 at 6, 90202 at 10,
90203 at 14, 90204 at 20, Flaque de bile (90223) at 24, Morsure fétide (90224) at 30, Riposte purulente (90225) at 36,
Carapace suintante (90226) at 44, Bond putride (90227) at 50, Puanteur insoutenable (90228) at 60 and Pandémie
(90229) at 70.

## Spell data this module depends on

| Spell | Slot | Expected |
|---|---|---|
| 90200 Frappe putride | EFFECT_1 | triggers 90205 Pourriture |
| 90202 Détonation | EFFECT_0 | `SPELL_EFFECT_SCHOOL_DAMAGE`, caster-centred area |
| 90206 Détonation (chain) | EFFECT_0 | `SPELL_EFFECT_SCHOOL_DAMAGE`, single enemy |
| 90207 / 90208 Sépulcre | EFFECT_0 | `SPELL_AURA_PERIODIC_DAMAGE` |
| 90211 Carapace nécrosée | EFFECT_0 | `SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN` |
| 90213 Chair putride | EFFECT_0 | `SPELL_AURA_PERIODIC_HEAL` |
| 90215 Peste virulente | EFFECT_0 | `SPELL_AURA_MOD_DAMAGE_PERCENT_DONE` |
| 90215 Peste virulente | EFFECT_1 | `SPELL_AURA_PERIODIC_DAMAGE` |
| 90256 Charnier ambulant | EFFECT_0 | `SPELL_EFFECT_DUMMY`, enemies around the caster |
| 90265 Purge cathartique | EFFECT_0 | `SPELL_EFFECT_DUMMY` on the caster |
| 90268 Sépulcre | EFFECT_0 | `SPELL_AURA_SCHOOL_ABSORB` |
| 90243-90245 Mains putrides | EFFECT_0 | `SPELL_AURA_DUMMY` with off-hand proc flags |

A slot laid out differently simply keeps its DBC value: the matching handler never fires, and the worldserver
logs the mismatch at startup.

90203 Odeur de charogne, 90204 Crachat bilieux and 90205 Pourriture need no script: the taunt, the interrupt,
and the Pourriture damage and model swelling are all spell data.

## SQL

`data/sql/db-world/base/pestifere_scripts.sql` binds each script name to its spell id, the healer's included. The updater applies it
on the next worldserver start.
