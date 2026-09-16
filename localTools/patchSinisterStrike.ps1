$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$serverDbcRoot = Join-Path $repoRoot 'server\Data\dbc'
$clientDbcRoot = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK\Data\DBFilesClient'
$serverSpellPath = Join-Path $serverDbcRoot 'Spell.dbc'
$spellBackupPath = Join-Path $serverDbcRoot 'Spell.before-sinister-strike.dbc'
$serverSkillPath = Join-Path $serverDbcRoot 'SkillLineAbility.dbc'
$skillBackupPath = Join-Path $serverDbcRoot 'SkillLineAbility.before-rogue-momentum.dbc'
$serverIconPath = Join-Path $serverDbcRoot 'SpellIcon.dbc'
$iconBackupPath = Join-Path $serverDbcRoot 'SpellIcon.before-rogue-momentum.dbc'
$clientSpellPath = Join-Path $clientDbcRoot 'Spell.dbc'
$clientSkillPath = Join-Path $clientDbcRoot 'SkillLineAbility.dbc'
$clientIconPath = Join-Path $clientDbcRoot 'SpellIcon.dbc'

$sinisterStrikeRanks = @(1752, 1757, 1758, 1759, 1760, 8621, 11293, 11294, 26861, 26862, 48637, 48638)
$customSpells = @(
    @{ Id = 90010; Clone = 1752; Name = 'Quick Cut'; IconPath = 'Interface\Icons\RogueMomentum_QuickCut'; Description = 'Consumes Opening to carve through the target for 175% damage and generate an additional combo point.'; Cost = 45; Cooldown = 0; Level = 4; Spellbook = $true },
    @{ Id = 90011; Clone = 36554; Name = 'Shadow Lunge'; IconPath = 'Interface\Icons\RogueMomentum_ShadowLunge'; Description = 'Step through the shadows to your target, strike for 150% weapon damage, and generate a combo point. Resets whenever you kill an enemy.'; Cost = 0; Cooldown = 10000; Level = 8; Spellbook = $true; ShadowLungeDamage = $true },
    @{ Id = 90012; Clone = 14278; Name = 'Riposte'; IconPath = 'Interface\Icons\RogueMomentum_Riposte'; Description = 'Turn defense into offense with a vicious counterattack. Deals 165% weapon damage, grants a combo point, restores 20 Energy, and increases dodge by 15% for 6 sec.'; Cost = 0; Cooldown = 8000; Level = 10; Spellbook = $true },
    @{ Id = 90013; Clone = 2983; Name = 'Sanguine Veil'; IconPath = 'Interface\Icons\RogueMomentum_SanguineVeil'; Description = 'Shroud yourself in a sanguine veil for 15 sec, healing for 50% of all effective damage you deal.'; AuraDescription = 'Healing for 50% of all effective damage dealt.'; Cost = 25; Cooldown = 20000; Level = 10; DummyAura = $true; Spellbook = $true },
    @{ Id = 90014; Clone = 2983; Name = 'Opening'; IconPath = 'Interface\Icons\RogueMomentum_Opening'; Description = 'Your next Quick Cut consumes Opening to deal 175% damage and generate an additional combo point.'; AuraDescription = 'Quick Cut is empowered.'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 1; Spellbook = $false },
    @{ Id = 90015; Clone = 2983; Name = 'Battle Tempo'; IconPath = 'Interface\Icons\RogueMomentum_BattleTempo'; Description = 'Finishing moves grant 3% attack speed per combo point for 8 sec.'; AuraDescription = 'Attack speed increased by 3% per stack.'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false },
    @{ Id = 90016; Clone = 2983; Name = 'Killing Momentum'; IconPath = 'Interface\Icons\RogueMomentum_KillingMomentum'; Description = 'Kills grant 5% movement speed and Energy regeneration per stack for 6 sec, stacking up to 5 times.'; AuraDescription = 'Movement speed and Energy regeneration increased by 5% per stack.'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false },
    @{ Id = 90017; Clone = 51723; Name = 'Crimson Sweep'; IconPath = 'Interface\Icons\RogueMomentum_CrimsonSweep'; Description = 'Sweep through nearby enemies for 175% weapon damage and apply Crimson Wounds for 6 sec. Each bleed tick restores 3 Energy. Opening increases all damage by 50%, extends the bleed to 10 sec, and grants a combo point.'; Cost = 35; Cooldown = 6000; Level = 18; Spellbook = $true; NoEquipment = $true; CrimsonSweep = $true },
    @{ Id = 90018; Clone = 1943; Name = 'Crimson Wounds'; IconPath = 'Interface\Icons\RogueMomentum_CrimsonSweep'; Description = 'Bleeding from Crimson Sweep.'; AuraDescription = 'Bleeding and restoring Energy to the Rogue every 2 sec.'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; BleedAura = $true },
    @{ Id = 90019; Clone = 8690; Name = 'Quick Travel'; IconPath = 'Interface\Icons\INV_Misc_Rune_01'; Description = 'Teleport to the selected flight master after a 3 sec cast.'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TravelCast = $true },
    @{ Id = 90020; Clone = 2983; Name = 'Guarded Rhythm'; IconPath = 'Interface\Icons\Ability_Rogue_SinisterCalling'; Description = 'Your relentless assault keeps you ready to turn aside incoming blows.'; AuraDescription = 'Damage taken reduced by Vanguard''s Rhythm.'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 1; Spellbook = $false },
    @{ Id = 90021; Clone = 2565; Name = 'Gladiator Stance'; IconPath = 'Interface\Icons\Ability_Warrior_GladiatorStance'; Description = 'Fight as a gladiator while in Defensive Stance with a shield equipped. Doubles all damage you deal and all damage you take before other reductions. Cast again to leave.'; AuraDescription = 'Damage dealt and damage taken increased by 100%.'; Cost = 0; Cooldown = 0; Level = 10; Spellbook = $true; GladiatorStance = $true; SkillLine = 257; ClassMask = 1 }
)
$spellbookSpells = @($customSpells | Where-Object { $_.Spellbook })
$talentReworks = @{
    13732 = @{ Name = 'Vanguard''s Rhythm'; Description = 'Sinister Strike and Quick Cut deal 10% more damage. Hitting with either grants Guarded Rhythm for 4 sec, reducing damage taken by 3%.'; PassiveDummy = $true }
    13863 = @{ Name = 'Vanguard''s Rhythm'; Description = 'Sinister Strike and Quick Cut deal 20% more damage. Hitting with either grants Guarded Rhythm for 4 sec, reducing damage taken by 6%.'; PassiveDummy = $true }
    14251 = @{ Name = 'Riposte Mastery'; Description = 'Riposte deals 30% more damage and restores 20 additional Energy. While its dodge bonus is active, damage taken is reduced by 10%.'; PassiveDummy = $true }
    5952  = @{ Name = 'Crimson Reach'; Description = 'Increases the radius of Crimson Sweep by 2 yards and the damage of Crimson Wounds by 15%.'; PassiveDummy = $true }
    51679 = @{ Name = 'Crimson Reach'; Description = 'Increases the radius of Crimson Sweep by 4 yards and the damage of Crimson Wounds by 30%.'; PassiveDummy = $true }
    18427 = @{ Name = 'Aggression'; Description = 'Increases the damage of Sinister Strike, Quick Cut, Shadow Lunge, Riposte, Crimson Sweep, Backstab, and Eviscerate by 3%.' }
    18428 = @{ Name = 'Aggression'; Description = 'Increases the damage of Sinister Strike, Quick Cut, Shadow Lunge, Riposte, Crimson Sweep, Backstab, and Eviscerate by 6%.' }
    18429 = @{ Name = 'Aggression'; Description = 'Increases the damage of Sinister Strike, Quick Cut, Shadow Lunge, Riposte, Crimson Sweep, Backstab, and Eviscerate by 9%.' }
    61330 = @{ Name = 'Aggression'; Description = 'Increases the damage of Sinister Strike, Quick Cut, Shadow Lunge, Riposte, Crimson Sweep, Backstab, and Eviscerate by 12%.' }
    61331 = @{ Name = 'Aggression'; Description = 'Increases the damage of Sinister Strike, Quick Cut, Shadow Lunge, Riposte, Crimson Sweep, Backstab, and Eviscerate by 15%.' }
}

