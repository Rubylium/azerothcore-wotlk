"""Generates the paragon board and writes it as the module's world SQL.

The board is data, not code: the server reads `paragon_node` and `paragon_node_link` and serves whatever is in
them, so reshaping the tree is a rerun of this script rather than a rebuild. Node positions are in canvas units
with the start node at the origin; the client scales them to the window.

Shape: a hub with six branches radiating out, one stat each. Minor nodes carry the branch out, notables sit at
the junctions, and keystones close each zone. Three zones, each further and stronger than the last:
- Éveil, rings 1-8: the first board. Adjacent branches are stitched together at rings two and three.
- Ascension, rings 9-16: the branch widens; bigger numbers, and the first effects that change a fight.
- Transcendance, rings 17-24: the far edge, where a character becomes something else. Its keystones are the
  Apothéoses.
Bridge nodes join neighbouring branches in the middle of the two outer zones, so a build can cross over there too.
The first zone's node ids never change: an allocation made on the first board stays valid.

Each branch is a side and each zone a tier: a side's Ascension opens once every node of its Éveil is taken, its
Transcendance once every node of its Ascension is (the server enforces it; the frame shows it). A node costs 1 to 5
points by how much it does (COST), and a branch's effects answer only to its own way of fighting (BRANCH_SCOPE): the
melee branches to weapon attacks, the caster branches to spells.

Every icon is checked against the client's SpellIcon.dbc before anything is written: a name that is not in the
client renders as a green question mark in game, which is hard to spot and easy to ship.

Usage: python localTools/paragon/buildParagonTree.py
"""
import math
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ICON_DBC = os.path.join(REPO, "server", "Data", "dbc", "SpellIcon.dbc")
OUTPUT = os.path.join(REPO, "modules", "mod-stat-growth", "data", "sql", "db-world", "base",
                      "stat_growth_paragon_board.sql")
# The client draws from its own copy rather than being sent 241 nodes over addon whispers, which cap at 255
# bytes each. Both come out of this script, so they cannot drift apart without the signature changing.
LUA_OUTPUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "FrameXML", "ParagonBoard.lua")

# PermanentStat (StatGrowthSystem.h)
STRENGTH, AGILITY, STAMINA, INTELLECT, SPIRIT, ATTACK_POWER, SPELL_POWER = range(7)

MINOR, NOTABLE, KEYSTONE = 0, 1, 2

# What one point buys, before the branch's own scale. A minor node is about four essences, a notable is about a
# Mythic+ run of essence farming, and a keystone is worth more than that: nothing on the board is filler.
# Effect ids, matching ParagonEffect in ParagonSystem.cpp. Change one and change the other.
E_STAT, E_ARMOR, E_ARMOR_PCT, E_GUARD, E_RETALIATE, E_LAST_STAND, E_FURY, E_SURGE = range(8)
# The outer zones' effects
E_DAMAGE, E_REDUCTION, E_LEECH, E_DOUBLE, E_EXECUTE, E_EXPLODE, E_UNDYING, E_HEALTH = range(8, 16)
E_KILL_STREAK, E_SPLASH, E_THREAT, E_GRUDGE = range(16, 20)
# The caster side's own procs: set off by casting and by spell damage only, never by a weapon
E_ECHO, E_ARC, E_QUICKEN, E_INSIGHT, E_WARD, E_MANA_SURGE = range(20, 26)

# What sets a node off (ParagonScope in ParagonSystem.cpp). A branch's nodes answer to its own way of fighting: the
# melee branches' procs and bonuses only to weapon attacks and abilities, the caster branches' only to spells, so
# neither side is worth raiding for the other. The armour branch, the hub and the bridges answer to anything.
SCOPE_ANY, SCOPE_WEAPON, SCOPE_SPELL = range(3)
BRANCH_SCOPE = {"Force": SCOPE_WEAPON, "Puissance": SCOPE_WEAPON, "Agilité": SCOPE_WEAPON,
                "Carapace": SCOPE_ANY, "Arcanes": SCOPE_SPELL, "Intellect": SCOPE_SPELL}
# No branch: the hub and the bridge nodes. They are never behind a tier gate.
NO_SIDE = 255

# What a node costs, by zone and kind. A plain minor node is a point; the nodes that change a fight cost more, up
# to 5 for an Apothéose. Zone 0 is Éveil, 1 Ascension, 2 Transcendance; "special" is a notable with an effect, "plain"
# a notable that is only a bigger stat.
COST = {
    0: {"minor": 1, "plain": 2, "special": 2, "keystone": 4, "bridge": 2},
    1: {"minor": 1, "plain": 2, "special": 3, "keystone": 5, "bridge": 2},
    2: {"minor": 1, "plain": 2, "special": 4, "keystone": 5, "bridge": 3},
}

# What a plain node is worth. The board is mostly these: a tree of nothing but procs would be noise, and the
# quiet nodes are what make the loud ones feel like arriving somewhere.
BASE_VALUE = {MINOR: 8, NOTABLE: 30, KEYSTONE: 0}     # keystones are never plain stats


def special(effect, name, icon, value=0, value2=0, chance=0.0, duration=0, cooldown=0):
    """A node with an effect. Its description is written once its branch, and so its scope, is known."""
    return {"effect": effect, "name": name, "icon": icon, "special": True,
            "value": value, "value2": value2, "chance": chance,
            "duration": duration, "cooldown": cooldown}


