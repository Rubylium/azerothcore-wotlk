# mod-rogue

The Rogue on the retail-style talent trees (localTools/rogue/talentTree.json, mod-custom-classes TalentTree.cpp): the
effects of its talents and abilities that spell data alone cannot carry. Spell data: localTools/rogue/Spells.ps1.

Every rogue also gets two changes:

- Backstab works from any side, and deals 20% more damage from behind (the "must be behind" requirement is dropped
  in data/sql/db-world/base/rogue_backstab.sql).
- Poisons put on a weapon never wear off: they are refreshed to a full hour every few minutes.

The third tree is Hors-la-loi, retail's Outlaw (it replaced the Crimson Duelist, whose spells 90010-90105 are taken
back from every rogue at login): see below.

## Finesse's and Assassinat's looks

Finesse and Assassinat wear retail's looks, put together from the effects and sounds the Ascension client carries
(localTools/rogue/ascensionVisuals.json, imported by localTools/ascensionImport/importVisuals.py into
client-assets/imported): Backstab, Ambush (Shadowstrike), Eviscerate, Hemorrhage and Shadow Dance, then Mutilate,
Envenom, Garrote, Rupture, Fan of Knives, Cold Blood and the Deadly and Instant Poison procs through
localTools/rogue/StockSpells.ps1, the specs' own abilities (Vendetta, Exsanguiner, Tempête cramoisie, Marqué pour la
mort) through Spells.ps1. What spell data cannot show is played
here, by kit id:

- Poudre noire's hit on every enemy it reaches (kit 77900), Technique secrète's on every enemy at each strike (77901).
- Technique secrète's two shadows: a creature (910300, data/sql/db-world/base/rogue_shadow_clone.sql) summoned on each
  side of the target for a moment, wearing the rogue's look and weapons (Mirror Image's Clone Me!) under a dark
  see-through skin (spell 92327), stabbing with its strike (kit 77902).

## Hors-la-loi (Outlaw)

src/RogueOutlaw.cpp, everything gated on the spec's passive (92194); design and ids in
.agents/plans/outlaw-rogue/outlaw-rogue.PLAN.md. Every number is a `rogue.outlaw_*` knob (`.tune list outlaw`).

- The kit's damage is worked out in code: Tir de pistolet (a share of a main-hand hit), Achever and Entre les deux
  yeux (WotLK's Eviscerate of the rogue's rank, times a factor), Ruée des lames, Série meurtrière, Déluge de lames'
  first strike, Main gauche. An ability whose data is a dummy has it dealt by the module; one whose data deals damage
  has that hit's amount replaced by the same number.
- Frappe sinistre strikes again now and then (Opportunité, Triple menace; Ambush too with Opportunité cachée);
  Opportunité makes the next Tir de pistolet free and stronger (Dégainer vite, Marteau en éventail's extra shots,
  Audace); Mèches de Peau-Verte, Atout dans la manche, Entre les deux yeux amélioré on Entre les deux yeux.
- Jeter les os removes its buffs and grants one or two of the six (Tour de passe-passe, Dés pipés after Poussée
  d'adrénaline); Bordée, Tête de mort and Cap assuré act here, the other three are their auras' own data. Compter les
  chances and Rejouer.
- Déluge de lames repeats every single-target hit of the rogue on the enemies around as plain damage (no proc, no
  repeat of a repeat), with its own hit's kit; Acier dansant or Coupes précises (a choice), Déluge amélioré,
  Manœuvres habiles.
- Built in: Lames sans repos (the choice 92220 then adds nothing), Potentiel de combat on off-hand hits (Fioriture
  fatale: more often), Cruauté (Cruauté accrue: a second point now and then).
- The tree's talents scale per rank (`Rank()` reads the highest rank spell; each per-rank number is a knob); its
  capstone is a choice, Lames d'effroi or Rejouer.
- Kits played by id: Déluge de lames' repeat on each enemy (77903), Série meurtrière's strike (77904), Main gauche
  (77905). Sounds through our own engine (mod-stat-growth EvolutionsAudio, `Outlaw.*` keys) for every player within
  40 yd who sees the rogue.