function Assert-Wdbc([byte[]]$data, [string]$name) {
    if ($data.Length -lt 21 -or [Text.Encoding]::ASCII.GetString($data, 0, 4) -ne 'WDBC') {
        throw "$name has an invalid WDBC header."
    }
}

function Set-Field([byte[]]$record, [int]$field, [uint32]$value) {
    [BitConverter]::GetBytes($value).CopyTo($record, $field * 4)
}

function Add-DbcString([Collections.Generic.List[byte]]$strings, [string]$value) {
    $offset = $strings.Count
    $strings.AddRange([Text.Encoding]::UTF8.GetBytes($value))
    $strings.Add(0)
    return [uint32]$offset
}

if (-not (Test-Path -LiteralPath $spellBackupPath)) {
    Copy-Item -LiteralPath $serverSpellPath -Destination $spellBackupPath
}
if (-not (Test-Path -LiteralPath $skillBackupPath)) {
    Copy-Item -LiteralPath $serverSkillPath -Destination $skillBackupPath
}
if (-not (Test-Path -LiteralPath $iconBackupPath)) {
    Copy-Item -LiteralPath $serverIconPath -Destination $iconBackupPath
}

$iconSource = [IO.File]::ReadAllBytes($iconBackupPath)
Assert-Wdbc $iconSource 'SpellIcon.dbc'
$iconCount = [BitConverter]::ToInt32($iconSource, 4)
$iconRecordSize = [BitConverter]::ToInt32($iconSource, 12)
$iconStringSize = [BitConverter]::ToInt32($iconSource, 16)
if ([BitConverter]::ToInt32($iconSource, 8) -ne 2 -or $iconRecordSize -ne 8) { throw 'Unexpected SpellIcon.dbc layout.' }
$iconRecordsSize = $iconCount * $iconRecordSize
$iconStringOffset = 20 + $iconRecordsSize
$iconStrings = [Collections.Generic.List[byte]]::new()
$iconStrings.AddRange([byte[]]$iconSource[$iconStringOffset..($iconStringOffset + $iconStringSize - 1)])
$maxIconId = 0
for ($index = 0; $index -lt $iconCount; ++$index) {
    $iconId = [BitConverter]::ToUInt32($iconSource, 20 + $index * $iconRecordSize)
    if ($iconId -gt $maxIconId) { $maxIconId = $iconId }
}
$customIconIds = @{}
$customIconRecords = [Collections.Generic.List[byte]]::new()
for ($index = 0; $index -lt $customSpells.Count; ++$index) {
    $custom = $customSpells[$index]
    $iconId = [uint32]($maxIconId + $index + 1)
    $customIconIds[[int]$custom.Id] = $iconId
    $pathOffset = Add-DbcString $iconStrings $custom.IconPath
    $record = [byte[]]::new($iconRecordSize)
    Set-Field $record 0 $iconId
    Set-Field $record 1 $pathOffset
    $customIconRecords.AddRange($record)
}
$newIconCount = $iconCount + $customSpells.Count
$iconOutput = [byte[]]::new(20 + ($newIconCount * $iconRecordSize) + $iconStrings.Count)
[Array]::Copy($iconSource, 0, $iconOutput, 0, 20 + $iconRecordsSize)
[BitConverter]::GetBytes([uint32]$newIconCount).CopyTo($iconOutput, 4)
[BitConverter]::GetBytes([uint32]$iconStrings.Count).CopyTo($iconOutput, 16)
$customIconRecords.CopyTo($iconOutput, 20 + $iconRecordsSize)
$iconStrings.CopyTo($iconOutput, 20 + ($newIconCount * $iconRecordSize))