# Attack and spell power are worth less per point than a primary stat, so their branches carry bigger numbers
# for the same real gain.
#
# Each branch's `specials` are dealt out to its notable slots in order and then its keystone. A branch with
# fewer specials than notables fills the rest with plain stats, which is deliberate: the interesting nodes
# should be worth walking to, not the default.
BRANCHES = [
    (0, STRENGTH, 1.0, "Force",
     ("ParagonNode_Strength", "ParagonNode_StrengthMajor", "ParagonNode_StrengthMajor"),
     [
         special(E_FURY, "Ardeur", "ParagonNode_Ardour", value=8, chance=10.0, duration=10000),
     ],
     special(E_FURY, "Furie du parangon", "ParagonNode_Fury", value=20, chance=15.0, duration=12000)),

    (60, ATTACK_POWER, 2.0, "Puissance",
     ("ParagonNode_Power", "ParagonNode_PowerMajor", "ParagonNode_PowerMajor"),
     [
         special(E_SURGE, "Curée", "ParagonNode_Quarry", value=120, duration=15000),
     ],
     special(E_SURGE, "Élan du parangon", "ParagonNode_Momentum", value=350, duration=20000)),

    (120, AGILITY, 1.0, "Agilité",
     ("ParagonNode_Agility", "ParagonNode_AgilityMajor", "ParagonNode_AgilityMajor"),
     [
         special(E_RETALIATE, "Riposte", "ParagonNode_Riposte", value=25, chance=10.0),
     ],
     special(E_RETALIATE, "Représailles du parangon", "ParagonNode_Reprisal", value=60, chance=20.0)),

    # The armour branch. Minor nodes here are armour rather than stamina, so walking it actually makes you
    # harder to kill instead of just larger.
    (180, STAMINA, 1.2, "Carapace",
     ("ParagonNode_Armor", "ParagonNode_ArmorMajor", "ParagonNode_ArmorMajor"),
     [
         special(E_GUARD, "Écaille de pierre", "ParagonNode_Stonescale", value=10, chance=10.0, duration=8000),
         special(E_ARMOR_PCT, "Peau d'acier", "ParagonNode_Steelskin", value=5),
     ],
     special(E_LAST_STAND, "Rempart du parangon", "ParagonNode_Bulwark", value=40, value2=35, duration=10000,
             cooldown=60000)),

    # The caster branches carry their own procs, set off by spells alone: an echo of a spell's critical strike and a
    # quickening of the casts here, spell power, a ward and mana on the Intellect branch.
    (240, SPELL_POWER, 1.4, "Arcanes",
     ("ParagonNode_Arcane", "ParagonNode_ArcaneMajor", "ParagonNode_ArcaneMajor"),
     [
         special(E_ECHO, "Résonance", "ParagonNode_Resonance", value=50, chance=25.0),
     ],
     special(E_QUICKEN, "Célérité du parangon", "ParagonNode_Cataclysm", value=20, chance=15.0, duration=12000)),

    (300, INTELLECT, 1.0, "Intellect",
     ("ParagonNode_Intellect", "ParagonNode_IntellectMajor", "ParagonNode_IntellectMajor"),
     [
         special(E_INSIGHT, "Clairvoyance", "ParagonNode_Clairvoyance", value=120, chance=10.0, duration=10000),
     ],
     special(E_INSIGHT, "Omniscience du parangon", "ParagonNode_Omniscience", value=300, chance=15.0,
             duration=15000)),
]

# Armour a plain node on the Carapace branch is worth, in place of a stat.
ARMOR_MINOR, ARMOR_NOTABLE = 150, 600
ARMOR_BRANCH = "Carapace"

STAT_NAME = {
    STRENGTH: "Force", AGILITY: "Agilité", STAMINA: "Endurance", INTELLECT: "Intellect",
    SPIRIT: "Esprit", ATTACK_POWER: "Puissance d'attaque", SPELL_POWER: "Puissance des sorts",
}

# radius, node count, and which slots in the ring are notables (keystone is handled separately).
#
# Eight rings, not four. Depth is what makes a keystone a decision: at four rings the best node on the board
# was four points from the hub, so there was nothing to weigh up. At eight, a keystone costs eight of fifty
# and committing to two of them is most of a build.
# A branch leaves the hub as a single node and fans out from there. Three nodes abreast at a small radius
# cannot be done: the circle is not long enough to hold six branches of them and still keep the branches
# apart, and they collide with the neighbouring branch before they collide with each other.
RINGS = [
    (150, 1, ()),
    (240, 3, ()),
    (330, 5, (2,)),
    (420, 5, ()),
    (510, 7, (1, 5)),
    (600, 7, (3,)),
    (690, 5, (0, 4)),
    (780, 3, ()),
]
KEYSTONE_RING, KEYSTONE_SLOT = 7, 1

# The two outer zones, ring by ring outward from the first board's edge (radius 780): radius, nodes per branch,
# notable slots. Each zone ends in a keystone at `keystone` (ring index within the zone, slot).
OUTER_ZONES = [
    {
        "name": "Ascension",
        "rings": [(870, 7, ()), (960, 7, (1, 5)), (1050, 9, ()), (1140, 9, (4,)), (1230, 11, (2, 8)),
                  (1320, 11, ()), (1410, 9, (0, 8)), (1500, 7, ())],
        "keystone": (7, 3),
        "bridge_ring": 3,
        "minor": 20, "notable": 70, "armor": (150, 500),
    },
    {
        "name": "Transcendance",
        "rings": [(1590, 7, ()), (1680, 9, (2, 6)), (1770, 9, ()), (1860, 11, (1, 9)), (1950, 11, (5,)),
                  (2040, 9, (0, 8)), (2130, 7, (3,)), (2220, 3, ())],
        "keystone": (7, 1),
        "bridge_ring": 3,
        "minor": 45, "notable": 160, "armor": (300, 1100),
    },
]
OUTER_FIRST_ID = 2000
# Points that must already be spent on the board before a node can be taken, by zone and node type. Only the first
# zone's keystone still needs it: the outer zones sit behind their branch's tier gate instead (a side's Ascension
# opens once every node of its Éveil is taken, its Transcendance once every node of its Ascension is), which asks
# far more of a build than any spent total did. Zone 0 is Eveil, 1 Ascension, 2 Transcendance.
REQUIRED_SPENT = {
    0: {MINOR: 0, NOTABLE: 0, KEYSTONE: 20},
    1: {MINOR: 0, NOTABLE: 0, KEYSTONE: 0},
    2: {MINOR: 0, NOTABLE: 0, KEYSTONE: 0},
}

# A node's reach is drawn this far past the outermost ring, so the frame can pan to its edge
BOARD_MARGIN = 160

# How far a spell's arc jumps from its target (ParagonArcRange in ParagonSystem.cpp)
ARC_RANGE = 10


