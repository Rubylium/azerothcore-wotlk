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
E_KILL_STREAK, E_SPLASH = range(16, 18)

# What a plain node is worth. The board is mostly these: a tree of nothing but procs would be noise, and the
# quiet nodes are what make the loud ones feel like arriving somewhere.
BASE_VALUE = {MINOR: 8, NOTABLE: 30, KEYSTONE: 0}     # keystones are never plain stats


def special(effect, name, description, icon, value=0, value2=0, chance=0.0,
            duration=0, cooldown=0):
    return {"effect": effect, "name": name, "description": description, "icon": icon,
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
         special(E_FURY, "Ardeur", "10% de chances en infligeant des dégâts d'augmenter vos dégâts de 8% "
                 "pendant 10 s.", "ParagonNode_Ardour", value=8, chance=10.0, duration=10000),
     ],
     special(E_FURY, "Furie du parangon",
             "15% de chances en infligeant des dégâts d'augmenter tous vos dégâts de 20% pendant 12 s.",
             "ParagonNode_Fury", value=20, chance=15.0, duration=12000)),

    (60, ATTACK_POWER, 2.0, "Puissance",
     ("ParagonNode_Power", "ParagonNode_PowerMajor", "ParagonNode_PowerMajor"),
     [
         special(E_SURGE, "Curée", "Tuer un ennemi octroie 120 en puissance d'attaque et des sorts "
                 "pendant 15 s.", "ParagonNode_Quarry", value=120, duration=15000),
     ],
     special(E_SURGE, "Élan du parangon",
             "Tuer un ennemi octroie 350 en puissance d'attaque et des sorts pendant 20 s.",
             "ParagonNode_Momentum", value=350, duration=20000)),

    (120, AGILITY, 1.0, "Agilité",
     ("ParagonNode_Agility", "ParagonNode_AgilityMajor", "ParagonNode_AgilityMajor"),
     [
         special(E_RETALIATE, "Riposte", "10% de chances quand vous êtes touché de renvoyer 25% des dégâts "
                 "à l'attaquant.", "ParagonNode_Riposte", value=25, chance=10.0),
     ],
     special(E_RETALIATE, "Représailles du parangon",
             "20% de chances quand vous êtes touché de renvoyer 60% des dégâts à l'attaquant.",
             "ParagonNode_Reprisal", value=60, chance=20.0)),

    # The armour branch. Minor nodes here are armour rather than stamina, so walking it actually makes you
    # harder to kill instead of just larger.
    (180, STAMINA, 1.2, "Carapace",
     ("ParagonNode_Armor", "ParagonNode_ArmorMajor", "ParagonNode_ArmorMajor"),
     [
         special(E_GUARD, "Écaille de pierre", "10% de chances quand vous êtes touché d'augmenter votre "
                 "armure de 10% pendant 8 s.", "ParagonNode_Stonescale",
                 value=10, chance=10.0, duration=8000),
         special(E_ARMOR_PCT, "Peau d'acier", "Augmente votre armure de 5%.",
                 "ParagonNode_Steelskin", value=5),
     ],
     special(E_LAST_STAND, "Rempart du parangon",
             "Sous 35% de vie, vous subissez 40% de dégâts en moins pendant 10 s. 1 minute de recharge.",
             "ParagonNode_Bulwark", value=40, value2=35, duration=10000, cooldown=60000)),

    (240, SPELL_POWER, 1.4, "Arcanes",
     ("ParagonNode_Arcane", "ParagonNode_ArcaneMajor", "ParagonNode_ArcaneMajor"),
     [
         special(E_FURY, "Résonance", "8% de chances en infligeant des dégâts d'augmenter vos dégâts de 10% "
                 "pendant 10 s.", "ParagonNode_Resonance", value=10, chance=8.0, duration=10000),
     ],
     special(E_FURY, "Cataclysme du parangon",
             "12% de chances en infligeant des dégâts d'augmenter tous vos dégâts de 25% pendant 12 s.",
             "ParagonNode_Cataclysm", value=25, chance=12.0, duration=12000)),

    (300, INTELLECT, 1.0, "Intellect",
     ("ParagonNode_Intellect", "ParagonNode_IntellectMajor", "ParagonNode_IntellectMajor"),
     [
         special(E_SURGE, "Clairvoyance", "Tuer un ennemi octroie 150 en puissance des sorts et d'attaque "
                 "pendant 15 s.", "ParagonNode_Clairvoyance", value=150, duration=15000),
     ],
     special(E_SURGE, "Omniscience du parangon",
             "Tuer un ennemi octroie 400 en puissance des sorts et d'attaque pendant 20 s.",
             "ParagonNode_Omniscience", value=400, duration=20000)),
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
        "minor": 20, "notable": 70, "armor": (400, 1500),
    },
    {
        "name": "Transcendance",
        "rings": [(1590, 7, ()), (1680, 9, (2, 6)), (1770, 9, ()), (1860, 11, (1, 9)), (1950, 11, (5,)),
                  (2040, 9, (0, 8)), (2130, 7, (3,)), (2220, 3, ())],
        "keystone": (7, 1),
        "bridge_ring": 3,
        "minor": 45, "notable": 160, "armor": (900, 3500),
    },
]
OUTER_FIRST_ID = 2000
# Points that must already be spent on the board before a node can be taken, by zone and node type. Without it the
# strongest nodes were a straight run from the hub: ten points to a first keystone, sixteen to an Ascension one,
# twenty-four to an Apotheosis, with nothing spent on the way. The gate makes the far board something reached by
# building a character first. Zone 0 is Eveil, 1 Ascension, 2 Transcendance.
REQUIRED_SPENT = {
    0: {MINOR: 0, NOTABLE: 0, KEYSTONE: 20},
    1: {MINOR: 30, NOTABLE: 40, KEYSTONE: 55},
    2: {MINOR: 65, NOTABLE: 75, KEYSTONE: 95},
}

