# Oathblade spellbook and talent ranks. All icons remain stock until the separate art pass.
$classMask = 512
$skillLine = 902
$family = 19
# Field 206 is the global cooldown in milliseconds. 900 rather than the usual 1500: this class is meant to
# be pressed fast, and as SPELL_DAMAGE_CLASS_MELEE spells (204 = 2) these never had haste applied to their
# global cooldown - Spell::TriggerGlobalCooldown excludes melee from that - so 1500 was a hard floor that no
# gear could move. Anything under MIN_GCD (1000) skips the clamp in that function and is used as written.
$gcd = 900
$meleeFields = @{ 12 = 0; 13 = 0; 28 = 1; 41 = 3; 68 = 2; 69 = 128; 70 = 0; 204 = 2; 205 = 133; 206 = $gcd; 208 = $family }
$selfFields = @{ 12 = 0; 13 = 0; 28 = 1; 41 = 3; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 204 = 2; 205 = 133; 206 = $gcd; 208 = $family }
$damage = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
$dummy = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 })
$spells = @(
    @{ Id = 90800; Clone = 1752; Name = 'Swift Cut'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'A free sword cut that restores 22 Energy and grants Flow. Alternate it with Precise Thrust to gain additional Flow. Never leaves you waiting for an auto-attack.'
       Effects = $damage; Fields = $meleeFields; Visual = @{ Clone = 253; Cast = 'OB_Cast_SwiftCut'; Impact = 'OB_Imp_Frost' } },
    @{ Id = 90801; Clone = 2983; Name = 'Flow'; FallbackIconSpell = 5171; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 5
       Description = 'Precise sword techniques build Flow, up to 5. Finishers spend all available Flow.'
       AuraDescription = 'Flow increases the strength of your finishing techniques.'
       Fields = @{ 40 = 21; 208 = $family } },
    @{ Id = 90802; Clone = 1752; Name = 'Precise Thrust'; FallbackIconSpell = 14251; Cost = 20; Cooldown = 0; Level = 4; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'A focused thrust that grants 2 Flow. Alternating with Swift Cut grants another Flow.'
       Effects = $damage; Fields = $meleeFields; Visual = @{ Clone = 7660; Cast = 'OB_Cast_Thrust'; Impact = 'OB_Imp_Frost' } },
    # AttributesEx is set here rather than inherited: Eviscerate is a finisher and its combo-point
    # requirement comes with the clone. Flow is this class's resource and the server owns it.
    @{ Id = 90803; Clone = 2098; Name = 'Noble Verdict'; FallbackIconSpell = 2098; Cost = 15; Cooldown = 0; Level = 6; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Finishing technique. Requires at least 2 Flow and spends up to 5. Damage rises sharply with Flow spent; restores Energy and builds Tempo.'
       Effects = $damage; Fields = ($meleeFields + @{ 5 = 0x00000200 }); Visual = @{ Clone = 11613; Cast = 'OB_Cast_Verdict'; Impact = 'OB_Imp_Verdict' } },
    @{ Id = 90804; Clone = 51723; Name = 'Sweeping Arc'; FallbackIconSpell = 51723; Cost = 25; Cooldown = 6000; Level = 10; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Sweep your blade through up to 6 nearby enemies. Grants 1 Flow, or 2 when hitting at least 3 enemies.'
       Effects = $dummy; Fields = $selfFields; Visual = @{ Clone = 12006; Cast = 'OB_Cast_Sweep'; Impact = 0 } },
    @{ Id = 90805; Clone = 2983; Name = 'Flawless Form'; FallbackIconSpell = 13750; Cost = 0; Cooldown = 0; Level = 12; Spellbook = $false; DummyAura = $true
       Description = 'Your techniques and finishers build Tempo. At 12 Tempo, enter Flawless Form for 8 sec: attack faster, strike a second time with techniques and restore Energy with every technique.'
       Fields = @{ 4 = 448; 40 = 21; 208 = $family } },
    @{ Id = 90806; Clone = 14251; Name = 'Reversal'; FallbackIconSpell = 14251; Cost = 0; Cooldown = 10000; Level = 18; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'A swift counterattack after Precise Thrust. Grants Flow and Tempo.'
       Effects = $damage; Fields = $meleeFields; Visual = @{ Clone = 8316; Cast = 'OB_Cast_Reversal'; Impact = 'OB_Imp_Oath' } },
    @{ Id = 90807; Clone = 20252; Name = 'Noble Advance'; FallbackIconSpell = 20252; Cost = 15; Cooldown = 18000; Level = 24; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Rush to your opponent and deliver a sword strike. Grants Flow and Tempo.'
       Fields = $meleeFields; Visual = @{ Clone = 8316; Cast = 'OB_Cast_Advance'; Impact = 'OB_Imp_Oath' } },
    @{ Id = 90808; Clone = 51723; Name = 'Crescent Sweep'; FallbackIconSpell = 51690; Cost = 15; Cooldown = 10000; Level = 30; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Area finishing technique. Requires at least 2 Flow and spends up to 5, striking up to 6 nearby enemies. Damage scales with Flow spent.'
       Effects = $dummy; Fields = $selfFields; Visual = @{ Clone = 12006; Cast = 'OB_Cast_Crescent'; Impact = 0 } },
    @{ Id = 90809; Clone = 5277; Name = 'Blade Ward'; FallbackIconSpell = 5277; Cost = 0; Cooldown = 30000; Level = 36; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Defensive form. Reduces damage taken by 25% for 6 sec.'
       AuraDescription = 'Damage taken reduced by 25%.'
       DummyAura = $true; Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 }); Fields = $selfFields
       Visual = @{ Clone = 253; Cast = 'OB_Ward_Cast'; Impact = 0; State = 'OB_Ward_State' } },
    @{ Id = 90810; Clone = 2983; Name = 'Flourish'; FallbackIconSpell = 13750; Cost = 0; Cooldown = 20000; Level = 42; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Recover 45 Energy, gain 2 Flow and advance 2 Tempo.'
       Effects = $dummy; Fields = $selfFields; Visual = @{ Clone = 253; Cast = 'OB_Flourish_Cast'; Impact = 0 } },
    @{ Id = 90811; Clone = 1752; Name = 'Zeal Strike'; FallbackIconSpell = 5938; Cost = 25; Cooldown = 8000; Level = 50; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'A decisive sword strike that grants 2 Flow and 2 Tempo.'
       Effects = $damage; Fields = $meleeFields; Visual = @{ Clone = 11612; Cast = 'OB_Cast_Zeal'; Impact = 'OB_Imp_Frost' } },
    @{ Id = 90812; Clone = 8696; Name = 'Rally'; FallbackIconSpell = 48982; Cost = 0; Cooldown = 45000; Level = 60; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Recover 18% of your maximum health.'
       Effects = $dummy; Fields = $selfFields; Visual = @{ Clone = 253; Cast = 'OB_Rally_Cast'; Impact = 0 } },
    @{ Id = 90813; Clone = 2098; Name = 'Final Edict'; FallbackIconSpell = 48668; Cost = 20; Cooldown = 15000; Level = 70; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Your strongest single-target finishing technique. Requires at least 2 Flow and spends up to 5. A precise, devastating sword impact.'
       Effects = $damage; Fields = ($meleeFields + @{ 5 = 0x00000200 }); Visual = @{ Clone = 11613; Cast = 'OB_Cast_Edict'; Impact = 'OB_Imp_Edict' } },
    @{ Id = 90825; Clone = 2983; Name = 'Tempo'; FallbackIconSpell = 13750; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 12
       AuraDescription = 'At 12 Tempo, Flawless Form activates.'; Fields = @{ 40 = 21; 208 = $family } },
    @{ Id = 90826; Clone = 2983; Name = 'Flawless Form'; FallbackIconSpell = 13750; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       AuraDescription = 'Attack speed increased. Your techniques strike again and restore Energy.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 30 }); Fields = @{ 40 = 29; 208 = $family }
       # The burst, seen and heard: a frost blast when it starts, the body lit blue with glowing blades while it
       # lasts, a shatter when it ends (patchSinisterStrike.ps1, OB_Burst_*). Lichborne's visual underneath, every
       # slot of it replaced, so nothing of the clone's own look or sound is left.
       Visual = @{ Clone = 11706; Precast = 0; Cast = 'OB_Burst_Cast'; State = 'OB_Burst_State'
                   StateDone = 'OB_Burst_End'; Impact = 0 } },
    @{ Id = 90827; Clone = 2983; Name = 'Swift Cut'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true
       AuraDescription = 'Alternate with Precise Thrust for additional Flow.'; Fields = @{ 40 = 27; 208 = $family } },
    @{ Id = 90828; Clone = 2983; Name = 'Precise Thrust'; FallbackIconSpell = 14251; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true
       AuraDescription = 'Alternate with Swift Cut for additional Flow. Reversal is ready.'; Fields = @{ 40 = 27; 208 = $family } },
    @{ Id = 90829; Clone = 1752; Name = 'Second Strike'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Effects = $damage; Fields = @{ 12 = 0; 13 = 0; 28 = 1; 41 = 3; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 204 = 2; 205 = 0; 206 = 0; 208 = $family }
       # The burst's echo: no swing of its own (the technique that triggered it is the swing), but its own
       # crackling arcane-frost impact and report, so every technique during Flawless Form lands twice, visibly
       # and audibly. Cloned from Sinister Strike, its cast kit is cleared so that strike's sound is not heard.
       Visual = @{ Clone = 253; Cast = 0; Impact = 'OB_Imp_Echo' } },
    # Damage carriers for the area abilities. Sweep() casts one of these at each enemy, and the
    # combat log names the spell that was cast - so each ability needs its own or they all read
    # as the same thing. Never in the spellbook; they only exist to carry a number and a name.
    @{ Id = 90831; Clone = 1752; Name = 'Sweeping Arc'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Effects = $damage; Fields = @{ 5 = 0; 12 = 0; 13 = 0; 28 = 1; 41 = 3; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 204 = 2; 205 = 0; 206 = 0; 208 = $family }
       # Silent: the ability that cast this already played its own recording
       Visual = @{ Clone = 12006; Cast = 0; Impact = 'OB_Imp_Storm' } },
    @{ Id = 90832; Clone = 1752; Name = 'Crescent Sweep'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Effects = $damage; Fields = @{ 5 = 0; 12 = 0; 13 = 0; 28 = 1; 41 = 3; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 204 = 2; 205 = 0; 206 = 0; 208 = $family }
       # Silent: the ability that cast this already played its own recording
       Visual = @{ Clone = 12006; Cast = 0; Impact = 'OB_Imp_Storm' } },
    @{ Id = 90833; Clone = 1752; Name = 'Blade Dance'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Effects = $damage; Fields = @{ 5 = 0; 12 = 0; 13 = 0; 28 = 1; 41 = 3; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 204 = 2; 205 = 0; 206 = 0; 208 = $family }
       # Silent: the ability that cast this already played its own recording
       Visual = @{ Clone = 12006; Cast = 0; Impact = 'OB_Imp_Knives' } },
    @{ Id = 90834; Clone = 1752; Name = 'Grand Flourish'; FallbackIconSpell = 1752; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Effects = $damage; Fields = @{ 5 = 0; 12 = 0; 13 = 0; 28 = 1; 41 = 3; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 204 = 2; 205 = 0; 206 = 0; 208 = $family }
       # Silent: the ability that cast this already played its own recording
       Visual = @{ Clone = 12006; Cast = 0; Impact = 'OB_Imp_Shock' } },
    @{ Id = 90830; Clone = 703; Name = 'Serrated Cuts'; FallbackIconSpell = 703; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; BleedAura = $true
       # Silent: a bleed kept Rend's impact and spoke again every time it was refreshed
       Visual = @{ Clone = 757; Impact = 0 }
       AuraDescription = 'Bleeding from a precise sword cut.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = @{ 40 = 28; 98 = 3000; 208 = $family } },
    @{ Id = 90942; Clone = 51723; Name = 'Blade Dance'; FallbackIconSpell = 51723; Cost = 20; Cooldown = 12000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Talent. A rapid circular sword attack against up to 6 nearby enemies. Generates Flow and refunds 20 Energy.'
       Effects = $dummy; Fields = $selfFields; Visual = @{ Clone = 12006; Cast = 'OB_Cast_BladeDance'; Impact = 0 } },
    @{ Id = 90953; Clone = 51723; Name = 'Grand Flourish'; FallbackIconSpell = 51690; Cost = 20; Cooldown = 30000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Talent. A powerful area finisher that spends 2-5 Flow and hits up to 6 nearby enemies.'
       Effects = $dummy; Fields = $selfFields; Visual = @{ Clone = 12006; Cast = 'OB_Cast_Flourish'; Impact = 0 } },
    # Pommel Strike: the class tree's interrupt (talentTree.json node 105). Kick's own effects, unchanged: interrupt
    # and lock the school out for 5 sec. Reversal's sword swing and blue impact, so it reads as this class's.
    @{ Id = 91004; Clone = 1766; Name = 'Pommel Strike'; IconPath = 'Interface\Icons\Ability_Warrior_PunishingBlow'; Cost = 15; Cooldown = 12000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Talent. Strike the target with your pommel, interrupting spellcasting and preventing any spell in that school from being cast for 5 sec.'
       Fields = $meleeFields; Visual = @{ Clone = 8316; Cast = 'OB_Cast_Reversal'; Impact = 'OB_Imp_Oath' } }
)

# Talent ranks come from the talent trees (talentTree.json), which the server and the talent window read as well
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\oathblade\talentTree.json') -Family $family

$spells