def seconds_text(ms):
    if ms % 60000 == 0 and ms >= 60000:
        return "%d min" % (ms // 60000)
    if ms % 1000:
        return ("%.1f s" % (ms / 1000.0)).replace(".", ",")
    return "%d s" % (ms // 1000)


def describe(effect, value=0, value2=0, chance=0.0, duration=0, cooldown=0, scope=SCOPE_ANY):
    """A node's text, in the words of what sets it off. The caps named here are ParagonSystem.cpp's."""
    seconds = duration // 1000
    weapon, spell = scope == SCOPE_WEAPON, scope == SCOPE_SPELL
    damage_of = "les dégâts de vos attaques d'arme" if weapon else ("les dégâts de vos sorts" if spell
                                                                    else "tous vos dégâts")
    hits = "vos attaques d'arme" if weapon else ("vos sorts" if spell else "vos coups")
    dealing = ("en frappant avec une arme" if weapon else
               ("en infligeant des dégâts avec un sort" if spell else "en infligeant des dégâts"))
    killing = ("Tuer un ennemi d'une attaque d'arme" if weapon else
               ("Tuer un ennemi avec un sort" if spell else "Tuer un ennemi"))
    recharge = (" %s de recharge." % seconds_text(cooldown)) if cooldown else ""

    if effect == E_DAMAGE:
        return "Augmente %s de %d%%." % (damage_of, value)
    if effect == E_REDUCTION:
        return "Réduit tous les dégâts subis de %d%% (réductions du parangon plafonnées à 25%%)." % value
    if effect == E_LEECH:
        return "Vous rend en points de vie %d%% des dégâts infligés par %s." % (value, hits)
    if effect == E_DOUBLE:
        return ("%d%% de chances que %s infligent %d%% de dégâts supplémentaires. Les chances de double "
                "frappe s'additionnent, jusqu'à 25%%." % (chance, hits, value))
    if effect == E_EXECUTE:
        return "%s sont augmentés de %d%% contre les cibles sous %d%% de vie." % (
            damage_of[0].upper() + damage_of[1:], value, value2)
    if effect == E_EXPLODE:
        return ("%s a %d%% de chances de le faire exploser : %d%% de ses points de vie maximum infligés aux "
                "ennemis à moins de %d mètres. Les explosions s'additionnent, jusqu'à 15%% des points de vie et "
                "40%% de chances." % (killing, chance, value, value2))
    if effect == E_KILL_STREAK:
        return ("%s a %d%% de chances d'augmenter %s de %d%% pendant %d s, cumulable %d fois. Chaque victime "
                "relance la durée." % (killing, chance, damage_of, value, seconds, value2))
    if effect == E_SPLASH:
        return ("%s ont %d%% de chances d'infliger aussi %d%% de leurs dégâts à 4 autres ennemis au plus, à moins "
                "de %d mètres de votre cible.%s" % (hits[0].upper() + hits[1:], chance, value, value2, recharge))
    if effect == E_THREAT:
        return "Augmente la menace que vous générez de %d%%." % value
    if effect == E_GRUDGE:
        return ("Les coups subis vous emplissent de rancune : vous gagnez en puissance d'attaque et des sorts %d%% des "
                "dégâts subis ces dernières secondes, jusqu'à %d%% de vos points de vie maximum." % (value, value2))
    if effect == E_UNDYING:
        return ("Un coup qui devrait vous tuer vous laisse à 1 point de vie, et vous êtes insensible aux dégâts "
                "pendant %d s. %d min de recharge." % (seconds, cooldown // 60000))
    if effect == E_HEALTH:
        return "Augmente vos points de vie maximum de %d%%." % value
    if effect == E_FURY:
        return ("%d%% de chances %s d'augmenter %s de %d%% pendant %d s."
                % (chance, dealing, damage_of, value, seconds))
    if effect == E_SURGE:
        power = ("puissance d'attaque" if weapon else ("puissance des sorts" if spell
                                                        else "puissance d'attaque et des sorts"))
        return "%s octroie %d en %s pendant %d s." % (killing, value, power, seconds)
    if effect == E_RETALIATE:
        return ("%d%% de chances quand vous êtes touché de renvoyer %d%% des dégâts à l'attaquant, au plus 4%% de "
                "vos points de vie maximum." % (chance, value))
    if effect == E_GUARD:
        return ("%d%% de chances quand vous êtes touché d'augmenter votre armure de %d%% pendant %d s."
                % (chance, value, seconds))
    if effect == E_LAST_STAND:
        return ("Sous %d%% de vie, vous subissez %d%% de dégâts en moins pendant %d s. %d min de recharge."
                % (value2, value, seconds, cooldown // 60000))
    if effect == E_ARMOR:
        return "+%d Armure" % value
    if effect == E_ARMOR_PCT:
        return "Augmente votre armure de %d%%." % value
    # The caster side: spells alone set these off
    if effect == E_ECHO:
        return ("Vos coups critiques de sort ont %d%% de chances de résonner : %d%% des dégâts du critique sont "
                "infligés une seconde fois. Les chances d'écho s'additionnent, jusqu'à 60%%." % (chance, value))
    if effect == E_ARC:
        return ("Vos sorts de dégâts ont %d%% de chances de propager %d%% de leurs dégâts à %d ennemis au plus, à "
                "moins de %d mètres de la cible.%s Les arcs s'additionnent, jusqu'à 35%% de chances."
                % (chance, value, value2, ARC_RANGE, recharge))
    if effect == E_QUICKEN:
        return ("Lancer un sort a %d%% de chances d'accélérer vos incantations de %d%% pendant %d s (30%% au plus)."
                % (chance, value, seconds))
    if effect == E_INSIGHT:
        return ("Lancer un sort a %d%% de chances de vous octroyer %d en puissance des sorts pendant %d s."
                % (chance, value, seconds))
    if effect == E_WARD:
        return ("Lancer un sort a %d%% de chances de vous entourer d'une égide qui absorbe des dégâts à hauteur de "
                "%d%% de votre puissance des sorts, pendant %d s.%s" % (chance, value, seconds, recharge))
    if effect == E_MANA_SURGE:
        return ("Lancer un sort a %d%% de chances de vous rendre %d%% de votre mana maximum.%s"
                % (chance, value, recharge))
    raise ValueError(effect)


# The outer zones' own icons (client-assets/source/icons, prompts in localTools/paragon/paragonIconPrompts.txt).
# Until a PNG exists the node falls back to the stock icon written next to it, so art never blocks the board.
CUSTOM_ICON = {
    "Force brute": "BruteForce", "Double frappe": "DoubleStrike", "Coup de grâce": "CoupDeGrace",
    "Colère contenue": "PentUpWrath", "Colosse": "Colossus",
    "Force titanesque": "TitanicStrength", "Frappes jumelles": "TwinStrikes", "Exécuteur": "Executioner",
    "Onde de choc": "Shockwave", "Apothéose : Titan": "ApotheosisTitan",
    "Curée sanglante": "BloodyQuarry", "Soif de sang": "Bloodthirst", "Carcasse explosive": "ExplodingCarcass",
    "Élan meurtrier": "MurderousDrive", "Carnage": "Carnage",
    "Frénésie du massacre": "SlaughterFrenzy", "Festin": "Feast", "Réaction en chaîne": "ChainReaction",
    "Instinct du prédateur": "PredatorInstinct", "Apothéose : Cataclysme": "ApotheosisCataclysm",
    "Contre-attaque": "Counterattack", "Lames agiles": "NimbleBlades", "Esquive parfaite": "PerfectDodge",
    "Précision mortelle": "DeadlyPrecision", "Épines": "Thorns",
    "Vengeance": "Vengeance", "Tourbillon de lames": "BladeWhirl", "Insaisissable": "Elusive",
    "Saignée": "Bloodletting", "Apothéose : Miroir": "ApotheosisMirror",
    "Peau de pierre": "Stoneskin", "Présence imposante": "Stoneskin", "Défi éternel": "AdamantiteSkin", "Vigueur": "Vigor", "Écailles de granit": "GraniteScales",
    "Robustesse": "Sturdiness", "Bastion": "Bastion",
    "Peau d'adamantite": "AdamantiteSkin", "Colossal": "Colossal", "Dernier rempart": "LastRampart",
    "Endurance infinie": "EndlessEndurance", "Apothéose : Immortel": "ApotheosisImmortal",
    "Puissance arcanique": "ArcanePotency", "Afflux": "Influx", "Désintégration": "Disintegration",
    "Écho": "Echo", "Surcharge": "Overload",
    "Maîtrise absolue": "AbsoluteMastery", "Tempête arcanique": "ArcaneStorm", "Double incantation": "Doublecast",
    "Siphon": "Siphon", "Apothéose : Singularité": "ApotheosisSingularity",
    "Rémanence": "Remanence", "Esprit fortifié": "FortifiedMind", "Illumination": "Illumination",
    "Aura protectrice": "ProtectiveAura", "Clarté": "Clarity",
    "Communion": "Communion", "Transcendance de l'âme": "SoulTranscendence", "Révélation": "Revelation",
    "Savoir interdit": "ForbiddenKnowledge", "Apothéose : Éternité": "ApotheosisEternity",
    "Carrefour": "Crossroads", "Confluence": "Confluence",
}
# The plain nodes of each outer zone wear their branch's icon with the zone's name after it
ZONE_ICON_SUFFIX = ["Ascension", "Transcendence"]


def outer(effect, name, icon, value=0, value2=0, chance=0.0, duration=0, cooldown=0):
    icon = "ParagonNode_%s|%s" % (CUSTOM_ICON[name], icon)
    return special(effect, name, icon, value=value, value2=value2, chance=chance, duration=duration,
                   cooldown=cooldown)


# Each branch's notables and keystone in the two outer zones, dealt out in order like the first zone's. Ascension is
# where a build starts to change how a fight goes; Transcendance is where it stops being fair.
OUTER_SPECIALS = {
    "Force": [
        ([outer(E_DAMAGE, "Force brute", "Ability_Warrior_InnerRage", value=4),
          outer(E_DOUBLE, "Double frappe", "Ability_DualWield", value=100, chance=5.0),
          outer(E_EXECUTE, "Coup de grâce", "Ability_Warrior_DecisiveStrike", value=15, value2=30),
          outer(E_DAMAGE, "Colère contenue", "Ability_Warrior_BloodFrenzy", value=5)],
         outer(E_DOUBLE, "Colosse", "Ability_Warrior_Rampage", value=100, chance=8.0)),
        ([outer(E_DAMAGE, "Force titanesque", "Spell_Shadow_UnholyStrength", value=8),
          outer(E_DOUBLE, "Frappes jumelles", "Ability_Warrior_PunishingBlow", value=100, chance=6.0),
          outer(E_EXECUTE, "Exécuteur", "Ability_Rogue_Eviscerate", value=30, value2=35),
          outer(E_EXPLODE, "Onde de choc", "Ability_Warrior_Cleave", value=10, value2=8, chance=25.0)],
         outer(E_DOUBLE, "Apothéose : Titan", "INV_Sword_48", value=100, chance=12.0)),
    ],
    "Puissance": [
        ([outer(E_SURGE, "Curée sanglante", "Ability_Rogue_MurderSpree", value=600, duration=20000),
          outer(E_LEECH, "Soif de sang", "Spell_Shadow_LifeDrain02", value=3),
          outer(E_EXPLODE, "Carcasse explosive", "Spell_Fire_SelfDestruct", value=5, value2=6, chance=25.0),
          outer(E_DAMAGE, "Élan meurtrier", "Ability_Warrior_Warcry", value=4)],
         outer(E_KILL_STREAK, "Carnage", "Spell_Deathknight_BloodBoil", value=2, value2=8, chance=50.0,
               duration=15000)),
        ([outer(E_SURGE, "Frénésie du massacre", "Spell_Shadow_UnholyFrenzy", value=1500, duration=20000),
          outer(E_LEECH, "Festin", "Spell_Shadow_SoulLeech_3", value=6),
          outer(E_KILL_STREAK, "Réaction en chaîne", "Spell_Fire_Incinerate", value=1, value2=10, chance=50.0,
                duration=15000),
          outer(E_DAMAGE, "Instinct du prédateur", "Ability_Hunter_Pet_Devilsaur", value=8)],
         outer(E_SPLASH, "Apothéose : Cataclysme", "Spell_Fire_MeteorStorm", value=50, value2=10, chance=20.0,
               cooldown=1000)),
    ],
    "Agilité": [
        ([outer(E_RETALIATE, "Contre-attaque", "Ability_Warrior_Revenge", value=50, chance=20.0),
          outer(E_DOUBLE, "Lames agiles", "Ability_Rogue_SliceDice", value=100, chance=4.0),
          outer(E_REDUCTION, "Esquive parfaite", "Ability_Rogue_Feint", value=4),
          outer(E_DAMAGE, "Précision mortelle", "Ability_Rogue_Feint", value=4)],
         outer(E_RETALIATE, "Épines", "Spell_Nature_Thorns", value=100, chance=35.0)),
        ([outer(E_RETALIATE, "Vengeance", "Ability_Warrior_Revenge", value=100, chance=30.0),
          outer(E_DOUBLE, "Tourbillon de lames", "Ability_Rogue_MurderSpree", value=100, chance=5.0),
          outer(E_REDUCTION, "Insaisissable", "Spell_Arcane_PrismaticCloak", value=6),
          outer(E_LEECH, "Saignée", "Spell_Shadow_LifeDrain02", value=4)],
         outer(E_RETALIATE, "Apothéose : Miroir", "Spell_Holy_AshesToAshes", value=200, chance=50.0)),
    ],
    "Carapace": [
        ([outer(E_THREAT, "Présence imposante", "Ability_Warrior_DefensiveStance", value=25),
          outer(E_HEALTH, "Vigueur", "Spell_Holy_BlessingOfStamina", value=5),
          outer(E_GUARD, "Écailles de granit", "Spell_Holy_PowerWordShield", value=25, chance=20.0, duration=8000),
          outer(E_HEALTH, "Robustesse", "Spell_Nature_Reincarnation", value=4)],
         outer(E_GRUDGE, "Bastion", "Ability_Warrior_ShieldWall", value=15, value2=8)),
        ([outer(E_THREAT, "Défi éternel", "Spell_Holy_DivineIntervention", value=40),
          outer(E_HEALTH, "Colossal", "Spell_Holy_Heroism", value=10),
          outer(E_LAST_STAND, "Dernier rempart", "Spell_Holy_LayOnHands", value=60, value2=40, duration=10000,
                cooldown=60000),
          outer(E_HEALTH, "Endurance infinie", "Spell_Nature_Reincarnation", value=8)],
         outer(E_UNDYING, "Apothéose : Immortel", "Spell_Holy_GuardianSpirit", duration=4000, cooldown=120000)),
    ],
    # The caster side: echoes of a spell's critical strikes and arcs of its damage to the pack on Arcanes; spell
    # power, a ward and mana from casting on Intellect. Nothing here answers to a weapon.
    "Arcanes": [
        ([outer(E_DAMAGE, "Puissance arcanique", "Spell_Arcane_ArcanePotency", value=4),
          outer(E_QUICKEN, "Afflux", "Spell_Arcane_ArcaneTorrent", value=15, chance=15.0, duration=10000),
          outer(E_EXECUTE, "Désintégration", "Spell_Arcane_Blast", value=15, value2=30),
          outer(E_ECHO, "Écho", "Spell_Nature_LightningOverload", value=60, chance=30.0)],
         outer(E_ARC, "Surcharge", "Spell_Fire_Fireball02", value=50, value2=3, chance=20.0, cooldown=1000)),
        ([outer(E_DAMAGE, "Maîtrise absolue", "Spell_Arcane_MindMastery", value=8),
          outer(E_ARC, "Tempête arcanique", "Spell_Nature_Bloodlust", value=40, value2=4, chance=15.0,
                cooldown=1000),
          outer(E_ECHO, "Double incantation", "Spell_Nature_LightningOverload", value=75, chance=35.0),
          outer(E_LEECH, "Siphon", "Spell_Shadow_SiphonMana", value=4)],
         outer(E_ECHO, "Apothéose : Singularité", "Spell_Shadow_Twilight", value=100, chance=40.0)),
    ],
    "Intellect": [
        ([outer(E_LEECH, "Rémanence", "Spell_Shadow_LifeDrain02", value=3),
          outer(E_WARD, "Esprit fortifié", "Spell_Holy_MindVision", value=150, chance=20.0, duration=10000,
                cooldown=20000),
          outer(E_MANA_SURGE, "Illumination", "Spell_Holy_SurgeOfLight", value=3, chance=15.0, cooldown=5000),
          outer(E_REDUCTION, "Aura protectrice", "Spell_Holy_PowerWordShield", value=3)],
         outer(E_LEECH, "Clarté", "Spell_Holy_BorrowedTime", value=8)),
        ([outer(E_LEECH, "Communion", "Spell_Shadow_SoulLeech_3", value=6),
          outer(E_HEALTH, "Transcendance de l'âme", "Spell_Holy_SealOfMight", value=8),
          outer(E_INSIGHT, "Révélation", "Spell_Holy_Crusade", value=600, chance=20.0, duration=15000),
          outer(E_DAMAGE, "Savoir interdit", "Spell_Arcane_MindMastery", value=6)],
         outer(E_LEECH, "Apothéose : Éternité", "Achievement_Boss_Algalon_01", value=20)),
    ],
}

# The nodes between two branches, in the middle of each outer zone
BRIDGES = [
    outer(E_HEALTH, "Carrefour", "Spell_Holy_BlessingOfStamina", value=3),
    outer(E_DAMAGE, "Confluence", "Spell_Arcane_PrismaticCloak", value=5),
]
BRANCH_SPREAD = 26.0          # hard cap on the half-angle a branch may occupy
NODE_SPACING = 95.0           # canvas units between neighbours in a ring, before the cap
BRANCH_GAP = 70.0             # canvas units of clear space between one branch and the next
START_NODE = 1


def load_icons():
    separator = chr(92)
    data = open(ICON_DBC, "rb").read()
    magic, record_count, _, record_size, string_size = struct.unpack_from("<4sIIII", data, 0)
    if magic != b"WDBC":
        raise SystemExit("%s is not a DBC" % ICON_DBC)

    header = 20
    strings = data[header + record_count * record_size:][:string_size]
    names = set()
    for index in range(record_count):
        _, offset = struct.unpack_from("<II", data, header + index * record_size)
        if 0 < offset < len(strings):
            path = strings[offset:strings.find(b"\0", offset)].decode("latin1")
            if path:
                names.add(path.split(separator)[-1].lower())
    return names


def escape(text):
    return text.replace(chr(92), chr(92) * 2).replace("'", "''")


ICON_PREFIX = "ParagonNode_"
ICON_SOURCE = os.path.join(REPO, "modules", "mod-stat-growth", "client-assets", "source", "icons")


def custom_icons():
    """The node icons this module ships, by name."""
    if not os.path.isdir(ICON_SOURCE):
        return set()
    return {os.path.splitext(name)[0].lower() for name in os.listdir(ICON_SOURCE)
            if name.lower().endswith(".png")}


def build():
    available = load_icons()
    ours = custom_icons()
    wanted = {icon for branch in BRANCHES for icon in branch[4]}
    wanted |= {s["icon"] for branch in BRANCHES for s in branch[5]}
    wanted |= {branch[6]["icon"] for branch in BRANCHES}
    for zones in OUTER_SPECIALS.values():
        for notables, keystone in zones:
            wanted |= {s["icon"].split("|")[-1] for s in notables}
            wanted.add(keystone["icon"].split("|")[-1])
    wanted |= {b["icon"].split("|")[-1] for b in BRIDGES}
    missing_custom = sorted({i for i in wanted
                             if i.startswith(ICON_PREFIX) and i.lower() not in ours})
    if missing_custom:
        raise SystemExit("No source PNG in %s for: %s" % (ICON_SOURCE, ", ".join(missing_custom)))

    missing_stock = sorted({i for i in wanted
                            if not i.startswith(ICON_PREFIX) and i.lower() not in available})
    if missing_stock:
        raise SystemExit("Not in SpellIcon.dbc, would show as a green question mark: %s"
                         % ", ".join(missing_stock))

    nodes = []
    links = set()
    branches = []                                        # ring_slots per branch, for the cross-links below

    nodes.append({
        "id": START_NODE, "type": MINOR, "x": 0, "y": 0, "effect": E_STAT, "stat": STAMINA, "value": 0,
        "value2": 0, "chance": 0.0, "duration": 0, "cooldown": 0, "free": 1,
        "icon": "ParagonNode_Awakening", "name": "Éveil", "branch": "",
        "description": "Le point de départ du tableau. Aucun point requis.",
    })
    if "paragonnode_awakening" not in ours:
        raise SystemExit("start node icon missing from %s" % ICON_SOURCE)

    node_id = 100
    for angle, stat, scale, branch_name, icons, specials, keystone in BRANCHES:
        remaining = list(specials)
        ring_slots = []                                  # node ids per ring, in ring order
        for ring_index, (radius, count, notable_slots) in enumerate(RINGS):
            slots = []
            for slot in range(count):
                # Spread the ring's nodes to a constant distance apart rather than a constant angle:
                # a fixed half-angle crams the inner rings and strands the outer ones, because the arc a
                # given angle covers grows with the radius. Capped so a branch never bleeds into its
                # neighbour's wedge.
                # Three caps, tightest wins: the branch's own limit, the spacing its nodes need, and the
                # clearance to the next branch. That last one binds hard near the hub, where a wide angle is
                # only a few units of arc and the arms run into each other.
                clearance = (360.0 / len(BRANCHES) - math.degrees(BRANCH_GAP / radius)) / 2.0
                spread = min(BRANCH_SPREAD,
                             math.degrees((count - 1) * NODE_SPACING / (2.0 * radius)),
                             max(0.0, clearance))
                offset = 0.0 if count == 1 else (slot / (count - 1.0) - 0.5) * 2.0 * spread
                theta = math.radians(angle + offset)

                if ring_index == KEYSTONE_RING and slot == KEYSTONE_SLOT:
                    node_type = KEYSTONE
                elif slot in notable_slots:
                    node_type = NOTABLE
                else:
                    node_type = MINOR

                # Keystones are always a special, notables take one while any are left, and everything
                # else is the branch's plain stat - or its armour, on the branch that is about armour.
                chosen = None
                if node_type == KEYSTONE:
                    chosen = keystone
                elif node_type == NOTABLE and remaining:
                    chosen = remaining.pop(0)

                if chosen:
                    node = {
                        "id": node_id, "type": node_type,
                        "x": int(round(radius * math.cos(theta))),
                        "y": int(round(radius * math.sin(theta))),
                        "effect": chosen["effect"], "stat": stat,
                        "value": chosen["value"], "value2": chosen["value2"],
                        "chance": chosen["chance"], "duration": chosen["duration"],
                        "cooldown": chosen["cooldown"], "free": 0, "icon": chosen["icon"],
                        "name": chosen["name"], "branch": branch_name, "special": True,
                    }
                elif branch_name == ARMOR_BRANCH:
                    amount = ARMOR_MINOR if node_type == MINOR else ARMOR_NOTABLE
                    node = {
                        "id": node_id, "type": node_type,
                        "x": int(round(radius * math.cos(theta))),
                        "y": int(round(radius * math.sin(theta))),
                        "effect": E_ARMOR, "stat": stat, "value": amount, "value2": 0, "chance": 0.0,
                        "duration": 0, "cooldown": 0, "free": 0, "icon": icons[node_type],
                        "name": "Carapace" if node_type == MINOR else "Carapace majeure",
                        "branch": branch_name, "description": "+%d Armure" % amount,
                    }
                else:
                    value = int(round(BASE_VALUE[node_type] * scale))
                    node = {
                        "id": node_id, "type": node_type,
                        "x": int(round(radius * math.cos(theta))),
                        "y": int(round(radius * math.sin(theta))),
                        "effect": E_STAT, "stat": stat, "value": value, "value2": 0, "chance": 0.0,
                        "duration": 0, "cooldown": 0, "free": 0, "icon": icons[node_type],
                        "name": branch_name if node_type == MINOR else "%s majeur" % branch_name,
                        "branch": branch_name, "description": "+%d %s" % (value, STAT_NAME[stat]),
                    }

                nodes.append(node)
                slots.append(node_id)
                node_id += 1

            # Along the ring, so a build can spread sideways instead of only outward
            for left, right in zip(slots, slots[1:]):
                links.add((left, right))
            ring_slots.append(slots)

        # The hub feeds the middle of the first ring
        links.add((START_NODE, ring_slots[0][len(ring_slots[0]) // 2]))

        # Each node reaches the nearest node of the next ring out, by angle
        for inner, outer in zip(ring_slots, ring_slots[1:]):
            for index, node in enumerate(inner):
                position = index / max(len(inner) - 1.0, 1.0)
                target = outer[int(round(position * (len(outer) - 1)))]
                links.add((min(node, target), max(node, target)))
        branches.append(ring_slots)

    # Stitch neighbouring branches together at rings 2 and 3, so the board is a web rather than six spokes
    for index, rings in enumerate(branches):
        neighbour = branches[(index + 1) % len(branches)]
        for ring in (1, 2):
            a, b = rings[ring][-1], neighbour[ring][0]
            links.add((min(a, b), max(a, b)))

    build_outer(nodes, links, branches)
    sides = {branch[3]: index for index, branch in enumerate(BRANCHES)}
    for node in nodes:
        zone = node.get("zone", 0)
        node["required"] = 0 if node["free"] else REQUIRED_SPENT[zone][node["type"]]
        # The side and tier the gate reads: a side's tier opens once every node of the tier before it on that side
        # is taken. The hub and the bridges belong to no side and are never gated.
        node["tier"] = zone
        node["side"] = sides.get(node["branch"], NO_SIDE)
        node["scope"] = BRANCH_SCOPE.get(node["branch"], SCOPE_ANY)
        if node["free"]:
            node["cost"] = 0
        elif node.get("bridge"):
            node["cost"] = COST[zone]["bridge"]
        elif node["type"] == KEYSTONE:
            node["cost"] = COST[zone]["keystone"]
        elif node["type"] == NOTABLE:
            node["cost"] = COST[zone]["special" if node.get("special") else "plain"]
        else:
            node["cost"] = COST[zone]["minor"]
        if node.get("special"):
            node["description"] = describe(node["effect"], node["value"], node["value2"], node["chance"],
                                           node["duration"], node["cooldown"], node["scope"])

    # An outer icon is "custom|stock": the custom one once its PNG is in, the stock one until then
    waiting = set()
    for node in nodes:
        if "|" in node["icon"]:
            custom, stock = node["icon"].split("|")
            if custom.lower() in ours:
                node["icon"] = custom
            else:
                node["icon"] = stock
                waiting.add(custom)
    if waiting:
        print("  %d custom icons not drawn yet, standing in with stock ones (prompts: paragonIconPrompts.txt)"
              % len(waiting))
    return nodes, sorted(links)


def place(radius, angle, count, slot):
    """A ring slot's position, spread the same way as the first zone's"""
    clearance = (360.0 / len(BRANCHES) - math.degrees(BRANCH_GAP / radius)) / 2.0
    spread = min(BRANCH_SPREAD, math.degrees((count - 1) * NODE_SPACING / (2.0 * radius)), max(0.0, clearance))
    offset = 0.0 if count == 1 else (slot / (count - 1.0) - 0.5) * 2.0 * spread
    theta = math.radians(angle + offset)
    return int(round(radius * math.cos(theta))), int(round(radius * math.sin(theta)))


def make_node(node_id, node_type, x, y, stat, chosen=None, plain=None):
    node = {"id": node_id, "type": node_type, "x": x, "y": y, "stat": stat, "free": 0}
    if chosen:
        node.update({"effect": chosen["effect"], "value": chosen["value"], "value2": chosen["value2"],
                     "chance": chosen["chance"], "duration": chosen["duration"], "cooldown": chosen["cooldown"],
                     "icon": chosen["icon"], "name": chosen["name"], "special": True})
    else:
        node.update(plain)
        node.update({"value2": 0, "chance": 0.0, "duration": 0, "cooldown": 0})
    return node


def build_outer(nodes, links, branches):
    """Ascension and Transcendance: each branch carries on outward from the first board's edge"""
    node_id = OUTER_FIRST_ID
    ends = [rings[-1] for rings in branches]             # each branch's last ring so far
    for zone_index, zone in enumerate(OUTER_ZONES):
        bridge_rows = []
        for branch_index, (angle, stat, scale, branch_name, icons, _, _) in enumerate(BRANCHES):
            notables, keystone = OUTER_SPECIALS[branch_name][zone_index]
            remaining = list(notables)
            previous = ends[branch_index]
            zone_rings = []
            for ring_index, (radius, count, notable_slots) in enumerate(zone["rings"]):
                slots = []
                for slot in range(count):
                    x, y = place(radius, angle, count, slot)
                    if (ring_index, slot) == zone["keystone"]:
                        node = make_node(node_id, KEYSTONE, x, y, stat, chosen=keystone)
                    elif slot in notable_slots and remaining:
                        node = make_node(node_id, NOTABLE, x, y, stat, chosen=remaining.pop(0))
                    else:
                        node_type = NOTABLE if slot in notable_slots else MINOR
                        zone_icon = "%s%s%s|%s" % (icons[0], ZONE_ICON_SUFFIX[zone_index],
                                                   "Major" if node_type == NOTABLE else "", icons[node_type])
                        if branch_name == ARMOR_BRANCH:
                            amount = zone["armor"][0 if node_type == MINOR else 1]
                            plain = {"effect": E_ARMOR, "value": amount, "icon": zone_icon,
                                     "name": "Carapace" if node_type == MINOR else "Carapace majeure",
                                     "branch": branch_name, "description": "+%d Armure" % amount}
                        else:
                            base = zone["minor"] if node_type == MINOR else zone["notable"]
                            value = int(round(base * scale))
                            plain = {"effect": E_STAT, "value": value, "icon": zone_icon,
                                     "name": branch_name if node_type == MINOR else "%s majeur" % branch_name,
                                     "branch": branch_name,
                                     "description": "+%d %s" % (value, STAT_NAME[stat])}
                        node = make_node(node_id, node_type, x, y, stat, plain=plain)
                    node["branch"] = branch_name
                    node["zone"] = zone_index + 1
                    nodes.append(node)
                    slots.append(node_id)
                    node_id += 1

                for left, right in zip(slots, slots[1:]):
                    links.add((left, right))
                # Each node of the ring before reaches the nearest of this one, by angle
                for index, inner in enumerate(previous):
                    position = index / max(len(previous) - 1.0, 1.0)
                    target = slots[int(round(position * (len(slots) - 1)))]
                    links.add((min(inner, target), max(inner, target)))
                previous = slots
                zone_rings.append(slots)
            ends[branch_index] = previous
            bridge_rows.append(zone_rings[zone["bridge_ring"]])

        # A bridge node halfway between each branch and the next, in the middle of the zone
        bridge = BRIDGES[zone_index]
        radius = zone["rings"][zone["bridge_ring"]][0]
        for branch_index, (angle, stat, *_rest) in enumerate(BRANCHES):
            x = int(round(radius * math.cos(math.radians(angle + 30))))
            y = int(round(radius * math.sin(math.radians(angle + 30))))
            node = make_node(node_id, NOTABLE, x, y, stat, chosen=bridge)
            node["branch"] = ""
            node["bridge"] = True
            node["zone"] = zone_index + 1
            nodes.append(node)
            left = bridge_rows[branch_index][-1]
            right = bridge_rows[(branch_index + 1) % len(BRANCHES)][0]
            links.add((min(left, node_id), max(left, node_id)))
            links.add((min(right, node_id), max(right, node_id)))
            node_id += 1


def signature(nodes, links):
    """Must match LoadParagonBoard in ParagonSystem.cpp, wraparound included."""
    total = 0
    for node in nodes:
        total += node["id"] * 31 + node["value"] * 7 + node["stat"] + node["effect"] * 3 + node["required"] * 5
        total += node["cost"] * 11 + node["tier"] * 19 + node["side"] * 23 + node["scope"] * 29
    for a, b in links:
        total += a * 13 + b * 17
    return total % (2 ** 32)


def write_lua(nodes, links, stamp):
    extent = 2 * (max(max(abs(n["x"]), abs(n["y"])) for n in nodes) + BOARD_MARGIN)
    zones = [("Éveil", 0)] + [(zone["name"], zone["rings"][0][0] - 45) for zone in OUTER_ZONES]
    lines = [
        "-- Generated by localTools/paragon/buildParagonTree.py -- do not edit by hand.",
        "--",
        "-- The board the frame draws. The server holds the same one in paragon_node and decides what may be",
        "-- allocated; this copy exists so opening the frame does not mean sending 241 nodes through addon",
        "-- whispers that cap at 255 bytes each. The signature is checked against the server's on open, so a",
        "-- client patched out of step says so instead of quietly drawing the wrong tree.",
        "",
        "ParagonBoard = {",
        "    signature = %d," % stamp,
        "    extent = %d," % extent,
        "    zones = { %s }," % ", ".join("{ name = %s, radius = %d }" % (lua_string(name), radius)
                                          for name, radius in zones),
        "    sides = { %s }," % ", ".join("[%d] = %s" % (index, lua_string(branch[3]))
                                          for index, branch in enumerate(BRANCHES)),
        "    nodes = {",
    ]
    for node in nodes:
        lines.append(
            "        [%d] = { type = %d, x = %d, y = %d, effect = %d, stat = %d, value = %d, required = %d,"
            " cost = %d, side = %d, tier = %d, scope = %d,"
            " free = %s, icon = %s, name = %s, description = %s }," % (
                node["id"], node["type"], node["x"], node["y"], node["effect"], node["stat"],
                node["value"], node["required"], node["cost"], node["side"], node["tier"], node["scope"],
                "true" if node["free"] else "false",
                lua_string("Interface" + chr(92) + "Icons" + chr(92) + node["icon"]),
                lua_string(node["name"]), lua_string(node["description"])))
    lines += ["    },", "    links = {"]
    for a, b in links:
        lines.append("        { %d, %d }," % (a, b))
    lines += ["    },", "}", ""]

    with open(LUA_OUTPUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))


def lua_string(text):
    return '"' + text.replace(chr(92), chr(92) * 2).replace('"', chr(92) + '"') + '"'


def main():
    nodes, links = build()

    counts = {MINOR: 0, NOTABLE: 0, KEYSTONE: 0}
    for node in nodes:
        counts[node["type"]] += 1

    lines = [
        "-- Generated by localTools/paragon/buildParagonTree.py -- do not edit by hand.",
        "--",
        "-- The paragon board: a hub and six stat branches through three zones (Éveil, Ascension,",
        "-- Transcendance), %d rings deep. %d nodes (%d minor, %d notable, %d keystone); the far edge sits"
        % (len(RINGS) + sum(len(z["rings"]) for z in OUTER_ZONES), len(nodes), counts[MINOR],
           counts[NOTABLE], counts[KEYSTONE]),
        "-- %d points from the hub." % (len(RINGS) + sum(len(z["rings"]) for z in OUTER_ZONES)),
        "",
        "-- Schema lives here too, so this file never depends on another one having run first. The board is generated",
        "-- data with nothing of the players in it, so it is dropped and rebuilt whole: a new column needs no migration.",
        "DROP TABLE IF EXISTS `paragon_node`;",
        "CREATE TABLE `paragon_node` (",
        "    `id` INT UNSIGNED NOT NULL,",
        "    `type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 minor, 1 notable, 2 keystone',",
        "    `x` SMALLINT NOT NULL DEFAULT 0,",
        "    `y` SMALLINT NOT NULL DEFAULT 0,",
        "    `effect` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ParagonEffect',",
        "    `stat` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'PermanentStat',",
        "    `value` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `value2` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `chance` FLOAT NOT NULL DEFAULT 0 COMMENT 'percent, for the procs',",
        "    `duration` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'milliseconds',",
        "    `cooldown` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'milliseconds',",
        "    `free` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'costs no point, always allocated',",
        "    `required` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'points already spent before it can be taken',",
        "    `cost` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'points it takes, 1 to 5',",
        "    `side` TINYINT UNSIGNED NOT NULL DEFAULT 255 COMMENT 'branch index, 255 for the hub and the bridges',",
        "    `tier` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 Eveil, 1 Ascension, 2 Transcendance',",
        "    `scope` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'what sets it off: 0 anything, 1 weapons, 2 spells',",
        "    `icon` VARCHAR(128) NOT NULL DEFAULT '',",
        "    `name` VARCHAR(64) NOT NULL DEFAULT '',",
        "    `description` VARCHAR(255) NOT NULL DEFAULT '',",
        "    PRIMARY KEY (`id`)",
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
        "",
        "CREATE TABLE IF NOT EXISTS `paragon_node_link` (",
        "    `node_a` INT UNSIGNED NOT NULL,",
        "    `node_b` INT UNSIGNED NOT NULL,",
        "    PRIMARY KEY (`node_a`, `node_b`)",
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
        "",
        "DELETE FROM `paragon_node_link`;",
        "DELETE FROM `paragon_node`;",
        "",
        "INSERT INTO `paragon_node` (`id`, `type`, `x`, `y`, `effect`, `stat`, `value`, `value2`,"
        " `chance`, `duration`, `cooldown`, `free`, `required`, `cost`, `side`, `tier`, `scope`, `icon`, `name`,"
        " `description`) VALUES",
    ]

    rows = ["    (%d, %d, %d, %d, %d, %d, %d, %d, %g, %d, %d, %d, %d, %d, %d, %d, %d, '%s', '%s', '%s')" % (
        n["id"], n["type"], n["x"], n["y"], n["effect"], n["stat"], n["value"], n["value2"],
        n["chance"], n["duration"], n["cooldown"], n["free"], n["required"], n["cost"], n["side"], n["tier"],
        n["scope"],
        escape("Interface" + chr(92) + "Icons" + chr(92) + n["icon"]),
        escape(n["name"]), escape(n["description"])) for n in nodes]
    lines.append(",\n".join(rows) + ";")
    lines.append("")
    lines.append("INSERT INTO `paragon_node_link` (`node_a`, `node_b`) VALUES")
    lines.append(",\n".join("    (%d, %d)" % (a, b) for a, b in links) + ";")
    lines.append("")

    with open(OUTPUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))

    print("%s" % OUTPUT)
    print("  %d nodes: %d minor, %d notable, %d keystone" % (
        len(nodes), counts[MINOR], counts[NOTABLE], counts[KEYSTONE]))
    print("  %d links" % len(links))

    stamp = signature(nodes, links)
    write_lua(nodes, links, stamp)
    print("%s" % LUA_OUTPUT)
    print("  signature %d (must match the server's on open)" % stamp)


if __name__ == "__main__":
    sys.exit(main())
