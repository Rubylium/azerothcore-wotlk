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
# The Panthéon and the glyphs: a socket holds a glyph; the rest are what Blessings and glyph bonuses add on top of the
# effects above. Healing done, healing shared out, more targets for the board's area hits, a lower execute line.
E_SOCKET, E_HEAL_PCT, E_HEAL_SHARE, E_AREA_REACH, E_EXECUTE_REACH = range(26, 31)

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
    # The Panthéon: the ray and the ring are a point a star; a sigil's stars and its figure's key points cost 2, its
    # heart 5. Each carries its points' worth of stat at its kind's rate (a notable's is the better, as further in).
    3: {"minor": 1, "plain": 2, "special": 2, "keystone": 5, "bridge": 3, "star": 2, "socket": 2},
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
         special(E_FURY, "Curée", "ParagonNode_Quarry", value=8, chance=10.0, duration=10000),
     ],
     special(E_FURY, "Élan du parangon", "ParagonNode_Momentum", value=20, chance=15.0, duration=12000)),

    (120, AGILITY, 1.0, "Agilité",
     ("ParagonNode_Agility", "ParagonNode_AgilityMajor", "ParagonNode_AgilityMajor"),
     [
         special(E_DOUBLE, "Riposte", "ParagonNode_Riposte", value=100, chance=10.0),
     ],
     special(E_DOUBLE, "Représailles du parangon", "ParagonNode_Reprisal", value=100, chance=20.0)),

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
        "socket": (4, 5),
        "minor": 20, "notable": 70, "armor": (150, 500),
    },
    {
        "name": "Transcendance",
        "rings": [(1590, 7, ()), (1680, 9, (2, 6)), (1770, 9, ()), (1860, 11, (1, 9)), (1950, 11, (5,)),
                  (2040, 9, (0, 8)), (2130, 7, (3,)), (2220, 3, ())],
        "keystone": (7, 1),
        "bridge_ring": 3,
        "socket": (4, 5),
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
    3: {MINOR: 0, NOTABLE: 0, KEYSTONE: 0},
}

# A node's reach is drawn this far past the outermost ring, so the frame can pan to its edge
BOARD_MARGIN = 160

# How far a spell's arc jumps from its target (ParagonArcRange in ParagonSystem.cpp)
ARC_RANGE = 10

