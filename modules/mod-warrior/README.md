# mod-warrior

The Warrior on the retail-style talent trees (localTools/warrior/talentTree.json, mod-custom-classes TalentTree.cpp):
Enrage, Meat Cleaver, Colossus Smash, Ignore Pain, the charges, and the effects of its talents and abilities that spell
data alone cannot carry. Spell data: localTools/warrior/Spells.ps1 (spell ids 95000-95299; new family flags in word 2,
0x1-0x10000) and localTools/warrior/StockSpells.ps1 (the stock Warrior spells changed in place).

## Any stance

The Warrior keeps rage and its three stances (Defensive Stance still carries a tank's threat), but no ability asks for
one any more: every specialization plays its whole kit in any stance, as retail does. Overpower needs no dodge,
Revenge no block, dodge or parry and strikes a cone, Execute's health check moves here, Slam is instant, Whirlwind has
no cooldown and no target cap (Bladestorm's whirls neither).

## The specializations

- **Armes** (spec passive 95280: Mortal Strike, Overpower, Execute and Slam +5%; Whirlwind, Bladestorm's whirls and
  Cleave +30%): Mortal Strike, Overpower on 2 charges (12 s), Execute below 20% (35% with Massacre, or any time on Sudden
  Death's proc), Colossus Smash (175% weapon damage, the target takes 20% more from the Warrior for 10 s, 45 s) or
  Warbreaker (150% and the same mark on every enemy within 8 yd), Skullsplitter (200%, 20 rage, 21 s), Sweeping Strikes,
  Bladestorm or the Ravager (a whirling axe thrown at a spot: a blow every second for 7 s within 8 yd, 5 rage each), Die
  by the Sword. Deep Wounds is the first node. Talents: Martial Prowess and Battlelord (Overpower feeds Mortal Strike),
  Executioner's Precision, Tactician (every 10 rage spent may bring an Overpower charge back), In for the Kill,
  Unhinged (free Mortal Strikes while whirling) or Merciless Bonegrinder (Whirlwind and Cleave +50% after it).
- **Fureur** (95281: Whirlwind +30%, off hand +10%, 60% less rage from auto attacks so the builders feed Rampage):
  Bloodthirst (8 rage; a critical strike or 30% of the time, Enrage), Raging Blow (both weapons at 130%, 12 rage, 2
  charges on 8 s, 3 with Rage incontrôlée), Rampage (80 rage, four blows at 60%, Enrage), Onslaught (200%, 15 rage, 18
  s), Odyn's Fury (fire around the Warrior and a 4 s burn, Enrage, 45 s), Execute, Recklessness (20% critical strike for
  12 s, 1 min 30 s). Enrage (Enragé, 95128): 25% melee haste for 4 s. Meat Cleaver: Whirlwind gives 2 charges (4 with
  Fendoir affûté), each single-target attack (Bloodthirst, Raging Blow, every blow of Rampage, Onslaught, Execute) also
  strikes 4 enemies near its target for 90% of its damage. Titan's Grip comes with the specialization.
- **Protection** (95282: Stamina and armor +10%; Thunder Clap, Revenge and Shield Slam +30%): Shield Slam (15 rage),
  Thunder Clap (5 rage), Revenge (20 rage, a cone in front; free after a dodge, a parry or a block, at most every 3 s),
  Devastate, Ignore Pain (35 rage, off the global cooldown: an absorb of 2x attack power taking half of each hit, topped
  up by each cast up to 30% of maximum health, 12 s), Shield Block (6 s, 2 charges on 16 s), Last Stand, Shield Wall (3
  min), Shield Charge (a charge with the shield up to 25 yd, 150% weapon damage on the target and 4 enemies near it, 20
  rage, 45 s), the Ravager, Disrupting Shout or Spell Block.
- **Gladiateur** (95283: half the threat, Shield Slam recharges in 12 s; the fourth tree, a damage dealer with a
  one-handed weapon and a shield, no talent tab of its own: the stock systems and the bots see it as index 3). Shield
  Slam gives 20 rage and Ouverture (95160, 8 s): Revenge needs it, and costs nothing under it. Revenge leaves Plaie du
  gladiateur (95158) on its target (every enemy it hits with Revers): a bleed of a second's ticks for 6 s that rolls
  like Ignite (each one adds 40% of the hit to what is left and starts over), each tick with a 6% chance (9% more with
  Sang et sable, one roll per tick however many enemies bleed) to bring Shield Slam back. Devastate gives 5 rage and
  Garde brisée (95161: the next Revenge 20% stronger, more with Garde fracassée). Shield Slam under Shield Block keeps
  the bleed rolling. Execute is usable on any target bleeding of it, spends all rage up to 100 (each rage worth what
  the core's 30 are) and consumes the bleed: what is left of it at once (95159; 50% more with Pouce baissé, half with
  Panem et circenses, the other half bleeding on; Hémorragie contagieuse spreads half of it within 8 yd). The shield's
  defense, dodge, parry and block ratings become critical strike rating (95162); Tranchant du bouclier adds a share of
  its block value to every weapon blow. Duel (95150, Intervene's charge to an ally): both gain 15% attack power and
  spell damage and healing (95155) for 15 s under the duel flag, and the Warrior takes 30% of what the ally takes
  (95163, a split) until it falls under 35% health. Lancer de bouclier (Avenger's Shield's bounce, Sunder Armor and
  the bleed on each enemy), Tempête de l'arène (Bladestorm's spin, its whirls feed the bleed) or Coup de grâce (the
  next Execute free and as with 100 rage). The tree is an arena: the sword wall (single target) and the shield wall
  (AoE) down either side, the Duel talents at its exit.
- **Class tree** (Guerrier): Heroic Leap (to a spot 8-40 yd away, weapon damage within 8 yd of the landing, 45 s),
  Rallying Cry (15% maximum health to the raid for 10 s, 3 min), Storm Bolt or Shockwave, Avatar (20% damage for 20 s,
  1 min 30 s), Thunderous Roar (physical damage within 12 yd and an 8 s bleed, 1 min 30 s), Champion's Spear or Heroic
  Fury, Second Wind, and the WotLK utility talents (Improved Charge, Iron Will, Cruelty, Precision, Armored to the
  Teeth, Anger Management, Endless Rage, Safeguard, Improved Disciplines...).

Charges (shown as the stacks of an aura, like the Hunter's and the Priest's): Overpower (2, Arms), Raging Blow (2, 3 with
Rage incontrôlée, Fury), Shield Block (2, Protection).

The trees reuse WotLK talent ranks (Deep Wounds, Flurry, Sword and Board...). A WotLK talent aura the Warrior no longer
knows the spell of is dropped by mod-custom-classes (TalentTree.cpp, every class on the trees): a bot moved from Arms to
Fury kept Deep Wounds' aura until its next login.

## Rage

Rage still comes from auto attacks and damage taken, which at this server's gear fills it quickly (Fury takes 60% less
from its swings); the builders give it anyway (Bloodthirst 8, Raging Blow 12, Onslaught 15, Skullsplitter 20, Shield
Slam 15, Thunder Clap 5, the Ravager 5 a blow, Champion's Spear 10, Shield Charge 20, Reckless Abandon 50), and the
spenders cost it in their spell data (Mortal Strike 30, Rampage 80, Revenge 20, Ignore Pain 35, Whirlwind 25, Execute
15 plus up to 30).

## Tuning

The WotLK kit keeps its numbers; the new weapon strikes are weapon percentages next to Mortal Strike's, the other new
spells attack power coefficients in `data/sql/db-world/base/warrior_spells.sql`, and the shares this module works out at
the top of `src/WarriorTalents.cpp`. Past twelve enemies each area hit takes sqrt(12 / enemies): the casters' falloff
starts at five, but the Warrior's areas are 8 yd around it, so a big pack is already only partly in reach.

Measured on the combat bench (`localTools/combatBench/runBench.ps1`, 2026-10-01, key +10, item level ~244, 60 s, two
rounds each, the Fire Mage in the same runs; the warrior words are `arms`, `fury` and `prot`):

| | Fire Mage | Arms | Fury |
|---|---|---|---|
| single (single-target build) | 4.5k | 5.5k (120%) | 5.5k (120%) |
| pack of 5 (AoE build) | 11.4k | 11.4k (100%) | 12.7k (111%) |
| pack of 12 (AoE build) | 26.4k | 20.1k (76%) | 18.2k (69%) |

The other reworked melee measured in the same session: Retribution 174% / 134% / 75%, Frost Death Knight 114% / 181% /
80%. Bench a tank on its own (`tank`, `tankpack` layouts): a tank bot in the run drags the damage dealers' numbers down
(their threat strategy holds their abilities back). Protection, alone: about 2.6k damage and 790 damage taken a second
on the boss (Blood Death Knight 1.3k / 900, Protection Paladin 2.6k / 1.5k), 5.4k damage on the pack of five brutes,
where it dies after about a minute without a healer like the other two.

Bots: the retail actions sit above the stock Warrior ones and every one checks its spell is known
(mod-playerbots `WarriorRetail.h`); Arms and Fury bots no longer press Heroic Strike.