$source = [IO.File]::ReadAllBytes($spellBackupPath)
Assert-Wdbc $source 'Spell.dbc'
$recordCount = [BitConverter]::ToInt32($source, 4)
$fieldCount = [BitConverter]::ToInt32($source, 8)
$recordSize = [BitConverter]::ToInt32($source, 12)
$stringSize = [BitConverter]::ToInt32($source, 16)
if ($fieldCount -lt 234 -or $recordSize -lt 936) { throw 'Unexpected Spell.dbc layout.' }
$recordBlockSize = $recordCount * $recordSize
$stringOffset = 20 + $recordBlockSize
$records = [byte[]]::new($recordBlockSize)
[Array]::Copy($source, 20, $records, 0, $records.Length)
$strings = [Collections.Generic.List[byte]]::new()
$strings.AddRange([byte[]]$source[$stringOffset..($stringOffset + $stringSize - 1)])
$sinisterDescription = Add-DbcString $strings 'Strike instantly for weapon damage, generate a combo point and restore 45 Energy. Grants Opening for Quick Cut.'
$foundSinister = [Collections.Generic.HashSet[uint32]]::new()
# Victory Rush works like retail: no killing blow ("Victorious" caster aura state) or stance requirement,
# a 30 sec cooldown, and the module heals for 20% of maximum health on hit
$victoryRushId = 34428
$victoryRushDescription = Add-DbcString $strings 'Instantly attack the target causing ${$AP*$m1/100} damage and healing you for 20% of your maximum health.  Damage is based on your attack power.'
$foundVictoryRush = $false
$cloneRecords = @{}