# The caps ParagonSystem.cpp holds the Panthéon's and the glyphs' effects to, named in their texts
MAX_HEAL_PCT = 20
MAX_HEAL_SHARE_PCT = 25
HEAL_SHARE_RANGE = 30
MAX_AREA_TARGETS = 12
MAX_EXECUTE_THRESHOLD = 40
# A glyph reaches the nodes this many links from its socket (GlyphRadius in ParagonSystem.cpp)
GLYPH_RADIUS = 3


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
    # The Panthéon's and the glyphs'
    if effect == E_SOCKET:
        return ("Châsse de glyphe : sertissez-y un glyphe de parangon depuis la fenêtre des glyphes. Les nœuds acquis "
                "à %d liens ou moins gagnent +10%% de leurs valeurs, +2%% par niveau du glyphe." % GLYPH_RADIUS)
    if effect == E_HEAL_PCT:
        return "Augmente les soins que prodiguent vos sorts de %d%% (%d%% au plus)." % (value, MAX_HEAL_PCT)
    if effect == E_HEAL_SHARE:
        return ("%d%% des soins que vous recevez sont aussi rendus à l'allié le plus blessé à moins de %d mètres "
                "(%d%% au plus)." % (value, HEAL_SHARE_RANGE, MAX_HEAL_SHARE_PCT))
    if effect == E_AREA_REACH:
        return ("Vos éclaboussures et vos arcs touchent %d ennemi%s de plus (%d au plus ; au-delà de 5, chacun "
                "reçoit moins)." % (value, "s" if value > 1 else "", MAX_AREA_TARGETS))
    if effect == E_EXECUTE_REACH:
        return ("Vos nœuds d'exécution s'appliquent %d%% de vie plus haut (jusqu'à %d%% de vie au plus)."
                % (value, MAX_EXECUTE_THRESHOLD))
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
        ([outer(E_FURY, "Curée sanglante", "Ability_Rogue_MurderSpree", value=15, chance=12.0, duration=10000),
          outer(E_LEECH, "Soif de sang", "Spell_Shadow_LifeDrain02", value=3),
          outer(E_DAMAGE, "Carcasse explosive", "Spell_Fire_SelfDestruct", value=4),
          outer(E_DAMAGE, "Élan meurtrier", "Ability_Warrior_Warcry", value=4)],
         outer(E_KILL_STREAK, "Carnage", "Spell_Deathknight_BloodBoil", value=2, value2=8, chance=50.0,
               duration=15000)),
        ([outer(E_FURY, "Frénésie du massacre", "Spell_Shadow_UnholyFrenzy", value=25, chance=15.0, duration=12000),
          outer(E_LEECH, "Festin", "Spell_Shadow_SoulLeech_3", value=6),
          outer(E_KILL_STREAK, "Réaction en chaîne", "Spell_Fire_Incinerate", value=1, value2=10, chance=50.0,
                duration=15000),
          outer(E_DAMAGE, "Instinct du prédateur", "Ability_Hunter_Pet_Devilsaur", value=8)],
         outer(E_SPLASH, "Apothéose : Cataclysme", "Spell_Fire_MeteorStorm", value=50, value2=10, chance=20.0,
               cooldown=1000)),
    ],
    "Agilité": [
        ([outer(E_DAMAGE, "Contre-attaque", "Ability_Warrior_Revenge", value=6),
          outer(E_DOUBLE, "Lames agiles", "Ability_Rogue_SliceDice", value=100, chance=15.0),
          outer(E_REDUCTION, "Esquive parfaite", "Ability_Rogue_Feint", value=4),
          outer(E_DAMAGE, "Précision mortelle", "Ability_Rogue_Feint", value=6)],
         outer(E_DOUBLE, "Épines", "Spell_Nature_Thorns", value=100, chance=25.0)),
        ([outer(E_EXECUTE, "Vengeance", "Ability_Warrior_Revenge", value=30, value2=35),
          outer(E_DOUBLE, "Tourbillon de lames", "Ability_Rogue_MurderSpree", value=100, chance=18.0),
          outer(E_DAMAGE, "Insaisissable", "Spell_Arcane_PrismaticCloak", value=10),
          outer(E_LEECH, "Saignée", "Spell_Shadow_LifeDrain02", value=4)],
         outer(E_DOUBLE, "Apothéose : Miroir", "Spell_Holy_AshesToAshes", value=100, chance=35.0)),
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
    wanted |= {SOCKET_ICON, PANTHEON_BRIDGE["icon"]} | {sigil[5] for sigil in SIGILS}
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

    apotheoses = build_outer(nodes, links, branches)
    sigils = build_pantheon(nodes, links, apotheoses)
    sides = {branch[3]: index for index, branch in enumerate(BRANCHES)}
    for node in nodes:
        zone = node.get("zone", 0)
        node["required"] = 0 if node["free"] else REQUIRED_SPENT[zone][node["type"]]
        # The side and tier the gate reads: a side's tier opens once every node of the tier before it on that side
        # is taken. The hub and the bridges belong to no side and are never gated.
        node["tier"] = zone
        node["side"] = sides.get(node["branch"], NO_SIDE)
        node["scope"] = BRANCH_SCOPE.get(node["branch"], SCOPE_ANY)
        node.setdefault("sigil", 0)
        if node["free"]:
            node["cost"] = 0
        elif node.get("cost_fixed"):
            node["cost"] = node["cost_fixed"]
        elif node.get("cost_key"):
            node["cost"] = COST[zone][node["cost_key"]]
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
    return nodes, sorted(links), sigils


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
                    if (ring_index, slot) == zone["socket"]:
                        # A glyph socket, in the middle of the zone so its radius takes in the branch on both sides.
                        # It keeps the id, kind and cost of the node it replaced: a board already holding that node
                        # stays within its cap and its tier.
                        node_type = NOTABLE if slot in notable_slots else MINOR
                        node = make_node(node_id, node_type, x, y, stat, plain=socket_node(zone["name"]))
                    elif (ring_index, slot) == zone["keystone"]:
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
    # Each branch's Apothéose: the middle of its last ring, where the Panthéon's ray starts
    return [end[len(end) // 2] for end in ends]


def socket_node(zone_name):
    return {"effect": E_SOCKET, "value": 0, "icon": SOCKET_ICON, "name": "Châsse %s" % ZONE_OF[zone_name],
            "branch": "", "socket": True, "description": describe(E_SOCKET)}


SOCKET_ICON = "INV_Misc_RunedOrb_01"
ZONE_OF = {"Ascension": "de l'Ascension", "Transcendance": "de la Transcendance", "Le Panthéon": "du Panthéon"}

# ---------------------------------------------------------------------------------------------------------------------
# Le Panthéon, rings 25-40: the sky around the three zones. Seen from afar the first board is a coin at its centre.
#
# - Each branch's Apothéose throws a ray of eight stars straight out to the Anneau des Titans, a ring that joins the
#   six branches all the way round. The ring's star on each branch's axis is that branch's third glyph socket; the six
#   stars halfway between two branches are bridges, open from either side.
# - Past the ring, twelve sigils: the Titans' and the Keepers' runes, geometric (circles, rays, triangles, arcs), two
#   per branch at 15 degrees either side of its axis, so they stand evenly round the sky, one every 30 degrees. Each is
#   reached from the ring by a single star; taking every star of a sigil grants its Blessing.
# - A side's Panthéon opens once its Transcendance is complete, like every tier before it.
#
# Plain stars carry Transcendance's values plus 30%, as much per point on every node: a sigil's stars cost 2 points and
# are worth two of the ring's, its figure's key points 2 at a notable's rate, its heart 5.
# ---------------------------------------------------------------------------------------------------------------------
PANTHEON_NAME = "Le Panthéon"
PANTHEON_FIRST_ID = 3000
PANTHEON_BOOST = 1.3                           # over Transcendance's plain values
PANTHEON_MINOR = 45 * PANTHEON_BOOST           # a point's worth on a minor star, before the branch's scale
PANTHEON_NOTABLE = 160 / 2.0 * PANTHEON_BOOST  # a point's worth on a notable star
PANTHEON_ARMOR = (300 * PANTHEON_BOOST, 1100 / 2.0 * PANTHEON_BOOST)
APOTHEOSIS_RADIUS = 2220                       # Transcendance's last ring
RAY_STARS = 8
RING_RADIUS = 3100
RING_STARS = 120                               # one every 3 degrees: the axes, the bridges and the sigils fall on stars
SIGIL_RADIUS = 600                             # a sigil's outer circle
SIGIL_ENTRY = 3230                             # the star between the ring and a sigil
SIGIL_CENTRE = SIGIL_ENTRY + 140 + SIGIL_RADIUS
SIGIL_OFFSET = 15                              # degrees either side of the branch's axis
MAX_STEP = 330                                 # the longest link a sigil's line runs before it takes a star
SIGIL_ICON_SUFFIX = "Pantheon"


class Sigil:
    """A rune drawn in stars, in its own frame: u along the ring, v away from the board's centre, the heart at 0, 0.
    Every design is symmetric about v: its entry is its lowest star, at (0, -SIGIL_RADIUS)."""

    def __init__(self):
        self.points = []            # (u, v, kind)
        self.edges = set()

    def add(self, u, v, kind=MINOR):
        for index, (pu, pv, pkind) in enumerate(self.points):
            if math.hypot(pu - u, pv - v) < 24:
                if kind > pkind:
                    self.points[index] = (pu, pv, kind)
                return index
        self.points.append((u, v, kind))
        return len(self.points) - 1

    def polar(self, radius, degrees, kind=MINOR):
        return self.add(radius * math.cos(math.radians(degrees)), radius * math.sin(math.radians(degrees)), kind)

    def link(self, a, b):
        if a != b:
            self.edges.add((min(a, b), max(a, b)))

    def line(self, a, b, kind=MINOR, step=MAX_STEP):
        """A straight line from star a to star b: through any star already lying on it, and with a new one wherever
        a stretch would run longer than `step`"""
        (au, av, _), (bu, bv, _) = self.points[a], self.points[b]
        length = math.hypot(bu - au, bv - av)
        stops = [(0.0, a), (1.0, b)]
        for index, (pu, pv, _) in enumerate(self.points):
            if index in (a, b):
                continue
            t = ((pu - au) * (bu - au) + (pv - av) * (bv - av)) / (length * length)
            if 0.0 < t < 1.0 and abs((pu - au) * (bv - av) - (pv - av) * (bu - au)) / length < 12:
                stops.append((t, index))
        stops.sort()
        path = [stops[0][1]]
        for (t0, i0), (t1, i1) in zip(stops, stops[1:]):
            pieces = max(1, int(math.ceil((t1 - t0) * length / step - 0.05)))
            for piece in range(1, pieces):
                t = t0 + (t1 - t0) * piece / pieces
                path.append(self.add(au + (bu - au) * t, av + (bv - av) * t, kind))
            path.append(i1)
        for left, right in zip(path, path[1:]):
            self.link(left, right)
        return path

    def circle(self, radius, count, phase=-90.0, kind=MINOR, degrees=None):
        """`count` stars round a circle, joined in a ring, the first at `phase` (-90: the entry, under the heart). An
        arc when `degrees` (start, end) is given."""
        if degrees:
            start, end = degrees
            ids = [self.polar(radius, start + (end - start) * i / (count - 1.0), kind) for i in range(count)]
            for left, right in zip(ids, ids[1:]):
                self.link(left, right)
            return ids
        ids = [self.polar(radius, phase + 360.0 * i / count, kind) for i in range(count)]
        for left, right in zip(ids, ids[1:] + ids[:1]):
            self.link(left, right)
        return ids

    def heart(self):
        return self.add(0.0, 0.0, KEYSTONE)


# The twelve runes. Each draws its own Titan's or Keeper's sign in the Ulduar manner - circles, rays, triangles, arcs -
# and every one is symmetric about its axis, so the crown of them reads as one design round the sky.
def sigil_khazgoroth():
    """The forge: a square on its point within the circle, a cross joining its opposite sides' middles, and a small
    square at the heart"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 16)
    heart = s.heart()
    corners = [ring[0], ring[4], ring[8], ring[12]]                   # -90, 0, 90, 180
    middles = [s.polar(SIGIL_RADIUS / math.sqrt(2.0), -45.0 + 90.0 * i, NOTABLE) for i in range(4)]
    inner = s.circle(150, 4, phase=-45.0, kind=NOTABLE)
    for index in range(4):
        s.link(corners[index], middles[index])
        s.link(middles[index], corners[(index + 1) % 4])
        # The cross: each side's middle through the small square to the heart
        s.line(middles[index], inner[index], NOTABLE)
        s.link(inner[index], heart)
    return s


def sigil_tyr():
    """Justice: an upright triangle within the circle, one blade straight up through its heart, a guard across it"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 12)
    heart = s.heart()
    top, left, right = ring[6], ring[10], ring[2]                      # 90, 210, 330
    base = s.add(0.0, -SIGIL_RADIUS / 2.0, NOTABLE)
    guard = s.add(0.0, 200.0, NOTABLE)
    s.add(0.0, 400.0, NOTABLE)
    for side in (-1, 1):
        s.link(guard, s.add(side * 160.0, 200.0, NOTABLE))
    s.line(left, top, NOTABLE, step=360)
    s.line(top, right, NOTABLE, step=360)
    s.line(right, left, NOTABLE, step=360)
    s.line(ring[0], top, NOTABLE)                                     # the blade, through base, heart and guard
    return s


def sigil_aggramar():
    """The avenger's burst: eight rays from the heart through a wheel to the circle"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 16)
    heart = s.heart()
    wheel = s.circle(300, 8, kind=NOTABLE)
    for index in range(8):
        s.link(heart, wheel[index])
        s.link(wheel[index], ring[index * 2])
    return s


def sigil_odyn():
    """The Allfather's hall: two circles and the six spokes of its shield"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 18)
    heart = s.heart()
    inner = s.circle(320, 6, kind=NOTABLE)
    for index in range(6):
        s.link(heart, inner[index])
        s.link(inner[index], ring[index * 3])
    return s


def sigil_thorim():
    """The storm: two triangles crossed in a star within the circle"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 18)
    heart = s.heart()
    inner = [s.polar(SIGIL_RADIUS / math.sqrt(3.0), 60.0 * i, NOTABLE) for i in range(6)]
    up = [ring[9], ring[15], ring[3]]                                 # 90, 210, 330
    down = [ring[0], ring[6], ring[12]]                               # -90, 30, 150
    for triangle in (up, down):
        for index in range(3):
            s.line(triangle[index], triangle[(index + 1) % 3], NOTABLE)
    for star in inner:
        s.link(heart, star)
    return s


def sigil_golganneth():
    """The sky and the sea: the horizon across the circle, two concentric arcs of sky above it and their reflection,
    the sea, below"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 12)
    heart = s.heart()
    s.line(ring[9], ring[3])                                          # the horizon, 180 to 0, through the heart
    near = {side: s.add(side * 300.0, 0.0) for side in (-1, 1)}
    for sign in (1, -1):
        outer = s.circle(430, 5, kind=NOTABLE, degrees=(sign * 30, sign * 150))
        inner = s.circle(240, 3, kind=NOTABLE, degrees=(sign * 45, sign * 135))
        s.link(outer[2], ring[6] if sign > 0 else ring[0])
        s.link(outer[0], near[1])
        s.link(outer[-1], near[-1])
        s.link(inner[0], near[1])
        s.link(inner[-1], near[-1])
    del heart
    return s


def sigil_hodir():
    """Winter: a six-armed frost star within a hexagon, each arm barbed like a snowflake's"""
    s = Sigil()
    corners = s.circle(SIGIL_RADIUS, 6)
    heart = s.heart()
    for index in range(6):
        s.line(corners[index], corners[(index + 1) % 6])
    for index, corner in enumerate(corners):
        angle = -90.0 + 60.0 * index
        s.polar(SIGIL_RADIUS / 2.0, angle, NOTABLE)
        s.line(heart, corner, NOTABLE)
        arm = s.polar(SIGIL_RADIUS / 2.0, angle, NOTABLE)
        for side in (-1, 1):
            s.link(arm, s.polar(SIGIL_RADIUS * 0.66, angle + side * 20.0))
    return s


def sigil_mimiron():
    """The engine: twelve even teeth round a wheel, a hub ring within it on six spokes"""
    s = Sigil()
    wheel = s.circle(430, 12)
    heart = s.heart()
    hub = s.circle(200, 6, kind=NOTABLE)
    for index in range(12):
        s.link(wheel[index], s.polar(SIGIL_RADIUS, -90.0 + 30.0 * index))
    for index in range(6):
        s.link(hub[index], wheel[index * 2])
        s.link(heart, hub[index])
    return s


def sigil_norgannon():
    """Lore: the five-pointed star of the arcane within the circle"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 20, phase=90.0)
    long_step = 460                                                   # the star's arms run long, as its points do
    heart = s.heart()
    inner_radius = SIGIL_RADIUS * math.cos(math.radians(72)) / math.cos(math.radians(36))
    inner = [s.polar(inner_radius, 126.0 + 72.0 * i, NOTABLE) for i in range(5)]
    points = [ring[i * 4] for i in range(5)]                          # 90, 162, 234, 306, 18
    for index in range(5):
        s.line(points[index], points[(index + 2) % 5], NOTABLE, step=long_step)
    for star in inner:
        s.link(heart, star)
    return s


def sigil_amanthul():
    """The Highfather's order: three circles, the spheres of the sky, on their two axes"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 12)
    heart = s.heart()
    middle = s.circle(400, 8, kind=NOTABLE)
    inner = s.circle(200, 4, kind=NOTABLE)
    for quarter in range(4):
        s.link(heart, inner[quarter])
        s.link(inner[quarter], middle[quarter * 2])
        s.link(middle[quarter * 2], ring[quarter * 3])
    return s


def sigil_eonar():
    """Life: six petals opening from the heart"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 12)
    heart = s.heart()
    for index in range(6):
        angle = -90.0 + 60.0 * index
        bud = s.polar(240, angle, NOTABLE)
        petal = s.polar(440, angle, NOTABLE)
        s.link(heart, bud)
        s.link(bud, petal)
        s.link(petal, ring[index * 2])
        s.link(petal, ring[index * 2 + 1])
        s.link(petal, ring[index * 2 - 1])
    return s


def sigil_freya():
    """Nature: the tree rune within the circle - a trunk straight up, three pairs of branches at 45 degrees, and
    three pairs of roots mirroring them below"""
    s = Sigil()
    ring = s.circle(SIGIL_RADIUS, 12)
    heart = s.heart()
    s.line(ring[0], ring[6], NOTABLE, step=210)                       # the trunk, bottom to top, every 200
    for height in (0.0, 200.0, 400.0):
        for sign in (1, -1):                                          # the branches up, the roots down
            origin = s.add(0.0, sign * height)
            reach = 250.0 if height < 400.0 else 180.0
            for side in (-1, 1):
                s.link(origin, s.add(side * reach * 0.7071, sign * (height + reach * 0.7071), NOTABLE))
    del heart
    return s


# The Blessing a completed sigil grants: a named effect of moderate size, through the board's own caps. Two sigils a
# branch, each in its way of fighting.
def blessing(effect, name, value=0, value2=0, chance=0.0, duration=0, cooldown=0):
    return {"effect": effect, "name": name, "value": value, "value2": value2, "chance": chance,
            "duration": duration, "cooldown": cooldown}


# (branch, offset from its axis, titan, epithet, design, heart icon, blessing)
SIGILS = [
    ("Force", -SIGIL_OFFSET, "Khaz'goroth", "le Façonneur", sigil_khazgoroth, "INV_Hammer_Unique_Sulfuras",
     blessing(E_DAMAGE, "Bénédiction de Khaz'goroth", value=5)),
    ("Force", SIGIL_OFFSET, "Tyr", "le Juste", sigil_tyr, "Spell_Holy_CrusaderStrike",
     blessing(E_EXECUTE, "Bénédiction de Tyr", value=15, value2=35)),
    ("Puissance", -SIGIL_OFFSET, "Aggramar", "le Vengeur", sigil_aggramar, "Ability_Warrior_TitansGrip",
     blessing(E_SPLASH, "Bénédiction d'Aggramar", value=25, value2=8, chance=15.0, cooldown=1500)),
    ("Puissance", SIGIL_OFFSET, "Odyn", "le Père de tout", sigil_odyn, "Achievement_Dungeon_UlduarRaid_Titan_01",
     blessing(E_KILL_STREAK, "Bénédiction d'Odyn", value=1, value2=10, chance=50.0, duration=15000)),
    ("Agilité", -SIGIL_OFFSET, "Thorim", "Seigneur des tempêtes", sigil_thorim, "Achievement_Boss_Thorim",
     blessing(E_DOUBLE, "Bénédiction de Thorim", value=100, chance=6.0)),
    ("Agilité", SIGIL_OFFSET, "Golganneth", "Seigneur des cieux et des mers", sigil_golganneth,
     "Spell_Nature_CallStorm", blessing(E_RETALIATE, "Bénédiction de Golganneth", value=80, chance=20.0)),
    ("Carapace", -SIGIL_OFFSET, "Hodir", "le Gardien de l'hiver", sigil_hodir, "Achievement_Boss_Hodir_01",
     blessing(E_REDUCTION, "Bénédiction de Hodir", value=4)),
    ("Carapace", SIGIL_OFFSET, "Mimiron", "l'Inventeur", sigil_mimiron, "Achievement_Boss_Mimiron_01",
     blessing(E_ARMOR_PCT, "Bénédiction de Mimiron", value=8)),
    ("Arcanes", -SIGIL_OFFSET, "Norgannon", "le Gardien du savoir", sigil_norgannon, "Spell_Arcane_Arcane04",
     blessing(E_ECHO, "Bénédiction de Norgannon", value=60, chance=12.0)),
    ("Arcanes", SIGIL_OFFSET, "Aman'Thul", "le Haut-Père", sigil_amanthul, "Achievement_Dungeon_UlduarRaid_Misc_01",
     blessing(E_DAMAGE, "Bénédiction d'Aman'Thul", value=5)),
    ("Intellect", -SIGIL_OFFSET, "Eonar", "la Gardienne de la vie", sigil_eonar, "Spell_Nature_HealingTouch",
     blessing(E_HEAL_PCT, "Bénédiction d'Eonar", value=6)),
    ("Intellect", SIGIL_OFFSET, "Freya", "la Gardienne de la nature", sigil_freya, "Achievement_Boss_Freya_01",
     blessing(E_HEAL_SHARE, "Bénédiction de Freya", value=5)),
]


def pantheon_plain(branch_name, stat, scale, icons, kind, cost):
    """A plain star: its points' worth of the branch's stat (or armour), at the Panthéon's rate"""
    icon = "%s%s%s|%s" % (icons[0], SIGIL_ICON_SUFFIX, "Major" if kind != MINOR else "", icons[min(kind, NOTABLE)])
    if branch_name == ARMOR_BRANCH:
        amount = int(round((PANTHEON_ARMOR[0] if kind == MINOR else PANTHEON_ARMOR[1]) * cost))
        return {"effect": E_ARMOR, "value": amount, "icon": icon,
                "name": "Carapace céleste" if kind == MINOR else "Carapace céleste majeure",
                "branch": branch_name, "description": "+%d Armure" % amount}
    value = int(round((PANTHEON_MINOR if kind == MINOR else PANTHEON_NOTABLE) * cost * scale))
    return {"effect": E_STAT, "value": value, "icon": icon,
            "name": "%s céleste" % branch_name if kind == MINOR else "%s céleste majeur" % branch_name,
            "branch": branch_name, "description": "+%d %s" % (value, STAT_NAME[stat])}


def check_symmetry(name, sigil):
    """A rune must be its own mirror about its axis: every star and every line reflected (u to -u) is one of its own"""
    def find(u, v):
        for index, (pu, pv, _) in enumerate(sigil.points):
            if math.hypot(pu - u, pv - v) < 3:
                return index
        return None
    mirror = {}
    for index, (u, v, _) in enumerate(sigil.points):
        other = find(-u, v)
        if other is None:
            raise SystemExit("sigil %s is not symmetric: no mirror for the star at %d, %d" % (name, u, v))
        mirror[index] = other
    for a, b in sigil.edges:
        if (min(mirror[a], mirror[b]), max(mirror[a], mirror[b])) not in sigil.edges:
            raise SystemExit("sigil %s is not symmetric: no mirror for the line %s - %s" % (
                name, sigil.points[a][:2], sigil.points[b][:2]))


def build_pantheon(nodes, links, apotheoses):
    """The fourth zone. Returns the sigils as the frame and the server need them."""
    node_id = PANTHEON_FIRST_ID
    branch_of = {branch[3]: branch for branch in BRANCHES}

    def add(node):
        nonlocal node_id
        node["id"] = node_id
        node["zone"] = 3
        nodes.append(node)
        node_id += 1
        return node["id"]

    def at(radius, degrees):
        return (int(round(radius * math.cos(math.radians(degrees)))),
                int(round(radius * math.sin(math.radians(degrees)))))

    def link(a, b):
        links.add((min(a, b), max(a, b)))

    # The Anneau des Titans first, so its stars can be named by angle: a star every 3 degrees from 0
    ring = {}
    for index in range(RING_STARS):
        degrees = 360.0 * index / RING_STARS
        x, y = at(RING_RADIUS, degrees)
        nearest = min(BRANCHES, key=lambda b: abs((degrees - b[0] + 180.0) % 360.0 - 180.0))
        angle, stat, scale, branch_name, icons = nearest[:5]
        off_axis = abs((degrees - angle + 180.0) % 360.0 - 180.0)
        if abs(off_axis - 30.0) < 0.01:
            node = make_node(0, NOTABLE, x, y, stat, chosen=PANTHEON_BRIDGE)
            node.update({"branch": "", "bridge": True})
        elif off_axis < 0.01:
            node = make_node(0, NOTABLE, x, y, stat, plain=socket_node(PANTHEON_NAME))
            node["cost_key"] = "socket"
        elif abs(off_axis - SIGIL_OFFSET) < 0.01:
            node = make_node(0, NOTABLE, x, y, stat,
                             plain=pantheon_plain(branch_name, stat, scale, icons, NOTABLE, COST[3]["plain"]))
        else:
            node = make_node(0, MINOR, x, y, stat,
                             plain=pantheon_plain(branch_name, stat, scale, icons, MINOR, COST[3]["minor"]))
        if not node.get("bridge"):
            node["branch"] = branch_name
        ring[index] = add(node)
    for index in range(RING_STARS):
        link(ring[index], ring[(index + 1) % RING_STARS])

    def ring_star(degrees):
        return ring[int(round((degrees % 360.0) * RING_STARS / 360.0)) % RING_STARS]

    # The rays: from each Apothéose straight out to the ring, the middle star a notable
    for branch_index, (angle, stat, scale, branch_name, icons, _, _) in enumerate(BRANCHES):
        previous = apotheoses[branch_index]
        step = (RING_RADIUS - APOTHEOSIS_RADIUS) / (RAY_STARS + 1.0)
        for index in range(RAY_STARS):
            kind = NOTABLE if index == RAY_STARS // 2 else MINOR
            x, y = at(APOTHEOSIS_RADIUS + step * (index + 1), angle)
            cost = COST[3]["plain" if kind == NOTABLE else "minor"]
            node = make_node(0, kind, x, y, stat, plain=pantheon_plain(branch_name, stat, scale, icons, kind, cost))
            node["branch"] = branch_name
            star = add(node)
            link(previous, star)
            previous = star
        link(previous, ring_star(angle))

    # The sigils, each hung from the ring by one star
    sigils = []
    for number, (branch_name, offset, titan, epithet, design, heart_icon, bless) in enumerate(SIGILS, start=1):
        angle, stat, scale, _, icons = branch_of[branch_name][:5]
        degrees = angle + offset
        sigil = design()
        check_symmetry(titan, sigil)
        radial = (math.cos(math.radians(degrees)), math.sin(math.radians(degrees)))
        tangent = (-radial[1], radial[0])
        cx, cy = SIGIL_CENTRE * radial[0], SIGIL_CENTRE * radial[1]

        entry_x, entry_y = at(SIGIL_ENTRY, degrees)
        entry = make_node(0, NOTABLE, entry_x, entry_y, stat,
                          plain=pantheon_plain(branch_name, stat, scale, icons, NOTABLE, COST[3]["plain"]))
        entry["branch"] = branch_name
        entry_id = add(entry)
        link(ring_star(degrees), entry_id)

        scope = BRANCH_SCOPE[branch_name]
        text = describe(bless["effect"], bless["value"], bless["value2"], bless["chance"], bless["duration"],
                        bless["cooldown"], scope)
        ids = []
        heart_id = None
        for (u, v, kind) in sigil.points:
            x = int(round(cx + u * tangent[0] + v * radial[0]))
            y = int(round(cy + u * tangent[1] + v * radial[1]))
            if kind == KEYSTONE:
                cost = COST[3]["keystone"]
                plain = pantheon_plain(branch_name, stat, scale, icons, NOTABLE, cost)
                plain.update({"icon": heart_icon, "name": "Cœur de %s" % titan})
                plain["description"] = ("%s. Cœur du sigille de %s : prenez chacune de ses étoiles pour recevoir la "
                                        "%s. %s" % (plain["description"], titan, bless["name"], text))
            else:
                cost = COST[3]["special" if kind == NOTABLE else "star"]
                plain = pantheon_plain(branch_name, stat, scale, icons, kind, cost)
            node = make_node(0, kind, x, y, stat, plain=plain)
            node.update({"branch": branch_name, "sigil": number, "cost_fixed": cost})
            ids.append(add(node))
            if kind == KEYSTONE:
                heart_id = ids[-1]
        for a, b in sigil.edges:
            link(ids[a], ids[b])
        # The entry reaches the rune's lowest star
        lowest = min(range(len(sigil.points)), key=lambda i: (sigil.points[i][1], abs(sigil.points[i][0])))
        link(entry_id, ids[lowest])

        sigils.append({"id": number, "titan": titan, "epithet": epithet, "branch": branch_name,
                       "side": [b[3] for b in BRANCHES].index(branch_name), "scope": scope,
                       "x": int(round(cx)), "y": int(round(cy)), "heart": heart_id, "nodes": ids,
                       "blessing": bless, "description": text})
    return sigils


# ---------------------------------------------------------------------------------------------------------------------
# Paragon glyphs: thirty legendary items, five a branch, set into the board's eighteen sockets (three a branch: in the
# Ascension, the Transcendance and the Panthéon). A socketed glyph raises every allocated node within GLYPH_RADIUS links
# by 10% of its values, +2% a level (level 25: +60%); once `need` nodes of its own branch are allocated in that
# radius, it also gives its bonus, a small effect through the board's own caps.
#
# Item ids: below 65536 (above, the client reads a Mythic+ generated item) and already in the client's Item.dbc, with no
# item_template on the server before these: 13642-13671, empty "ornate box" rows of class 15. Their icons are the
# inscription glyphs' (ItemDisplayInfo ids below), which a client Item.dbc patch gives them
# (localTools/paragon/glyphItemDisplays.json, applied by patchSinisterStrike.ps1).
# ---------------------------------------------------------------------------------------------------------------------
GLYPH_FIRST_ITEM = 13642
GLYPH_MAX_LEVEL = 25
# The inscription glyphs' displays (client ItemDisplayInfo.dbc): 21 major, then minor
GLYPH_DISPLAYS = [
    (54733, "INV_Inscription_MajorGlyph00"), (57346, "INV_Inscription_MajorGlyph01"),
    (57356, "INV_Inscription_MajorGlyph02"), (57349, "INV_Inscription_MajorGlyph03"),
    (57345, "INV_Inscription_MajorGlyph04"), (57353, "INV_Inscription_MajorGlyph05"),
    (54954, "INV_Inscription_MajorGlyph06"), (57351, "INV_Inscription_MajorGlyph07"),
    (57348, "INV_Inscription_MajorGlyph08"), (57344, "INV_Inscription_MajorGlyph09"),
    (57347, "INV_Inscription_MajorGlyph10"), (57352, "INV_Inscription_MajorGlyph11"),
    (55304, "INV_Inscription_MajorGlyph12"), (57343, "INV_Inscription_MajorGlyph13"),
    (57359, "INV_Inscription_MajorGlyph14"), (57358, "INV_Inscription_MajorGlyph15"),
    (54953, "INV_Inscription_MajorGlyph16"), (57357, "INV_Inscription_MajorGlyph17"),
    (57354, "INV_Inscription_MajorGlyph18"), (57355, "INV_Inscription_MajorGlyph19"),
    (57350, "INV_Inscription_MajorGlyph20"), (57362, "INV_Inscription_MinorGlyph00"),
    (57364, "INV_Inscription_MinorGlyph01"), (57367, "INV_Inscription_MinorGlyph02"),
    (54728, "INV_Inscription_MinorGlyph03"), (54730, "INV_Inscription_MinorGlyph04"),
    (54958, "INV_Inscription_MinorGlyph05"), (57371, "INV_Inscription_MinorGlyph06"),
    (54727, "INV_Inscription_MinorGlyph07"), (54729, "INV_Inscription_MinorGlyph08"),
]


def glyph(branch, name, lore, bonus, need):
    return {"branch": branch, "name": name, "lore": lore, "bonus": bonus, "need": need}


GLYPHS = [
    # Force: weapon attacks, the execute and the double strike
    glyph("Force", "Glyphe de l'Enclume", "Frappé sur l'enclume de Khaz'goroth, il sonne encore quand on le serre.",
          special(E_DAMAGE, "", "", value=3), 8),
    glyph("Force", "Glyphe du Verdict", "Tyr ne frappait qu'une fois. Il n'avait jamais besoin d'une deuxième.",
          special(E_EXECUTE_REACH, "", "", value=10), 10),
    glyph("Force", "Glyphe du Colosse", "Les géants de fer d'Ulduar portaient ce signe gravé sur le poing.",
          special(E_DOUBLE, "", "", value=100, chance=3.0), 10),
    glyph("Force", "Glyphe de la Forge ardente", "Une braise de la Forge des volontés, prisonnière d'une rune.",
          special(E_FURY, "", "", value=6, chance=8.0, duration=8000), 8),
    glyph("Force", "Glyphe du Titan", "Qui porte ce glyphe se tient comme une montagne se tient.",
          special(E_HEALTH, "", "", value=3), 6),
    # Puissance: kills, the pack, attack power
    glyph("Puissance", "Glyphe de la Curée", "Chaque proie abattue nourrit la suivante.",
          special(E_SURGE, "", "", value=400, duration=15000), 8),
    glyph("Puissance", "Glyphe du Carnage", "Odyn comptait ses guerriers à la hauteur de leur tas d'ennemis.",
          special(E_KILL_STREAK, "", "", value=1, value2=5, chance=30.0, duration=15000), 10),
    glyph("Puissance", "Glyphe de la Déflagration", "Aggramar ne frappait jamais un démon seul.",
          special(E_SPLASH, "", "", value=15, value2=8, chance=8.0, cooldown=2000), 10),
    glyph("Puissance", "Glyphe du Festin", "Le sang versé revient toujours à celui qui l'a versé.",
          special(E_LEECH, "", "", value=2), 8),
    glyph("Puissance", "Glyphe de la Horde", "Plus ils sont nombreux, plus le coup porte loin.",
          special(E_AREA_REACH, "", "", value=2), 12),
    # Agilité: blows given back, blows avoided
    glyph("Agilité", "Glyphe du Miroir", "Ce que l'on vous envoie vous appartient désormais.",
          special(E_RETALIATE, "", "", value=40, chance=10.0), 8),
    glyph("Agilité", "Glyphe de l'Ombre", "Taillé dans la nuit qui précède l'orage de Thorim.",
          special(E_REDUCTION, "", "", value=2), 10),
    glyph("Agilité", "Glyphe des Lames jumelles", "Deux éclairs ne tombent jamais au même endroit. Sauf ici.",
          special(E_DOUBLE, "", "", value=100, chance=3.0), 10),
    glyph("Agilité", "Glyphe de la Saignée", "La plus fine des lames laisse la plus longue des traces.",
          special(E_LEECH, "", "", value=2), 8),
    glyph("Agilité", "Glyphe du Vent", "Golganneth y a soufflé la vitesse des tempêtes.",
          special(E_DAMAGE, "", "", value=3), 6),
    # Carapace: the tank's
    glyph("Carapace", "Glyphe du Rempart", "Hodir a tenu seul une passe gelée contre une armée entière.",
          special(E_REDUCTION, "", "", value=2), 10),
    glyph("Carapace", "Glyphe de Granit", "Une écaille de la peau de pierre des premiers terrestres.",
          special(E_ARMOR_PCT, "", "", value=4), 8),
    glyph("Carapace", "Glyphe de Rancune", "Chaque coup reçu est une dette. Celui-ci tient les comptes.",
          special(E_GRUDGE, "", "", value=5, value2=2), 10),
    glyph("Carapace", "Glyphe du Défi", "Le rugissement gravé dans cette rune attire les regards, et les coups.",
          special(E_THREAT, "", "", value=15), 6),
    glyph("Carapace", "Glyphe de Vie", "Forgé par Mimiron pour qu'une machine ne s'arrête jamais.",
          special(E_HEALTH, "", "", value=4), 8),
    # Arcanes: spells, their echoes and arcs
    glyph("Arcanes", "Glyphe de Résonance", "Norgannon y a écrit un mot qui ne cesse de se répéter.",
          special(E_ECHO, "", "", value=50, chance=8.0), 10),
    glyph("Arcanes", "Glyphe de la Tempête", "Un fragment de l'orage qui gronde au-dessus d'Ulduar.",
          special(E_ARC, "", "", value=20, value2=3, chance=8.0, cooldown=1000), 10),
    glyph("Arcanes", "Glyphe de Célérité", "Le temps d'Aman'Thul s'y écoule un peu plus vite qu'ailleurs.",
          special(E_QUICKEN, "", "", value=8, chance=10.0, duration=8000), 8),
    glyph("Arcanes", "Glyphe de Puissance arcanique", "Il brûle les doigts de ceux qui ne savent pas le lire.",
          special(E_DAMAGE, "", "", value=3), 8),
    glyph("Arcanes", "Glyphe de la Singularité", "Tout ce qui faiblit y est attiré, puis englouti.",
          special(E_EXECUTE_REACH, "", "", value=10), 12),
    # Intellect: healing, wards, mana
    glyph("Intellect", "Glyphe de Sève", "Une goutte de la rosée des jardins d'Eonar.",
          special(E_HEAL_PCT, "", "", value=3), 8),
    glyph("Intellect", "Glyphe du Partage", "Freya ne gardait rien pour elle, pas même la vie.",
          special(E_HEAL_SHARE, "", "", value=5), 10),
    glyph("Intellect", "Glyphe de l'Égide", "Une rune de garde que les Gardiens gravaient sur leurs portes.",
          special(E_WARD, "", "", value=80, chance=10.0, duration=8000, cooldown=20000), 10),
    glyph("Intellect", "Glyphe de Clarté", "L'esprit qui le porte ne connaît pas la fatigue.",
          special(E_MANA_SURGE, "", "", value=2, chance=8.0, cooldown=6000), 8),
    glyph("Intellect", "Glyphe de Révélation", "Il montre, un instant, ce que les Titans ont vu.",
          special(E_INSIGHT, "", "", value=250, chance=10.0, duration=10000), 8),
]
assert len(GLYPHS) == 30 and len(GLYPH_DISPLAYS) == 30


# The items' English names and lore, for item_template (the French are the locale rows)
GLYPH_ENGLISH = [
    ("Glyph of the Anvil", "Struck on Khaz'goroth's anvil, it still rings when held tight."),
    ("Glyph of the Verdict", "Tyr only ever struck once. He never needed a second blow."),
    ("Glyph of the Colossus", "The iron giants of Ulduar bore this sign carved on their fists."),
    ("Glyph of the Burning Forge", "An ember of the Forge of Wills, caught in a rune."),
    ("Glyph of the Titan", "Whoever bears this glyph stands as a mountain stands."),
    ("Glyph of the Quarry", "Every prey brought down feeds the next."),
    ("Glyph of Carnage", "Odyn counted his warriors by the height of their foes' pile."),
    ("Glyph of Deflagration", "Aggramar never struck a demon alone."),
    ("Glyph of the Feast", "Blood that is spilled always returns to the one who spilled it."),
    ("Glyph of the Horde", "The more of them there are, the farther the blow carries."),
    ("Glyph of the Mirror", "What is sent your way now belongs to you."),
    ("Glyph of Shadow", "Carved in the night before Thorim's storm."),
    ("Glyph of Twin Blades", "Lightning never strikes twice in the same place. Except here."),
    ("Glyph of Bloodletting", "The finest blade leaves the longest trail."),
    ("Glyph of the Wind", "Golganneth breathed the speed of storms into it."),
    ("Glyph of the Rampart", "Hodir once held a frozen pass alone against a whole army."),
    ("Glyph of Granite", "A scale of the first earthen's stone skin."),
    ("Glyph of Rancor", "Every blow taken is a debt. This one keeps the books."),
    ("Glyph of Defiance", "The roar carved into this rune draws every eye, and every blow."),
    ("Glyph of Life", "Forged by Mimiron so that a machine would never stop."),
    ("Glyph of Resonance", "Norgannon wrote a word in it that never stops repeating."),
    ("Glyph of the Tempest", "A shard of the storm that rumbles over Ulduar."),
    ("Glyph of Celerity", "Aman'Thul's time runs a little faster in it than anywhere else."),
    ("Glyph of Arcane Might", "It burns the fingers of those who cannot read it."),
    ("Glyph of the Singularity", "All that weakens is drawn into it, then swallowed."),
    ("Glyph of Sap", "A drop of dew from the gardens of Eonar."),
    ("Glyph of Sharing", "Freya kept nothing for herself, not even life."),
    ("Glyph of the Aegis", "A warding rune the Keepers carved upon their doors."),
    ("Glyph of Clarity", "The mind that bears it knows no weariness."),
    ("Glyph of Revelation", "It shows, for a moment, what the Titans saw."),
]
assert len(GLYPH_ENGLISH) == 30

GLYPH_ITEM_SQL = os.path.join(REPO, "modules", "mod-stat-growth", "data", "sql", "db-world", "base",
                              "stat_growth_paragon_glyph_items.sql")
# What the client patch reads to give the glyph items their icons (Item.dbc's display id per item)
GLYPH_DISPLAY_JSON = os.path.join(REPO, "localTools", "paragon", "glyphItemDisplays.json")
# Learning (483): a harmless on-use spell, so the client lets the item be used; the item script does the work
GLYPH_USE_SPELL = 483


def write_glyph_items(glyphs):
    """The glyph items' templates: legendary, soulbound, one a stack, used from the bags to learn or absorb"""
    first, last = glyphs[0]["item"], glyphs[-1]["item"]
    lines = [
        "-- Generated by localTools/paragon/buildParagonTree.py -- do not edit by hand.",
        "--",
        "-- The paragon glyphs as items (ParagonSystem.cpp): legendary, soulbound, one a stack. Using one from the",
        "-- bags learns it into the character's collection, or absorbs a copy of one already known as experience.",
        "-- Entries %d-%d: below 65536 and already in the client's Item.dbc (empty class 15 rows) with no"
        % (first, last),
        "-- template on the server before these. Their icons are the inscription glyphs', set in the client's",
        "-- Item.dbc by localTools/patchSinisterStrike.ps1 from localTools/paragon/glyphItemDisplays.json.",
        "DELETE FROM `item_template_locale` WHERE `ID` BETWEEN %d AND %d;" % (first, last),
        "DELETE FROM `item_template` WHERE `entry` BETWEEN %d AND %d;" % (first, last),
        "INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`,"
        " `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`,"
        " `RequiredLevel`, `maxcount`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`,"
        " `description`, `Material`, `ScriptName`, `VerifiedBuild`) VALUES",
    ]
    rows = []
    for g in glyphs:
        name, lore = GLYPH_ENGLISH[g["id"] - 1]
        rows.append("(%d, 15, 0, '%s', %d, 5, 0, 1, 0, 0, 0, -1, -1, 80, 80, 0, 1, 1, %d, 0, 0, '%s', 0, "
                    "'item_paragon_glyph', 12340)" % (g["item"], escape(name), g["display"], GLYPH_USE_SPELL,
                                                      escape(lore)))
    lines.append(",\n".join(rows) + ";")
    lines.append("")
    lines.append("INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES")
    lines.append(",\n".join("(%d, 'frFR', '%s', '%s', 12340)" % (g["item"], escape(g["name"]), escape(g["lore"]))
                            for g in glyphs) + ";")
    lines.append("")
    with open(GLYPH_ITEM_SQL, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))

    displays = ["{\"item\": %d, \"display\": %d, \"icon\": \"%s\"}" % (g["item"], g["display"], g["icon"])
                for g in glyphs]
    with open(GLYPH_DISPLAY_JSON, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("[\n  " + ",\n  ".join(displays) + "\n]\n")
    print("%s\n%s" % (GLYPH_ITEM_SQL, GLYPH_DISPLAY_JSON))


def build_glyphs():
    glyphs = []
    sides = [b[3] for b in BRANCHES]
    for index, g in enumerate(GLYPHS):
        scope = BRANCH_SCOPE[g["branch"]]
        bonus = g["bonus"]
        display, icon = GLYPH_DISPLAYS[index]
        glyphs.append({
            "id": index + 1, "item": GLYPH_FIRST_ITEM + index, "display": display, "icon": icon,
            "side": sides.index(g["branch"]), "branch": g["branch"], "scope": scope, "name": g["name"],
            "lore": g["lore"], "need": g["need"], "effect": bonus["effect"], "value": bonus["value"],
            "value2": bonus["value2"], "chance": bonus["chance"], "duration": bonus["duration"],
            "cooldown": bonus["cooldown"],
            "description": describe(bonus["effect"], bonus["value"], bonus["value2"], bonus["chance"],
                                    bonus["duration"], bonus["cooldown"], scope),
        })
    return glyphs


# The ring's bridges, halfway between two branches: open from either side, like the bridges further in
PANTHEON_BRIDGE = outer(E_HEALTH, "Conjonction", "Spell_Arcane_PortalDalaran", value=3) \
    if "Conjonction" in CUSTOM_ICON else special(E_HEALTH, "Conjonction", "Spell_Arcane_PortalDalaran", value=3)


def signature(nodes, links):
    """Must match LoadParagonBoard in ParagonSystem.cpp, wraparound included."""
    total = 0
    for node in nodes:
        total += node["id"] * 31 + node["value"] * 7 + node["stat"] + node["effect"] * 3 + node["required"] * 5
        total += node["cost"] * 11 + node["tier"] * 19 + node["side"] * 23 + node["scope"] * 29
        total += node["sigil"] * 37
    for a, b in links:
        total += a * 13 + b * 17
    return total % (2 ** 32)


def write_lua(nodes, links, stamp, sigils, glyphs):
    extent = 2 * (max(max(abs(n["x"]), abs(n["y"])) for n in nodes) + BOARD_MARGIN)
    zones = [("Éveil", 0)] + [(zone["name"], zone["rings"][0][0] - 45) for zone in OUTER_ZONES]
    zones.append((PANTHEON_NAME, APOTHEOSIS_RADIUS + 330))
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
        # The Panthéon's sky: where its starfield starts and ends, and its ring, for the frame's background
        "    sky = { inner = %d, ring = %d, outer = %d }," % (APOTHEOSIS_RADIUS + 90, RING_RADIUS,
                                                             SIGIL_CENTRE + SIGIL_RADIUS + 250),
        "    glyphRadius = %d, glyphMaxLevel = %d, socketEffect = %d," % (GLYPH_RADIUS, GLYPH_MAX_LEVEL, E_SOCKET),
        "    nodes = {",
    ]
    for node in nodes:
        lines.append(
            "        [%d] = { type = %d, x = %d, y = %d, effect = %d, stat = %d, value = %d, required = %d,"
            " cost = %d, side = %d, tier = %d, scope = %d,%s"
            " free = %s, icon = %s, name = %s, description = %s }," % (
                node["id"], node["type"], node["x"], node["y"], node["effect"], node["stat"],
                node["value"], node["required"], node["cost"], node["side"], node["tier"], node["scope"],
                (" sigil = %d," % node["sigil"]) if node["sigil"] else "",
                "true" if node["free"] else "false",
                lua_string("Interface" + chr(92) + "Icons" + chr(92) + node["icon"]),
                lua_string(node["name"]), lua_string(node["description"])))
    lines += ["    },", "    sigils = {"]
    for sigil in sigils:
        lines.append(
            "        [%d] = { titan = %s, epithet = %s, side = %d, x = %d, y = %d, heart = %d, blessing = %s,"
            " description = %s, nodes = { %s } }," % (
                sigil["id"], lua_string(sigil["titan"]), lua_string(sigil["epithet"]), sigil["side"], sigil["x"],
                sigil["y"], sigil["heart"], lua_string(sigil["blessing"]["name"]), lua_string(sigil["description"]),
                ", ".join(str(i) for i in sigil["nodes"])))
    lines += ["    },", "    glyphs = {"]
    for g in glyphs:
        lines.append(
            "        [%d] = { item = %d, side = %d, need = %d, icon = %s, name = %s, lore = %s, description = %s },"
            % (g["id"], g["item"], g["side"], g["need"],
               lua_string("Interface" + chr(92) + "Icons" + chr(92) + g["icon"]), lua_string(g["name"]),
               lua_string(g["lore"]), lua_string(g["description"])))
    # Last: localTools/interface/buildParagonArt.py reads the link shapes from here to the end
    lines += ["    },", "    links = {"]
    for a, b in links:
        lines.append("        { %d, %d }," % (a, b))
    lines += ["    },", "}", ""]

    with open(LUA_OUTPUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))


def lua_string(text):
    return '"' + text.replace(chr(92), chr(92) * 2).replace('"', chr(92) + '"') + '"'


def main():
    nodes, links, sigils = build()
    glyphs = build_glyphs()

    counts = {MINOR: 0, NOTABLE: 0, KEYSTONE: 0}
    for node in nodes:
        counts[node["type"]] += 1

    lines = [
        "-- Generated by localTools/paragon/buildParagonTree.py -- do not edit by hand.",
        "--",
        "-- The paragon board: a hub and six stat branches through three zones (Éveil, Ascension,",
        "-- Transcendance), %d rings deep, and the Panthéon's sky around them: a ray from each Apothéose, the ring"
        % (len(RINGS) + sum(len(z["rings"]) for z in OUTER_ZONES)),
        "-- of the Titans and twelve sigils. %d nodes (%d minor, %d notable, %d keystone), %d points in all."
        % (len(nodes), counts[MINOR], counts[NOTABLE], counts[KEYSTONE], sum(n["cost"] for n in nodes)),
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
        "    `tier` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 Eveil, 1 Ascension, 2 Transcendance, 3 Pantheon',",
        "    `scope` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'what sets it off: 0 anything, 1 weapons, 2 spells',",
        "    `sigil` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'the Pantheon sigil it belongs to, 0 for none',",
        "    `icon` VARCHAR(128) NOT NULL DEFAULT '',",
        "    `name` VARCHAR(64) NOT NULL DEFAULT '',",
        "    `description` VARCHAR(512) NOT NULL DEFAULT '',",
        "    PRIMARY KEY (`id`)",
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
        "",
        "-- The Blessing each completed sigil grants: an effect like a node's, through the same caps",
        "DROP TABLE IF EXISTS `paragon_blessing`;",
        "CREATE TABLE `paragon_blessing` (",
        "    `sigil` TINYINT UNSIGNED NOT NULL,",
        "    `effect` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ParagonEffect',",
        "    `value` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `value2` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `chance` FLOAT NOT NULL DEFAULT 0,",
        "    `duration` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `cooldown` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `scope` TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `name` VARCHAR(64) NOT NULL DEFAULT '',",
        "    `description` VARCHAR(512) NOT NULL DEFAULT '',",
        "    PRIMARY KEY (`sigil`)",
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
        "",
        "-- The paragon glyphs: the item each is, its branch, and the bonus it gives once `need` nodes of its branch",
        "-- are allocated within its radius",
        "DROP TABLE IF EXISTS `paragon_glyph`;",
        "CREATE TABLE `paragon_glyph` (",
        "    `id` TINYINT UNSIGNED NOT NULL,",
        "    `item` INT UNSIGNED NOT NULL,",
        "    `side` TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `need` TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `effect` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'ParagonEffect',",
        "    `value` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `value2` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `chance` FLOAT NOT NULL DEFAULT 0,",
        "    `duration` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `cooldown` INT UNSIGNED NOT NULL DEFAULT 0,",
        "    `scope` TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `name` VARCHAR(64) NOT NULL DEFAULT '',",
        "    `description` VARCHAR(512) NOT NULL DEFAULT '',",
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
        " `chance`, `duration`, `cooldown`, `free`, `required`, `cost`, `side`, `tier`, `scope`, `sigil`, `icon`,"
        " `name`, `description`) VALUES",
    ]

    rows = ["    (%d, %d, %d, %d, %d, %d, %d, %d, %g, %d, %d, %d, %d, %d, %d, %d, %d, %d, '%s', '%s', '%s')" % (
        n["id"], n["type"], n["x"], n["y"], n["effect"], n["stat"], n["value"], n["value2"],
        n["chance"], n["duration"], n["cooldown"], n["free"], n["required"], n["cost"], n["side"], n["tier"],
        n["scope"], n["sigil"],
        escape("Interface" + chr(92) + "Icons" + chr(92) + n["icon"]),
        escape(n["name"]), escape(n["description"])) for n in nodes]
    lines.append(",\n".join(rows) + ";")
    lines.append("")
    lines.append("INSERT INTO `paragon_node_link` (`node_a`, `node_b`) VALUES")
    lines.append(",\n".join("    (%d, %d)" % (a, b) for a, b in links) + ";")
    lines.append("")
    lines.append("INSERT INTO `paragon_blessing` (`sigil`, `effect`, `value`, `value2`, `chance`, `duration`,"
                 " `cooldown`, `scope`, `name`, `description`) VALUES")
    lines.append(",\n".join("    (%d, %d, %d, %d, %g, %d, %d, %d, '%s', '%s')" % (
        g["id"], g["blessing"]["effect"], g["blessing"]["value"], g["blessing"]["value2"], g["blessing"]["chance"],
        g["blessing"]["duration"], g["blessing"]["cooldown"], g["scope"], escape(g["blessing"]["name"]),
        escape(g["description"])) for g in sigils) + ";")
    lines.append("")
    lines.append("INSERT INTO `paragon_glyph` (`id`, `item`, `side`, `need`, `effect`, `value`, `value2`, `chance`,"
                 " `duration`, `cooldown`, `scope`, `name`, `description`) VALUES")
    lines.append(",\n".join("    (%d, %d, %d, %d, %d, %d, %d, %g, %d, %d, %d, '%s', '%s')" % (
        g["id"], g["item"], g["side"], g["need"], g["effect"], g["value"], g["value2"], g["chance"], g["duration"],
        g["cooldown"], g["scope"], escape(g["name"]), escape(g["description"])) for g in glyphs) + ";")
    lines.append("")

    with open(OUTPUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    write_glyph_items(glyphs)

    print("%s" % OUTPUT)
    print("  %d nodes: %d minor, %d notable, %d keystone" % (
        len(nodes), counts[MINOR], counts[NOTABLE], counts[KEYSTONE]))
    print("  %d links" % len(links))

    stamp = signature(nodes, links)
    write_lua(nodes, links, stamp, sigils, glyphs)
    print("%s" % LUA_OUTPUT)
    print("  signature %d (must match the server's on open)" % stamp)


if __name__ == "__main__":
    sys.exit(main())
