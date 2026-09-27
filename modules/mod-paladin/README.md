# mod-paladin

The Paladin on the retail-style talent trees (localTools/paladin/talentTree.json, mod-custom-classes
TalentTree.cpp): Holy Power, and the effects of its talents and abilities that spell data alone cannot carry. Spell
data: localTools/paladin/Spells.ps1 (spell ids 92800-93099; new family flags in word 2, 0x200-0x40000000).

## Holy Power

Puissance sacrée (92900) is an aura of up to 5 stacks. Builders add to it as they are cast: Crusader Strike,
Judgement, Hammer of Wrath and Hammer of the Righteous 1, Holy Shock 1 (Holy), Blade of Justice 2, Wake of Ashes 3,
Divine Toll 1 per enemy hit, Avenger's Shield 1 with Bouclier béni, Infusion of Light's Flash of Light / Holy Light 1.
The finishers - Word of Glory (every spec), Light of Dawn (Holy), Shield of the Righteous (Protection), Templar's
Verdict and Divine Storm (Retribution) - cost no mana, cannot be cast below 3 (the cast fails with "can't do that yet")
and spend 3, unless a proc pays: Volonté divine (any), Puissance empyréenne (Divine Storm), Bastion de lumière (Shield
of the Righteous). Holy Power fades 10 s after combat.

## Tuning assumptions

The WotLK kit keeps its numbers; the new spells are sized against it at ~5000 attack power for Retribution and ~3000
spell power for Holy (raid gear before this server's paragon, which scales everything alike), with their coefficients
in `data/sql/db-world/base/paladin_spells.sql`:

- Retribution: one finisher every ~4 s (Crusader Strike 4 s, Judgement 8 s, Blade of Justice 8 s, Hammer of Wrath,
  Wake of Ashes). Templar's Verdict is 280% weapon damage plus 400 as holy (about 4.4 Crusader Strikes), Divine Storm
  160% on up to 5 enemies (6 in a dungeon, every enemy with Tempête vertueuse), so a pack of 4+ pays ~2.3x a single
  target, like the reworked Mage and Death Knight AoE. A Retribution bot is expected in the reworked Death Knight's
  order of magnitude (60-70k at +37-39), a human around the rogue's ~100k; an estimate from the coefficients, not a
  measurement: run keys and read `localTools/combatTelemetry` before retuning.
- Protection: Shield of the Righteous is off the global cooldown, 30% armor for 4.5 s stacking to 13.5 s, which a
  finisher every 3-4 s keeps up; Avenger's Shield every 15 s (5 targets with the talent, silencing), Eye of Tyr 25%
  less damage from a pack for 10 s every 45 s (30 with Vigilance), Ardent Defender (cheat death) and Guardian of
  Ancient Kings every 1 min 30 s, Consecration cheaper and faster, Sol consacré 10% less damage in it.
- Holy: Word of Glory ~7k and Light of Dawn ~3.5k on 5-7 allies around the Paladin; Holy Shock with 2-3 charges
  building Holy Power; the beacons relay 50% (Faith) or 40% between Virtue's 4 targets; Glimmer, Divine Toll and
  Avenging Crusader carry the M+ group healing.
- Cooldowns stay short (the key tools at 30-45 s, fillers at the global cooldown); Avenging Wrath drops to 2 min 30 s
  (Ardeur vengeresse; 1 min 30 s with Courroux sanctifié too) and Divine Protection / Divine Shield up to 1 min shorter.