for ($index = 0; $index -lt $recordCount; ++$index) {
    $offset = $index * $recordSize
    $spellId = [BitConverter]::ToUInt32($records, $offset)
    foreach ($localeGroup in @(136, 153, 170, 187)) {
        $englishOffset = [BitConverter]::ToUInt32($records, $offset + $localeGroup * 4)
        if ($englishOffset -ne 0) {
            for ($locale = 1; $locale -lt 16; ++$locale) {
                $localeOffset = $offset + ($localeGroup + $locale) * 4
                if ([BitConverter]::ToUInt32($records, $localeOffset) -eq 0) {
                    [BitConverter]::GetBytes($englishOffset).CopyTo($records, $localeOffset)
                }
            }
        }
    }
    if ($sinisterStrikeRanks -contains $spellId) {
        [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + 42 * 4)
        for ($locale = 0; $locale -lt 16; ++$locale) {
            [BitConverter]::GetBytes($sinisterDescription).CopyTo($records, $offset + (170 + $locale) * 4)
        }
        [void]$foundSinister.Add($spellId)
    }
    if ($spellId -eq $victoryRushId) {
        [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + 12 * 4)
        [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + 20 * 4)
        # 30 sec cooldown
        [BitConverter]::GetBytes([uint32]30000).CopyTo($records, $offset + 29 * 4)
        for ($locale = 0; $locale -lt 16; ++$locale) {
            [BitConverter]::GetBytes($victoryRushDescription).CopyTo($records, $offset + (170 + $locale) * 4)
        }
        $foundVictoryRush = $true
    }
    if ($talentReworks.ContainsKey([int]$spellId)) {
        $rework = $talentReworks[[int]$spellId]
        if ($rework.PassiveDummy) {
            $attributes = [BitConverter]::ToUInt32($records, $offset + 4 * 4)
            [BitConverter]::GetBytes([uint32]($attributes -bor 0x40)).CopyTo($records, $offset + 4 * 4)
            for ($field = 28; $field -le 30; ++$field) { [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + $field * 4) }
            for ($field = 40; $field -le 45; ++$field) { [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + $field * 4) }
            [BitConverter]::GetBytes([uint32]::MaxValue).CopyTo($records, $offset + 68 * 4)
            [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + 69 * 4)
            [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + 70 * 4)
            for ($field = 71; $field -le 130; ++$field) { [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + $field * 4) }
            [BitConverter]::GetBytes([uint32]6).CopyTo($records, $offset + 71 * 4)
            [BitConverter]::GetBytes([uint32]1).CopyTo($records, $offset + 86 * 4)
            [BitConverter]::GetBytes([uint32]4).CopyTo($records, $offset + 95 * 4)
        }
        $talentName = Add-DbcString $strings $rework.Name
        $talentDescription = Add-DbcString $strings $rework.Description
        for ($locale = 0; $locale -lt 16; ++$locale) {
            [BitConverter]::GetBytes($talentName).CopyTo($records, $offset + (136 + $locale) * 4)
            [BitConverter]::GetBytes($talentDescription).CopyTo($records, $offset + (170 + $locale) * 4)
            [BitConverter]::GetBytes([uint32]0).CopyTo($records, $offset + (187 + $locale) * 4)
        }
    }
    foreach ($custom in $customSpells) {
        if ($spellId -eq $custom.Clone) {
            $clone = [byte[]]::new($recordSize)
            [Array]::Copy($records, $offset, $clone, 0, $recordSize)
            $cloneRecords[[int]$custom.Id] = $clone
        }
    }
}

