# The rank spells of a class's retail-style talent trees, for patchSinisterStrike.ps1 (through the class's own
# Spells.ps1): one hidden passive per rank of a passive node and per option of a choice node. A node that teaches
# an ability (kind "active") points at a spell the class authors itself, so nothing is made for it here.
#
# A rank carries a real aura when its node declares one (valued per rank by `auraValues`, else `values`);
# otherwise it is a dummy the class's C++ reads with GetTalentValue. An aura with a `classMask` is a spell modifier
# (its misc the SPELLMOD op) reaching the class's spells that carry those family flags: one number for the first
# flag word, or [word 0, word 1, word 2]. `aura` may also be a list of up to three auras, one effect each, each
# valued by its own `values` when it has them. A node or option marked
# `existing` names rank spells the class authors itself (the Pestiféré's older talents), so nothing is made for
# it. Runs inside patchSinisterStrike.ps1's scope, which defines $A_Dummy.
param(
    [Parameter(Mandatory)][string]$TreePath,
    [Parameter(Mandatory)][int]$Family
)

$talentTree = Get-Content -LiteralPath $TreePath -Raw -Encoding UTF8 | ConvertFrom-Json

function New-TalentRank($id, $name, $icon, $description, $aura, $auraValue, $rank) {
    $fields = @{ 208 = $Family }
    $effects = @()
    if ($aura) {
        $auras = @($aura)
        for ($index = 0; $index -lt $auras.Count; ++$index) {
            $one = $auras[$index]
            $value = if ($one.values) { $one.values[$rank] } else { $auraValue }
            $auraEffect = @{ Index = $index; Effect = 6; Aura = [int]$one.type; TargetA = 1; Value = [int]$value }
            if ($null -ne $one.misc) { $auraEffect.Misc = [int]$one.misc }
            $effects += $auraEffect
            if ($null -ne $one.classMask) {
                # Effect n's class mask is fields 122 + 3n .. 124 + 3n
                $words = @($one.classMask)
                for ($word = 0; $word -lt $words.Count; ++$word) {
                    $fields[122 + 3 * $index + $word] = [int64]$words[$word]
                }
            }
        }
    }
    else { $effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 }) }
    return @{
        Id = [int]$id; Clone = 2983; Name = $name; IconPath = "Interface\Icons\$icon"
        Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
        Description = $description; Effects = $effects; Fields = $fields
    }
}

$ranks = @()
foreach ($tree in $talentTree.trees) {
    foreach ($node in $tree.nodes) {
        if ($node.kind -eq 'active' -or $node.existing) { continue }
        if ($node.kind -eq 'choice') {
            foreach ($option in $node.options) {
                if ($option.existing) { continue }
                $value = if ($option.aura) { @($option.aura)[0].value } else { 0 }
                $ranks += New-TalentRank $option.spell $option.name $option.icon $option.text $option.aura $value 0
            }
            continue
        }
        for ($rank = 0; $rank -lt $node.spells.Count; ++$rank) {
            $value = if ($node.values) { $node.values[$rank] } else { '' }
            $auraValue = if ($node.auraValues) { $node.auraValues[$rank] } else { $value }
            $ranks += New-TalentRank $node.spells[$rank] $node.name $node.icon `
                ([string]::Format($node.text, $value)) $node.aura $auraValue $rank
        }
    }
}
return $ranks
