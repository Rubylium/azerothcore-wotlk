# mod-hunter

The Hunter on the retail-style talent trees (localTools/hunter/talentTree.json, mod-custom-classes TalentTree.cpp):
Focus, and the effects of its talents and abilities that spell data alone cannot carry. Spell data:
localTools/hunter/Spells.ps1 (spell ids 93100-93399; new family flags in word 2, 0x200000-0x40000000) and localTools/hunter/StockSpells.ps1 (the stock Hunter spells changed in place).

## Focus

The Hunter's power is Focus (Focalisation): power type 2, 100 at most, its bar the one the client already draws for
hunter pets. The server's ChrClasses gives the class that power (`powerType` in its classes.json stockTalentTrees entry,
written by buildCustomClasses.py), `Unit::GetCreatePowers` gives it a maximum of 100, and `Player::Regenerate`
regenerates it every update: 5 a second, faster with ranged haste (the quiver, haste rating, Rapid Fire; twice as fast
at most), and by the SPELL_AURA_MOD_POWER_REGEN_PERCENT auras (Concentration naturelle, Aspect de la bête féroce,
Visée parfaite). It is sent as it changes, and starts full.

Why Focus rather than mana with retail costs: the retail rotations are generator / spender loops (Kill Command and
Raptor Strike in Survival, Steady Shot and Aimed Shot in Marksmanship, Barbed Shot feeding Cobra Shot), which a mana
pool cannot express; mana on a Hunter also meant Aspect of the Viper and drinking, which retail removed. The core
already knew Focus (pets), so costs, the client's cost checks and tooltips, and the power bar all work as they are. The
Hunter keeps a mana pool (intellect), which nothing spends.

Costs (StockSpells.ps1): Arcane Shot and Multi-Shot 30 (no cooldown any more), Cobra Shot 35, Aimed Shot 35, Kill
Command 30 (free in Survival), Raptor Strike 30 (an instant strike on the global cooldown), Serpent Sting 15, Kill Shot
10, Chimera Shot 35, Volley 40; utility (traps, stings of control, Misdirection, Feign Death, Disengage, pet spells) is
free. Steady Shot costs nothing and gives 10 back; Survival's Kill Command gives 15, Barbed Shot and Bête sauvage 20
over 8 s, Frappe de flanc 30, Termes de l'engagement 20, each Rapid Fire shot 1, each chakram hit 3.

## Charges

Kill Command (2 in Beast Mastery with Prédateur alpha), Barbed Shot (2, 3 with Rechargement barbelé), Aimed Shot (2 in
Marksmanship), Wildfire Bomb (2 with Tactique de guérilla) and Butchery (3) have charges, shown as the stacks of an
aura (Charges de ...): the cast's cooldown is taken back while one is left, and the recharge runs on the spell's own
cooldown with the Hunter's modifiers, faster during Appel de la nature sauvage and Assaut coordonné (Kill Command,
50%) and Visée parfaite (Aimed Shot, twice). Tir du cobra, Découpe, Frappes frénétiques and Appel sauvage shorten the
recharge in progress.

## Tuning assumptions

The WotLK kit keeps its numbers; the new spells are sized against it at ~6000 ranged attack power (raid gear with
Aspect of the Dragonhawk, before this server's paragon, which scales everything alike), with their coefficients in
`data/sql/db-world/base/hunter_spells.sql` and the shares of the hits this module works out at the top of
`src/HunterTalents.cpp`:

- Beast Mastery plays through its pet: Kill Command 125% of the ranged attack power as the pet's hit (the pet's own
  bonuses then apply: Bestial Wrath, Furie déchaînée, Esprits apparentés; plus 15% from the spec and up to 20% from
  the talents), on a 7.5 s recharge sped up 1-2 s by each Cobra Shot. Barbed Shot keeps Frenzy up and Bestial Wrath
  coming back. AoE: Multi-Shot (5 targets) puts Beast Cleave on the pet, 80% of its swings and abilities (and of Kill
  Command with Ordre sauvage) on every enemy within 8 yd, Stomp on each Barbed Shot, wild beasts.
- Marksmanship: Aimed Shot is WotLK's (weapon damage + 407) with a 2.5 s cast, 2 charges on 12 s, 15% more from the
  spec and 20% from the talents, and gives 2 Precise Shots (+75-115% on the next Arcane Shots or Multi-Shots). Rapid
  Fire 6 x (180 + 12% ranged attack power) every 20 s. AoE: Multi-Shot on 3+ enemies gives Trick Shots, so the next
  Aimed Shot or Rapid Fire ricochets to 4 more for 55%; Volley every 45 s, 6 s of 9% a second on everything under it.
- Survival fights in melee: Kill Command free and giving 15 Focus every 6 s, Raptor Strike (weapon damage + 334, +20%
  from the spec, Mongoose Fury and Tip of the Spear on top) spending it. AoE: Wildfire Bomb every 18 s (45% of the
  ranged attack power on the target, as much on every enemy within 8 yd, all burning 6 x 4%, spreading Serpent Sting
  with Morsure de vipère), Butchery (3 charges) or Carve, Fury of the Eagle.
- The class tree carries Counter Shot (the interrupt, 24 s), Exhilaration, Tar Trap, Binding Shot, Camouflage,
  Survival of the Fittest, an improved Deterrence (Aspect de la tortue) and a capstone of Death Chakram or Stampede.
- Cooldowns stay short (the key tools 20-60 s, the big ones 1 min 30 s, Rapid Fire the haste one 2 min).

Telemetry before the rework put a Hunter bot near 47k in keys +37-39, against ~60-70k for the reworked Mage and Death
Knight bots and ~100k for a human rogue; the reworked Hunter is aimed at the Death Knight bot's order of magnitude.
Measured numbers are in the change that brought this module (and `localTools/combatTelemetry`): run keys and read them
before retuning.
