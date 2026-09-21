"""Generates the paragon board and writes it as the module's world SQL.

The board is data, not code: the server reads `paragon_node` and `paragon_node_link` and serves whatever is in
them, so reshaping the tree is a rerun of this script rather than a rebuild. Node positions are in canvas units
with the start node at the origin; the client scales them to the window.

Shape: a hub with six branches radiating out, one stat each, in eight rings. Minor nodes carry the branch out,
notables sit at the junctions, and each branch ends in a single keystone. Adjacent branches are stitched
together at rings two and three, so a build can cross between neighbours instead of only running outward.

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

    return nodes, sorted(links)


def signature(nodes, links):
    """Must match LoadParagonBoard in ParagonSystem.cpp, wraparound included."""
    total = 0
    for node in nodes:
        total += node["id"] * 31 + node["value"] * 7 + node["stat"] + node["effect"] * 3
    for a, b in links:
        total += a * 13 + b * 17
    return total % (2 ** 32)


def write_lua(nodes, links, stamp):
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
        "    nodes = {",
    ]
    for node in nodes:
        lines.append(
            "        [%d] = { type = %d, x = %d, y = %d, effect = %d, stat = %d, value = %d,"
            " free = %s, icon = %s, name = %s, description = %s }," % (
                node["id"], node["type"], node["x"], node["y"], node["effect"], node["stat"],
                node["value"], "true" if node["free"] else "false",
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
        "-- The paragon board: a hub and six stat branches of %d rings each, neighbouring branches stitched"
        % len(RINGS),
        "-- together partway out. %d nodes (%d minor, %d notable, %d keystone) against a 50 point cap, so most"
        % (len(nodes), counts[MINOR], counts[NOTABLE], counts[KEYSTONE]),
        "-- of the board stays permanently out of reach and what to give up is the whole choice. A keystone",
        "-- sits %d points from the hub." % len(RINGS),
        "",
        "-- Schema lives here too, so this file never depends on another one having run first.",
        "CREATE TABLE IF NOT EXISTS `paragon_node` (",
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
        " `chance`, `duration`, `cooldown`, `free`, `icon`, `name`, `description`) VALUES",
    ]

    rows = ["    (%d, %d, %d, %d, %d, %d, %d, %d, %g, %d, %d, %d, '%s', '%s', '%s')" % (
        n["id"], n["type"], n["x"], n["y"], n["effect"], n["stat"], n["value"], n["value2"],
        n["chance"], n["duration"], n["cooldown"], n["free"],
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