if ($foundSinister.Count -ne $sinisterStrikeRanks.Count) { throw 'Not all Sinister Strike ranks were found.' }
if (-not $foundVictoryRush) { throw 'Victory Rush was not found.' }
if ($cloneRecords.Count -ne $customSpells.Count) { throw 'Not all custom clone spells were found.' }

$customRecordBytes = [Collections.Generic.List[byte]]::new()
foreach ($custom in $customSpells) {
    $record = $cloneRecords[[int]$custom.Id]
    Set-Field $record 0 ([uint32]$custom.Id)
    Set-Field $record 29 ([uint32]$custom.Cooldown)
    Set-Field $record 30 0
    Set-Field $record 37 0
    Set-Field $record 38 ([uint32]$custom.Level)
    Set-Field $record 39 ([uint32]$custom.Level)
    Set-Field $record 42 ([uint32]$custom.Cost)
    Set-Field $record 43 0
    Set-Field $record 44 0
    Set-Field $record 45 0
    if ($custom.NoEquipment) {
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
    }
    if ($custom.DummyAura) {
        Set-Field $record 34 0
        Set-Field $record 35 0
        Set-Field $record 36 0
        Set-Field $record 41 3
        Set-Field $record 49 ([uint32]$(if ($custom.MaxStacks) { $custom.MaxStacks } else { 1 }))
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 71 6
        Set-Field $record 86 1
        Set-Field $record 95 4
        Set-Field $record 131 0
        Set-Field $record 132 0
        Set-Field $record 205 133
        Set-Field $record 206 1000
    }
    if ($custom.BleedAura) {
        Set-Field $record 40 32
        Set-Field $record 41 3
        Set-Field $record 49 1
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 71 6
        Set-Field $record 80 0
        Set-Field $record 86 6
        Set-Field $record 95 3
        Set-Field $record 98 2000
        Set-Field $record 208 0
        Set-Field $record 209 0
        Set-Field $record 210 0
        Set-Field $record 211 0
    }
    if ($custom.TravelCast) {
        Set-Field $record 28 14
        Set-Field $record 40 0
        Set-Field $record 41 0
        Set-Field $record 49 0
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 71 3
        Set-Field $record 86 1
    }
    if ($custom.ShadowLungeDamage) {
        Set-Field $record 73 31
        Set-Field $record 76 0
        Set-Field $record 79 0
        Set-Field $record 82 149
        Set-Field $record 85 0
        Set-Field $record 88 6
        Set-Field $record 91 0
        Set-Field $record 94 0
        Set-Field $record 97 0
        Set-Field $record 100 0
        Set-Field $record 103 0
        Set-Field $record 106 0
        Set-Field $record 109 0
        Set-Field $record 112 0
        Set-Field $record 115 0
        Set-Field $record 118 0
        Set-Field $record 121 0
        for ($field = 128; $field -le 130; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 213 2
        Set-Field $record 225 1
    }
    if ($custom.CrimsonSweep) {
        Set-Field $record 92 32
    }
    if ($custom.GladiatorStance) {
        # Permanent self aura usable only in Defensive Stance: +100% damage done and taken, all schools.
        # The shield requirement is enforced by the module script: an item requirement here would stop
        # the damage done bonus from applying to weapon attacks.
        Set-Field $record 1 0
        Set-Field $record 4 0x10
        for ($field = 5; $field -le 11; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 12 131072
        for ($field = 13; $field -le 27; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 28 1
        for ($field = 31; $field -le 36; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 40 21
        Set-Field $record 41 1
        Set-Field $record 46 1
        Set-Field $record 49 0
        for ($field = 50; $field -le 67; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 68 ([uint32]::MaxValue)
        Set-Field $record 69 0
        Set-Field $record 70 0
        for ($field = 71; $field -le 130; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 71 6
        Set-Field $record 72 6
        Set-Field $record 74 1
        Set-Field $record 75 1
        Set-Field $record 80 99
        Set-Field $record 81 99
        Set-Field $record 86 1
        Set-Field $record 87 1
        Set-Field $record 95 79
        Set-Field $record 96 87
        Set-Field $record 110 127
        Set-Field $record 111 127
        Set-Field $record 134 0
        Set-Field $record 204 0
        Set-Field $record 205 133
        Set-Field $record 206 1500
        Set-Field $record 208 4
        for ($field = 209; $field -le 214; ++$field) { Set-Field $record $field 0 }
        Set-Field $record 225 1
    }
    Set-Field $record 133 ([uint32]$customIconIds[[int]$custom.Id])
    $nameOffset = Add-DbcString $strings $custom.Name
    $descriptionOffset = Add-DbcString $strings $custom.Description
    $auraDescriptionOffset = if ($custom.AuraDescription) { Add-DbcString $strings $custom.AuraDescription } else { [uint32]0 }
    for ($locale = 0; $locale -lt 16; ++$locale) {
        Set-Field $record (136 + $locale) $nameOffset
        Set-Field $record (153 + $locale) 0
        Set-Field $record (170 + $locale) $descriptionOffset
        Set-Field $record (187 + $locale) $auraDescriptionOffset
    }
    $customRecordBytes.AddRange($record)
}

$newCount = $recordCount + $customSpells.Count
$spellOutput = [byte[]]::new(20 + ($newCount * $recordSize) + $strings.Count)
[Array]::Copy($source, 0, $spellOutput, 0, 20)
[BitConverter]::GetBytes([uint32]$newCount).CopyTo($spellOutput, 4)
[BitConverter]::GetBytes([uint32]$strings.Count).CopyTo($spellOutput, 16)
[Array]::Copy($records, 0, $spellOutput, 20, $records.Length)
$customRecordBytes.CopyTo($spellOutput, 20 + $records.Length)
$strings.CopyTo($spellOutput, 20 + ($newCount * $recordSize))

$skillSource = [IO.File]::ReadAllBytes($skillBackupPath)
Assert-Wdbc $skillSource 'SkillLineAbility.dbc'
$skillCount = [BitConverter]::ToInt32($skillSource, 4)
$skillFields = [BitConverter]::ToInt32($skillSource, 8)
$skillRecordSize = [BitConverter]::ToInt32($skillSource, 12)
$skillStringSize = [BitConverter]::ToInt32($skillSource, 16)
if ($skillFields -ne 14 -or $skillRecordSize -ne 56) { throw 'Unexpected SkillLineAbility.dbc layout.' }
$skillRecordsSize = $skillCount * $skillRecordSize
$skillStringsOffset = 20 + $skillRecordsSize
$sinisterSkillRecord = $null
$sinisterSkillIndex = -1
$maxSkillId = 0
for ($index = 0; $index -lt $skillCount; ++$index) {
    $offset = 20 + $index * $skillRecordSize
    $rowId = [BitConverter]::ToUInt32($skillSource, $offset)
    if ($rowId -gt $maxSkillId) { $maxSkillId = $rowId }
    if ([BitConverter]::ToUInt32($skillSource, $offset + 8) -eq 1752) {
        $sinisterSkillRecord = [byte[]]::new($skillRecordSize)
        [Array]::Copy($skillSource, $offset, $sinisterSkillRecord, 0, $skillRecordSize)
        $sinisterSkillIndex = $index
    }
}
if (-not $sinisterSkillRecord) { throw 'Sinister Strike SkillLineAbility row was not found.' }

$newSkillCount = $skillCount + $spellbookSpells.Count
$skillOutput = [byte[]]::new(20 + ($newSkillCount * $skillRecordSize) + $skillStringSize)
[Array]::Copy($skillSource, 0, $skillOutput, 0, 20)
[BitConverter]::GetBytes([uint32]$newSkillCount).CopyTo($skillOutput, 4)
$customSkillRecords = [Collections.Generic.List[byte]]::new()
for ($index = 0; $index -lt $spellbookSpells.Count; ++$index) {
    $record = [byte[]]::new($skillRecordSize)
    [Array]::Copy($sinisterSkillRecord, 0, $record, 0, $skillRecordSize)
    Set-Field $record 0 ([uint32]($maxSkillId + $index + 1))
    Set-Field $record 2 ([uint32]$spellbookSpells[$index].Id)
    Set-Field $record 8 0
    Set-Field $record 9 2
    if ($spellbookSpells[$index].SkillLine) {
        Set-Field $record 1 ([uint32]$spellbookSpells[$index].SkillLine)
        Set-Field $record 3 0
        Set-Field $record 4 ([uint32]$spellbookSpells[$index].ClassMask)
    }
    $customSkillRecords.AddRange($record)
}

$destinationOffset = 20
for ($index = 0; $index -lt $skillCount; ++$index) {
    [Array]::Copy($skillSource, 20 + $index * $skillRecordSize, $skillOutput, $destinationOffset, $skillRecordSize)
    $destinationOffset += $skillRecordSize
    if ($index -eq $sinisterSkillIndex) {
        $customSkillRecords.CopyTo($skillOutput, $destinationOffset)
        $destinationOffset += $customSkillRecords.Count
    }
}
[Array]::Copy($skillSource, $skillStringsOffset, $skillOutput, 20 + ($newSkillCount * $skillRecordSize), $skillStringSize)

New-Item -ItemType Directory -Path $clientDbcRoot -Force | Out-Null
[IO.File]::WriteAllBytes($serverSpellPath, $spellOutput)
[IO.File]::WriteAllBytes($clientSpellPath, $spellOutput)
[IO.File]::WriteAllBytes($serverSkillPath, $skillOutput)
[IO.File]::WriteAllBytes($clientSkillPath, $skillOutput)
[IO.File]::WriteAllBytes($serverIconPath, $iconOutput)
[IO.File]::WriteAllBytes($clientIconPath, $iconOutput)
Write-Host "Installed $($spellbookSpells.Count) custom abilities and $($customSpells.Count - $spellbookSpells.Count) native combat-state auras."
