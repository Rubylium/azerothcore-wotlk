# The rank spells of a class's retail-style talent trees, for patchSinisterStrike.ps1 (through the class's own
# Spells.ps1): one hidden passive per rank of a passive node and per option of a choice node. A node that teaches
# an ability (kind "active") points at a spell the class authors itself, so nothing is made for it here.
#
# A rank carries a real aura when its node declares one (valued per rank by `auraValues`, else `values`);
# otherwise it is a dummy the class's C++ reads with GetTalentValue. An aura with a `classMask` is a spell modifier
# (its misc the SPELLMOD op) reaching the class's spells that carry those family flags. A node or option marked
# `existing` names rank spells the class authors itself (the Pestiféré's older talents), so nothing is made for
# it. Runs inside patchSinisterStrike.ps1's scope, which defines $A_Dummy.
param(
    [Parameter(Mandatory)][string]$TreePath,
    [Parameter(Mandatory)][int]$Family
)

$talentTree = Get-Content -LiteralPath $TreePath -Raw -Encoding UTF8 | ConvertFrom-Json

function New-TalentRank($id, $name, $icon, $description, $aura, $auraValue) {
    $effect = if ($aura) {
        $auraEffect = @{ Index = 0; Effect = 6; Aura = [int]$aura.type; TargetA = 1; Value = [int]$auraValue }
        if ($null -ne $aura.misc) { $auraEffect.Misc = [int]$aura.misc }
        $auraEffect
    }
    else { @{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 } }
    $fields = @{ 208 = $Family }
    if ($aura -and $null -ne $aura.classMask) { $fields[122] = [int64]$aura.classMask }
    return @{
        Id = [int]$id; Clone = 2983; Name = $name; IconPath = "Interface\Icons\$icon"
        Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
        Description = $description; Effects = @($effect); Fields = $fields
    }
}

$ranks = @()
foreach ($tree in $talentTree.trees) {
    foreach ($node in $tree.nodes) {
        if ($node.kind -eq 'active' -or $node.existing) { continue }
        if ($node.kind -eq 'choice') {
            foreach ($option in $node.options) {
                if ($option.existing) { continue }
                $value = if ($option.aura) { $option.aura.value } else { 0 }
                $ranks += New-TalentRank $option.spell $option.name $option.icon $option.text $option.aura $value
            }
            continue
        }
        for ($rank = 0; $rank -lt $node.spells.Count; ++$rank) {
            $value = if ($node.values) { $node.values[$rank] } else { '' }
            $auraValue = if ($node.auraValues) { $node.auraValues[$rank] } else { $value }
            $ranks += New-TalentRank $node.spells[$rank] $node.name $node.icon `
                ([string]::Format($node.text, $value)) $node.aura $auraValue
        }
    }
}
return $ranks