# A node's reach is drawn this far past the outermost ring, so the frame can pan to its edge
BOARD_MARGIN = 160


def describe(effect, value=0, value2=0, chance=0.0, duration=0, cooldown=0):
    seconds = duration // 1000
    if effect == E_DAMAGE:
        return "Augmente tous vos dégâts de %d%%." % value
    if effect == E_REDUCTION:
        return "Réduit tous les dégâts subis de %d%%." % value
    if effect == E_LEECH:
        return "Vous rend %d%% des dégâts que vous infligez sous forme de points de vie." % value
    if effect == E_DOUBLE:
        return "%d%% de chances que vos coups infligent %d%% de dégâts supplémentaires." % (chance, value)
    if effect == E_EXECUTE:
        return "Vos dégâts sont augmentés de %d%% contre les cibles sous %d%% de vie." % (value, value2)
    if effect == E_EXPLODE:
        return ("Tuer un ennemi le fait exploser : %d%% de ses points de vie maximum infligés aux ennemis à "
                "moins de %d mètres." % (value, value2))
    if effect == E_KILL_STREAK:
        return ("Tuer un ennemi augmente vos dégâts de %d%% pendant %d s, cumulable %d fois. Chaque victime "
                "relance la durée." % (value, seconds, value2))
    if effect == E_SPLASH:
        return ("%d%% des dégâts de vos coups sont aussi infligés à 4 autres ennemis au plus, à moins de %d mètres "
                "de votre cible." % (value, value2))
    if effect == E_UNDYING:
        return ("Un coup qui devrait vous tuer vous laisse à 1 point de vie, et vous êtes insensible aux dégâts "
                "pendant %d s. %d min de recharge." % (seconds, cooldown // 60000))
    if effect == E_HEALTH:
        return "Augmente vos points de vie maximum de %d%%." % value
    if effect == E_FURY:
        return ("%d%% de chances en infligeant des dégâts d'augmenter tous vos dégâts de %d%% pendant %d s."
                % (chance, value, seconds))
    if effect == E_SURGE:
        return "Tuer un ennemi octroie %d en puissance d'attaque et des sorts pendant %d s." % (value, seconds)
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
    "Peau de pierre": "Stoneskin", "Vigueur": "Vigor", "Écailles de granit": "GraniteScales",
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
    return special(effect, name, describe(effect, value, value2, chance, duration, cooldown), icon,
                   value=value, value2=value2, chance=chance, duration=duration, cooldown=cooldown)


# Each branch's notables and keystone in the two outer zones, dealt out in order like the first zone's. Ascension is
# where a build starts to change how a fight goes; Transcendance is where it stops being fair.
OUTER_SPECIALS = {
    "Force": [
        ([outer(E_DAMAGE, "Force brute", "Ability_Warrior_InnerRage", value=4),
          outer(E_DOUBLE, "Double frappe", "Ability_DualWield", value=100, chance=6.0),
          outer(E_EXECUTE, "Coup de grâce", "Ability_Warrior_DecisiveStrike", value=15, value2=30),
          outer(E_DAMAGE, "Colère contenue", "Ability_Warrior_BloodFrenzy", value=5)],
         outer(E_DOUBLE, "Colosse", "Ability_Warrior_Rampage", value=100, chance=15.0)),
        ([outer(E_DAMAGE, "Force titanesque", "Spell_Shadow_UnholyStrength", value=8),
          outer(E_DOUBLE, "Frappes jumelles", "Ability_Warrior_PunishingBlow", value=100, chance=10.0),
          outer(E_EXECUTE, "Exécuteur", "Ability_Rogue_Eviscerate", value=30, value2=35),
          outer(E_EXPLODE, "Onde de choc", "Ability_Warrior_Cleave", value=10, value2=8)],
         outer(E_DOUBLE, "Apothéose : Titan", "INV_Sword_48", value=150, chance=30.0)),
    ],
    "Puissance": [
        ([outer(E_SURGE, "Curée sanglante", "Ability_Rogue_MurderSpree", value=600, duration=20000),
          outer(E_LEECH, "Soif de sang", "Spell_Shadow_LifeDrain02", value=3),
          outer(E_EXPLODE, "Carcasse explosive", "Spell_Fire_SelfDestruct", value=5, value2=6),
          outer(E_DAMAGE, "Élan meurtrier", "Ability_Warrior_Warcry", value=4)],
         outer(E_KILL_STREAK, "Carnage", "Spell_Deathknight_BloodBoil", value=2, value2=8, duration=15000)),
        ([outer(E_SURGE, "Frénésie du massacre", "Spell_Shadow_UnholyFrenzy", value=1500, duration=20000),
          outer(E_LEECH, "Festin", "Spell_Shadow_SoulLeech_3", value=6),
          outer(E_KILL_STREAK, "Réaction en chaîne", "Spell_Fire_Incinerate", value=1, value2=10, duration=15000),
          outer(E_DAMAGE, "Instinct du prédateur", "Ability_Hunter_Pet_Devilsaur", value=8)],
         outer(E_SPLASH, "Apothéose : Cataclysme", "Spell_Fire_MeteorStorm", value=15, value2=10)),
    ],
    "Agilité": [
        ([outer(E_RETALIATE, "Contre-attaque", "Ability_Warrior_Revenge", value=50, chance=20.0),
          outer(E_DOUBLE, "Lames agiles", "Ability_Rogue_SliceDice", value=100, chance=5.0),
          outer(E_REDUCTION, "Esquive parfaite", "Ability_Rogue_Feint", value=4),
          outer(E_DAMAGE, "Précision mortelle", "Ability_Rogue_Feint", value=4)],
         outer(E_RETALIATE, "Épines", "Spell_Nature_Thorns", value=100, chance=35.0)),
        ([outer(E_RETALIATE, "Vengeance", "Ability_Warrior_Revenge", value=100, chance=30.0),
          outer(E_DOUBLE, "Tourbillon de lames", "Ability_Rogue_MurderSpree", value=100, chance=8.0),
          outer(E_REDUCTION, "Insaisissable", "Spell_Arcane_PrismaticCloak", value=6),
          outer(E_LEECH, "Saignée", "Spell_Shadow_LifeDrain02", value=4)],
         outer(E_RETALIATE, "Apothéose : Miroir", "Spell_Holy_AshesToAshes", value=200, chance=50.0)),
    ],
    "Carapace": [
        ([outer(E_REDUCTION, "Peau de pierre", "Ability_Warrior_DefensiveStance", value=5),
          outer(E_HEALTH, "Vigueur", "Spell_Holy_BlessingOfStamina", value=5),
          outer(E_GUARD, "Écailles de granit", "Spell_Holy_PowerWordShield", value=25, chance=20.0, duration=8000),
          outer(E_HEALTH, "Robustesse", "Spell_Nature_Reincarnation", value=4)],
         outer(E_REDUCTION, "Bastion", "Ability_Warrior_ShieldWall", value=12)),
        ([outer(E_REDUCTION, "Peau d'adamantite", "Spell_Holy_DivineIntervention", value=8),
          outer(E_HEALTH, "Colossal", "Spell_Holy_Heroism", value=10),
          outer(E_LAST_STAND, "Dernier rempart", "Spell_Holy_LayOnHands", value=60, value2=40, duration=10000,
                cooldown=60000),
          outer(E_HEALTH, "Endurance infinie", "Spell_Nature_Reincarnation", value=8)],
         outer(E_UNDYING, "Apothéose : Immortel", "Spell_Holy_GuardianSpirit", duration=4000, cooldown=120000)),
    ],
    "Arcanes": [
        ([outer(E_DAMAGE, "Puissance arcanique", "Spell_Arcane_ArcanePotency", value=4),
          outer(E_FURY, "Afflux", "Spell_Arcane_ArcaneTorrent", value=20, chance=15.0, duration=10000),
          outer(E_EXECUTE, "Désintégration", "Spell_Arcane_Blast", value=15, value2=30),
          outer(E_DOUBLE, "Écho", "Spell_Nature_LightningOverload", value=100, chance=5.0)],
         outer(E_FURY, "Surcharge", "Spell_Fire_Fireball02", value=35, chance=25.0, duration=12000)),
        ([outer(E_DAMAGE, "Maîtrise absolue", "Spell_Arcane_MindMastery", value=8),
          outer(E_FURY, "Tempête arcanique", "Spell_Nature_Bloodlust", value=40, chance=20.0, duration=12000),
          outer(E_DOUBLE, "Double incantation", "Spell_Nature_LightningOverload", value=100, chance=10.0),
          outer(E_LEECH, "Siphon", "Spell_Shadow_SiphonMana", value=4)],
         outer(E_DOUBLE, "Apothéose : Singularité", "Spell_Shadow_Twilight", value=200, chance=25.0)),
    ],
    "Intellect": [
        ([outer(E_LEECH, "Rémanence", "Spell_Shadow_LifeDrain02", value=3),
          outer(E_HEALTH, "Esprit fortifié", "Spell_Holy_MindVision", value=4),
          outer(E_SURGE, "Illumination", "Spell_Holy_SurgeOfLight", value=600, duration=20000),
          outer(E_REDUCTION, "Aura protectrice", "Spell_Holy_PowerWordShield", value=3)],
         outer(E_LEECH, "Clarté", "Spell_Holy_BorrowedTime", value=8)),
        ([outer(E_LEECH, "Communion", "Spell_Shadow_SoulLeech_3", value=6),
          outer(E_HEALTH, "Transcendance de l'âme", "Spell_Holy_SealOfMight", value=8),
          outer(E_SURGE, "Révélation", "Spell_Holy_Crusade", value=1500, duration=20000),
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
                        "name": chosen["name"], "branch": branch_name,
                        "description": chosen["description"],
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
    for node in nodes:
        node["required"] = 0 if node["free"] else REQUIRED_SPENT[node.get("zone", 0)][node["type"]]

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
                     "icon": chosen["icon"], "name": chosen["name"], "description": chosen["description"]})
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
        "    nodes = {",
    ]
    for node in nodes:
        lines.append(
            "        [%d] = { type = %d, x = %d, y = %d, effect = %d, stat = %d, value = %d, required = %d,"
            " free = %s, icon = %s, name = %s, description = %s }," % (
                node["id"], node["type"], node["x"], node["y"], node["effect"], node["stat"],
                node["value"], node["required"], "true" if node["free"] else "false",
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
        " `chance`, `duration`, `cooldown`, `free`, `required`, `icon`, `name`, `description`) VALUES",
    ]

    rows = ["    (%d, %d, %d, %d, %d, %d, %d, %d, %g, %d, %d, %d, %d, '%s', '%s', '%s')" % (
        n["id"], n["type"], n["x"], n["y"], n["effect"], n["stat"], n["value"], n["value2"],
        n["chance"], n["duration"], n["cooldown"], n["free"], n["required"],
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
