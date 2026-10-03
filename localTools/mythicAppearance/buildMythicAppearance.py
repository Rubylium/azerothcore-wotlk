"""Generates the looks of the Mythic+ generated gear and writes them as the module's world SQL.

A generated item (mod-stat-growth MythicItemGeneration.cpp, entry >= 65536) is a copy of a real item, and the client
draws it with its base item's look: a 400 piece looked exactly like the 284 piece it came from. The server shows
another look instead (mod-stat-growth MythicAppearance.cpp): what the client is told a character wears - its visible
item fields - names a real item of the same slot and armour or weapon type, picked by the generated item's item
level. The item itself, its tooltip and what inspecting it shows do not change. This script picks those real items.

Appearance tiers, in the order a player should feel them going up (TIERS, easy to reorder):
- armour: a tier set per class for the five set slots; the set's own wrist, waist and feet pieces where it has them,
  otherwise a piece of the same raid and item levels, chosen from the raid's loot; and a cloak of that raid.
- weapons: a curated model per weapon type and tier (WEAPONS), the legendaries on top where one exists.
- weapons also glow, by tier: the look of a stock enchantment (GLOWS), only the visible one.

Every row is resolved here, fallbacks included: a class with no set in a tier (a death knight before Tier 8) wears
its armour type's default class set, so the server only looks up (class, type, slot) and takes the highest tier the
item level reaches. Classes outside the table (the custom ones) use the class 0 rows.

Every appearance is checked against the world database and the client's Item.dbc and ItemDisplayInfo.dbc (the
server's copies, which mirror the client's) before anything is written: an item the client does not know renders as
nothing at all.

The world database is only read.

Usage: python localTools/mythicAppearance/buildMythicAppearance.py [--dbc-dir DIR]
"""
import argparse
import os
import struct
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DBC_DIR = os.path.join(REPO, "server", "Data", "dbc")
OUTPUT = os.path.join(REPO, "modules", "mod-stat-growth", "data", "sql", "db-world", "base",
                      "stat_growth_mythic_appearance.sql")
MYSQL = [r"C:\laragon\bin\mysql\mariadb-12.0.2-winx64\bin\mysql.exe", "--user=root", "--port=3307",
         "--host=localhost", "--protocol=TCP", "--batch", "--skip-column-names", "acore_world"]

# Classes (SharedDefines.h)
WARRIOR, PALADIN, HUNTER, ROGUE, PRIEST, DEATH_KNIGHT, SHAMAN, MAGE, WARLOCK, DRUID = 1, 2, 3, 4, 5, 6, 7, 8, 9, 11
CLASS_NAMES = {0: "any class", WARRIOR: "Warrior", PALADIN: "Paladin", HUNTER: "Hunter", ROGUE: "Rogue",
               PRIEST: "Priest", DEATH_KNIGHT: "Death Knight", SHAMAN: "Shaman", MAGE: "Mage", WARLOCK: "Warlock",
               DRUID: "Druid"}

# Item classes and subclasses
ITEM_CLASS_WEAPON, ITEM_CLASS_ARMOR = 2, 4
ARMOR_MISC, CLOTH, LEATHER, MAIL, PLATE, SHIELD = 0, 1, 2, 3, 4, 6
ARMOR_NAMES = {CLOTH: "cloth", LEATHER: "leather", MAIL: "mail", PLATE: "plate"}
AXE, AXE2, BOW, GUN, MACE, MACE2, POLEARM, SWORD, SWORD2, STAFF, FIST, DAGGER, THROWN, CROSSBOW, WAND = (
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 13, 15, 16, 18, 19)

# Inventory types. The server reads a robe as a chest, and a one-hand weapon as a main or off hand by the slot it is
# worn in (MythicAppearance.cpp); the rows use those.
HEAD, SHOULDERS, CHEST, WAIST, LEGS, FEET, WRISTS, HANDS = 1, 3, 5, 6, 7, 8, 9, 10
WEAPON_ONE_HAND, SHIELD_SLOT, RANGED, BACK, TWO_HAND, ROBE = 13, 14, 15, 16, 17, 20
MAIN_HAND, OFF_HAND, HOLDABLE, THROWN_SLOT, RANGED_RIGHT = 21, 22, 23, 25, 26
SET_SLOTS = (HEAD, SHOULDERS, CHEST, LEGS, HANDS)
EXTRA_SLOTS = (WRISTS, WAIST, FEET)
SLOT_NAMES = {HEAD: "head", SHOULDERS: "shoulders", CHEST: "chest", WAIST: "waist", LEGS: "legs", FEET: "feet",
              WRISTS: "wrists", HANDS: "hands", BACK: "back"}

# The armour each class wears at 80, and the class whose set an armour type shows when the wearer's class has none
CLASS_ARMOR = {WARRIOR: PLATE, PALADIN: PLATE, DEATH_KNIGHT: PLATE, HUNTER: MAIL, SHAMAN: MAIL, ROGUE: LEATHER,
               DRUID: LEATHER, PRIEST: CLOTH, MAGE: CLOTH, WARLOCK: CLOTH}
DEFAULT_CLASS = {CLOTH: MAGE, LEATHER: ROGUE, MAIL: HUNTER, PLATE: WARRIOR}

# When a piece has to be chosen from a raid's loot, a class takes one with its own main stat first (the look does not
# depend on it, but a warrior in a spell power belt reads wrong to anyone who knows the item)
STRENGTH, AGILITY, INTELLECT = 4, 3, 5
CLASS_STAT = {WARRIOR: STRENGTH, PALADIN: STRENGTH, DEATH_KNIGHT: STRENGTH, HUNTER: AGILITY, ROGUE: AGILITY,
              SHAMAN: INTELLECT, DRUID: INTELLECT, PRIEST: INTELLECT, MAGE: INTELLECT, WARLOCK: INTELLECT}

# Items never shown: PvP sets, placeholders, test and monster items
EXCLUDED_NAMES = ("Gladiator", "Vengeful", "Merciless", "Brutal", "Hateful", "Deadly", "Furious", "Relentless",
                  "Wrathful", "Savage", "Grand Marshal", "High Warlord", "Monster", "[PH]", "DEPRECATED", "Test",
                  "90 Epic", "UNUSED", "DEP")
ITEM_FLAG_DEPRECATED = 0x10


class Tier:
    def __init__(self, min_item_level, name, sets=None, maps=(), item_levels=(0, 0), cloaks=(), pieces=None):
        self.min_item_level = min_item_level
        self.name = name
        self.sets = sets or {}              # class -> itemset id
        self.maps = maps                    # raids whose loot gives the pieces the sets lack, and the cloak
        self.item_levels = item_levels      # the item levels of that loot, both included
        self.cloaks = cloaks                # a fixed choice of cloaks when the loot is not in the database
        # Or every piece named: class -> entries of PIECE_SLOTS (a look no item of the game has, carried by an item
        # of its own: the retail imports, localTools/retailImport)
        self.pieces = pieces or {}


# The order of a Tier's pieces
PIECE_SLOTS = (HEAD, SHOULDERS, CHEST, HANDS, LEGS, FEET, WAIST, WRISTS, BACK)


# The appearance tiers, lowest first: from 300 a step every 20 item levels (5 keys), and the last one kept from there
# on. Generated items of 285 to 299 keep their base item's look. Reorder freely: the item levels stay where they are.
TIERS = [
    Tier(300, "Tier 2 - Blackwing Lair", {
        WARRIOR: 218, PALADIN: 217, HUNTER: 215, ROGUE: 213, PRIEST: 211, SHAMAN: 216, MAGE: 210, WARLOCK: 212,
        DRUID: 214}, maps=(469, 409), item_levels=(66, 83)),
    Tier(320, "Tier 5 - Serpentshrine Cavern and Tempest Keep", {
        WARRIOR: 657, PALADIN: 629, HUNTER: 652, ROGUE: 622, PRIEST: 666, SHAMAN: 636, MAGE: 649, WARLOCK: 646,
        DRUID: 641}, maps=(548, 550), item_levels=(128, 141)),
    Tier(340, "Tier 8 - Ulduar 25", {
        WARRIOR: 830, PALADIN: 820, HUNTER: 838, ROGUE: 826, PRIEST: 833, DEATH_KNIGHT: 834, SHAMAN: 825,
        MAGE: 836, WARLOCK: 837, DRUID: 828}, maps=(603,), item_levels=(226, 239)),
    Tier(360, "Tier 6 - Black Temple, Hyjal and the Sunwell", {
        WARRIOR: 672, PALADIN: 681, HUNTER: 669, ROGUE: 668, PRIEST: 675, SHAMAN: 683, MAGE: 671, WARLOCK: 670,
        DRUID: 676}, maps=(564, 534, 580), item_levels=(141, 164)),
    Tier(380, "Tier 10 heroic - Icecrown Citadel 25", {
        WARRIOR: 895, PALADIN: 900, HUNTER: 891, ROGUE: 890, PRIEST: 886, DEATH_KNIGHT: 897, SHAMAN: 894,
        MAGE: 883, WARLOCK: 884, DRUID: 888}, maps=(631,), item_levels=(277, 284)),
    Tier(400, "Tier 3 - Naxxramas 40", {
        WARRIOR: 523, PALADIN: 528, HUNTER: 530, ROGUE: 524, PRIEST: 525, SHAMAN: 527, MAGE: 526, WARLOCK: 529,
        DRUID: 521}, cloaks=(23050, 23045, 22731, 23017, 22960, 22938, 23030)),
    # The Hollow Voice's gear (Mythic::MaxPinnacleItemLevel, the only reward this high): each class's Tomb of Sargeras
    # tier (Legion T20) in its Mythic look, imported from retail (localTools/retailImport items.json, displays
    # 70003-70011 and 70020-70100, carried by the rogue's test items and "Hollow Voice look" items no one is given)
    Tier(477, "The Hollow Voice - Tomb of Sargeras sets, Mythic", pieces={
        WARRIOR: (16102, 16103, 16105, 16106, 16107, 16108, 16109, 16116, 16117),
        PALADIN: (16118, 16119, 16120, 16121, 16122, 16123, 16124, 16125, 16126),
        DEATH_KNIGHT: (16127, 16129, 16131, 16132, 16134, 16135, 16136, 16137, 16138),
        HUNTER: (16139, 16140, 16141, 16142, 16143, 16145, 16146, 16147, 16148),
        SHAMAN: (16149, 16150, 16151, 16152, 16153, 16154, 16155, 16156, 16157),
        ROGUE: (1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027, 1162),
        DRUID: (16158, 16159, 16160, 16161, 16162, 16163, 16164, 16165, 16172),
        PRIEST: (16173, 16174, 16175, 16176, 16177, 16178, 16179, 16180, 16181),
        MAGE: (16182, 16183, 16184, 16185, 16186, 16187, 16188, 16211, 16212),
        WARLOCK: (16213, 17824, 17825, 17826, 17831, 17832, 17833, 17834, 17835)}),
]

# The custom classes (localTools/customClasses/classes.json) wear the set of a class of their armour in the tiers
# that name pieces, and the class 0 rows' looks below (the server falls back to class 0 only when a class has no row
# at all for an armour type and slot, so they are given every tier's)
CUSTOM_CLASSES = {
    10: (PALADIN, "Oathblade"),         # plate, a knight of the blade
    12: (DEATH_KNIGHT, "Pestifere"),    # plate, built on the death knight
    13: (WARLOCK, "Necromancer"),       # cloth, built on the warlock
    14: (HUNTER, "Barbarian"),          # mail, an agility fighter
}


# The glow a generated weapon shows by tier, in the enchantment slot the client draws: stock enchantments whose item
# visual glows, from a few embers to a blaze. Only the visible field changes: the weapon's real enchantment and its
# effect stay.
GLOWS = [
    (300, 803, "Fiery Weapon: red embers"),
    (320, 1894, "Icy Chill: frost blue"),
    (340, 1900, "Crusader: holy gold"),
    (360, 2673, "Mongoose: bright swirls"),
    (380, 3225, "Executioner: dark smoke"),
    (400, 3789, "Berserking: burning red"),
]

# Weapon models, one per tier (the same levels as TIERS): (weapon subclass, role) -> entries, lowest tier first.
# A dict entry picks by class (0 for the others). One-hand weapons have a main-hand and an off-hand role, so a pair
# can differ (the Warglaives). The era follows the armour tiers where it can; the legendaries take the top. The
# Hollow Voice's tier has none (None): its weapons keep Tier 3's look, the highest below it.
W, A = ITEM_CLASS_WEAPON, ITEM_CLASS_ARMOR
ONE_HAND = (MAIN_HAND, OFF_HAND)
ATIESH = {0: 22589, MAGE: 22589, WARLOCK: 22630, PRIEST: 22631, DRUID: 22632}
WEAPONS = [
    # (item class, subclass, roles, entries by tier)
    (W, AXE, ONE_HAND, [17068, 29924, 46031, 32236, 50654, 50737, None]),         # ... Havoc's Call
    (W, AXE2, (TWO_HAND,), [19353, 30316, 45165, 32348, 50709, 49623, None]),     # Devastation ... Shadowmourne
    (W, BOW, (RANGED,), [18713, 30318, 45327, 32336, 50638, 34334, None]),        # Rhok'delar ... Thori'dal
    (W, GUN, (RANGED_RIGHT,), [19368, 29949, 45870, 32325, 51845, 51834, None]),
    (W, MACE, ONE_HAND, [19335, 30317, 45612, 34335, 50734, 46017, None]),        # Cosmic Infuser ... Val'anyr
    (W, MACE2, (TWO_HAND,), [19357, 30090, 46067, 32332, 50603, 17182, None]),    # ... Sulfuras
    (W, POLEARM, (TWO_HAND,), [21635, 28774, 45533, 34183, 50727, 50735, None]),  # ... Oathbinder
    (W, SWORD, (MAIN_HAND,), [19352, 30311, 45110, 34214, 19019, 32837, None]),   # Warp Slicer, Thunderfury, Warglaives
    (W, SWORD, (OFF_HAND,), [19352, 30311, 45110, 34214, 19019, 32838, None]),
    (W, SWORD2, (TWO_HAND,), [19364, 29993, 45516, 34247, 50730, 22691, None]),   # Ashkandi ... Corrupted Ashbringer
    (W, STAFF, (TWO_HAND,), [19356, 30313, 45457, 34337, 50731, ATIESH, None]),   # Staff of Disintegration ... Atiesh
    (W, FIST, (MAIN_HAND,), [19365, 32944, 45132, 32946, 50692, 34331, None]),
    (W, FIST, (OFF_HAND,), [23242, 29948, 45494, 32945, 50710, 34346, None]),
    # Perdition's, Infinity Blade ... Kingsfall
    (W, DAGGER, ONE_HAND, [18816, 30312, 45607, 34329, 50736, 22802, None]),
    (W, THROWN, (THROWN_SLOT,), [None, 30025, 45296, 34349, 50474, None, None]),
    (W, CROSSBOW, (RANGED_RIGHT,), [19361, 28504, 45570, 32253, 51940, 50733, None]),     # ... Fal'inrush
    (W, WAND, (RANGED_RIGHT,), [19367, 29982, 45511, 34347, 50684, 22821, None]),
    (A, SHIELD, (SHIELD_SLOT,), [19349, 30314, 45587, 32375, 50729, 23043, None]),        # Phaseshift ... Face of Death
    (A, ARMOR_MISC, (HOLDABLE,), [19366, 29923, 45617, 34179, 50635, 23049, None]),       # ... Sapphiron's Left Eye
]
# The inventory types an appearance may have for each role
ROLE_TYPES = {MAIN_HAND: (WEAPON_ONE_HAND, MAIN_HAND), OFF_HAND: (WEAPON_ONE_HAND, OFF_HAND, MAIN_HAND),
              TWO_HAND: (TWO_HAND,), RANGED: (RANGED,), RANGED_RIGHT: (RANGED_RIGHT,),
              THROWN_SLOT: (THROWN_SLOT,), SHIELD_SLOT: (SHIELD_SLOT,), HOLDABLE: (HOLDABLE,)}


def query(sql):
    result = subprocess.run(MYSQL + ["-e", sql], capture_output=True, text=True, encoding="utf-8", check=True)
    return [line.split("\t") for line in result.stdout.splitlines() if line]


def read_dbc(path):
    data = open(path, "rb").read()
    magic, count, fields, size, _ = struct.unpack_from("<4s4I", data)
    if magic != b"WDBC":
        sys.exit("%s is not a DBC file" % path)
    return [struct.unpack_from("<%dI" % fields, data, 20 + index * size) for index in range(count)]


class Item:
    def __init__(self, row):
        (entry, name, item_class, subclass, inventory_type, item_level, quality, flags, bonding, allowable,
         display, item_set) = row[:12]
        self.entry = int(entry)
        self.name = name
        self.item_class = int(item_class)
        self.subclass = int(subclass)
        self.inventory_type = int(inventory_type)
        self.item_level = int(item_level)
        self.quality = int(quality)
        self.flags = int(flags)
        self.bonding = int(bonding)
        self.allowable = int(allowable)
        self.display = int(display)
        self.item_set = int(item_set)
        self.stats = {int(stat) for stat in row[12:22] if int(stat)}

    def slot(self):
        return CHEST if self.inventory_type == ROBE else self.inventory_type

    def excluded(self):
        return self.flags & ITEM_FLAG_DEPRECATED or any(word in self.name for word in EXCLUDED_NAMES)

    def fits(self, player_class):
        return self.allowable in (-1, 0) or self.allowable & (1 << (player_class - 1))


ITEM_COLUMNS = ("entry, name, class, subclass, InventoryType, ItemLevel, Quality, Flags, bonding, AllowableClass, "
                "displayid, itemset, stat_type1, stat_type2, stat_type3, stat_type4, stat_type5, stat_type6, "
                "stat_type7, stat_type8, stat_type9, stat_type10")


def load_items(where):
    return [Item(row) for row in query("SELECT %s FROM item_template WHERE %s" % (ITEM_COLUMNS, where))]


def raid_loot(maps):
    """Every item the creatures and chests of these maps can drop, all difficulties, references followed."""
    map_list = ",".join(str(map_id) for map_id in maps)
    spawned = "SELECT %s FROM creature WHERE map IN (%s)" % (creature_column(), map_list)
    difficulties = " OR ".join(
        "t.entry IN (SELECT d.difficulty_entry_%d FROM creature_template d WHERE d.entry IN (%s))" % (index, spawned)
        for index in (1, 2, 3))
    creatures = query("SELECT DISTINCT t.lootid FROM creature_template t WHERE t.lootid > 0 AND (t.entry IN (%s) OR "
                      "%s)" % (spawned, difficulties))
    chests = query("SELECT DISTINCT Data1 FROM gameobject_template WHERE type = 3 AND Data1 > 0 AND entry IN "
                   "(SELECT id FROM gameobject WHERE map IN (%s))" % map_list)
    items = set()
    references = set()

    def collect(table, entries):
        if not entries:
            return
        for item, reference in query("SELECT Item, Reference FROM %s WHERE Entry IN (%s)" % (
                table, ",".join(entries))):
            if int(reference):
                references.add(reference)
            else:
                items.add(int(item))

    collect("creature_loot_template", [row[0] for row in creatures])
    collect("gameobject_loot_template", [row[0] for row in chests])
    followed = set()
    while references - followed:
        batch = references - followed
        followed |= batch
        collect("reference_loot_template", list(batch))
    return items


def creature_column():
    columns = {row[0] for row in query("SHOW COLUMNS FROM creature")}
    return "id1" if "id1" in columns else "id"


class Checker:
    """An appearance must be a real item the client knows, with a model it can draw."""

    def __init__(self, dbc_dir):
        items = read_dbc(os.path.join(dbc_dir, "Item.dbc"))
        self.client_display = {row[0]: row[5] for row in items}
        self.client_types = {row[0]: (row[1], row[2], row[6]) for row in items}
        self.displays = {row[0] for row in read_dbc(os.path.join(dbc_dir, "ItemDisplayInfo.dbc"))}
        self.enchants = {row[0]: row[31] for row in read_dbc(os.path.join(dbc_dir, "SpellItemEnchantment.dbc"))}
        self.errors = []

    def check(self, item, item_class, subclass, slot_types):
        where = "%d %s" % (item.entry, item.name)
        if item.item_class != item_class or item.subclass != subclass:
            self.errors.append("%s is not of class %d subclass %d" % (where, item_class, subclass))
        if item.inventory_type not in slot_types:
            self.errors.append("%s has inventory type %d, not one of %s" % (where, item.inventory_type, slot_types))
        display = self.client_display.get(item.entry)
        if display is None:
            self.errors.append("%s is not in the client's Item.dbc" % where)
        elif display not in self.displays:
            self.errors.append("%s has display %d, missing from ItemDisplayInfo.dbc" % (where, display))
        elif self.client_types[item.entry][:2] != (item.item_class, item.subclass):
            self.errors.append("%s differs between Item.dbc and item_template" % where)

    def check_glow(self, enchant):
        if not self.enchants.get(enchant):
            self.errors.append("enchantment %d has no item visual" % enchant)


def pick_extra(candidates, subclass, slot, player_class):
    """A piece from the raid's loot for a slot the set does not have"""
    stat = CLASS_STAT.get(player_class)
    pool = [item for item in candidates if item.subclass == subclass and item.slot() == slot and
            item.fits(player_class)]
    pool.sort(key=lambda item: (stat not in item.stats, -item.item_level, item.entry))
    return pool[0] if pool else None


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dbc-dir", default=DBC_DIR, help="where Item.dbc and ItemDisplayInfo.dbc are")
    args = parser.parse_args()

    checker = Checker(args.dbc_dir)
    rows = {}           # (item class, subclass, slot, player class, min item level) -> (entry, comment)
    items = {}

    def remember(found):
        for item in found:
            items[item.entry] = item
        return found

    def add(item_class, subclass, slot, player_class, tier, item, types, note):
        checker.check(item, item_class, subclass, types)
        rows[(item_class, subclass, slot, player_class, tier.min_item_level if isinstance(tier, Tier) else tier)] = (
            item.entry, "%s: %s (%s)" % (note, item.name, CLASS_NAMES.get(player_class, player_class)))

    # --- Armour ---------------------------------------------------------------------------------------------------
    set_ids = sorted({set_id for tier in TIERS for set_id in tier.sets.values()})
    set_items = remember(load_items("itemset IN (%s)" % ",".join(map(str, set_ids))))
    piece_entries = sorted({entry for tier in TIERS for entries in tier.pieces.values() for entry in entries})
    if piece_entries:
        remember(load_items("entry IN (%s)" % ",".join(map(str, piece_entries))))
        unknown = [entry for entry in piece_entries if entry not in items]
        if unknown:
            sys.exit("pieces missing from item_template (the retail import's SQL not applied yet?): %s" % unknown)
    armor_classes = [0] + sorted(CLASS_ARMOR)

    for tier in TIERS:
        if tier.pieces:
            for player_class in armor_classes:
                subclasses = [CLASS_ARMOR[player_class]] if player_class else sorted(DEFAULT_CLASS)
                for subclass in subclasses:
                    owner = player_class if player_class in tier.pieces else DEFAULT_CLASS[subclass]
                    for slot, entry in zip(PIECE_SLOTS, tier.pieces[owner]):
                        item = items[entry]
                        if slot == BACK:
                            add(A, CLOTH, BACK, player_class, tier, item, (BACK,), tier.name)
                        else:
                            add(A, subclass, slot, player_class, tier, item,
                                (slot, ROBE) if slot == CHEST else (slot,), tier.name)
            continue
        loot = []
        if tier.maps:
            dropped = raid_loot(tier.maps)
            loot = [item for item in remember(load_items(
                "class = 4 AND Quality = 4 AND bonding = 1 AND ItemLevel BETWEEN %d AND %d AND entry IN (%s)" % (
                    tier.item_levels[0], tier.item_levels[1], ",".join(map(str, sorted(dropped))) or "0")))
                if not item.excluded()]
        cloaks = [item for item in loot if item.subclass == CLOTH and item.slot() == BACK]
        if tier.cloaks:
            fixed = remember(load_items("entry IN (%s)" % ",".join(map(str, tier.cloaks))))
            cloaks = [item for entry in tier.cloaks for item in fixed if item.entry == entry]

        for player_class in armor_classes:
            # The wearer's class rows are its own armour type; class 0 covers every type with its default class
            subclasses = [CLASS_ARMOR[player_class]] if player_class else sorted(DEFAULT_CLASS)
            for subclass in subclasses:
                owner = player_class if player_class in tier.sets else DEFAULT_CLASS[subclass]
                if not player_class:
                    owner = DEFAULT_CLASS[subclass]
                pieces = {}
                for item in set_items:
                    if item.item_set == tier.sets[owner] and item.subclass == subclass and \
                            item.slot() in SET_SLOTS + EXTRA_SLOTS:
                        best = pieces.get(item.slot())
                        # The top item level of the set: heroic colours, the 25-man look
                        if not best or (item.item_level, -item.entry) > (best.item_level, -best.entry):
                            pieces[item.slot()] = item
                missing = [slot for slot in SET_SLOTS if slot not in pieces]
                if missing:
                    sys.exit("set %d (%s) has no %s piece of %s" % (tier.sets[owner], tier.name, missing,
                                                                     ARMOR_NAMES[subclass]))
                for slot in EXTRA_SLOTS:
                    if slot not in pieces:
                        extra = pick_extra(loot, subclass, slot, owner)
                        if extra:
                            pieces[slot] = extra
                        else:
                            print("  %s: no %s %s piece, the tier below stays" % (
                                tier.name, ARMOR_NAMES[subclass], SLOT_NAMES[slot]))
                for slot, item in sorted(pieces.items()):
                    add(A, subclass, slot, player_class, tier, item, (slot, ROBE) if slot == CHEST else (slot,),
                        tier.name)

            # Cloaks are cloth whoever wears them: a row per class, its own main stat first
            cloak = pick_extra(cloaks, CLOTH, BACK, player_class or MAGE) if not tier.cloaks else None
            if tier.cloaks:
                stat = CLASS_STAT.get(player_class or MAGE)
                ranked = sorted(cloaks, key=lambda item: (stat not in item.stats, tier.cloaks.index(item.entry)))
                cloak = ranked[0] if ranked else None
            if cloak:
                add(A, CLOTH, BACK, player_class, tier, cloak, (BACK,), tier.name)
            else:
                print("  %s: no cloak" % tier.name)

    # The custom classes: class 0's rows of their armour and cloaks, every tier, and their model class's pieces where
    # a tier names them (their other armour types fall back to class 0 on their own)
    for custom, (model, label) in CUSTOM_CLASSES.items():
        for (item_class, subclass, slot, player_class, level), (entry, note) in list(rows.items()):
            if item_class == A and player_class == 0 and (subclass == CLASS_ARMOR[model] or slot == BACK):
                rows[(item_class, subclass, slot, custom, level)] = (entry, note.replace("(any class)",
                                                                                          "(%s)" % label))
        for tier in TIERS:
            if model not in tier.pieces:
                continue
            for slot, entry in zip(PIECE_SLOTS, tier.pieces[model]):
                subclass = CLOTH if slot == BACK else CLASS_ARMOR[model]
                rows[(A, subclass, slot, custom, tier.min_item_level)] = (
                    entry, "%s: %s (%s, the %s's set)" % (tier.name, items[entry].name, label, CLASS_NAMES[model]))

    # --- Weapons, shields and off-hands ---------------------------------------------------------------------------
    weapon_entries = sorted({entry for _, _, _, entries in WEAPONS for choice in entries if choice
                             for entry in (choice.values() if isinstance(choice, dict) else [choice])})
    remember(load_items("entry IN (%s)" % ",".join(map(str, weapon_entries))))
    if len(WEAPONS[0][3]) != len(TIERS) or any(len(entries) != len(TIERS) for _, _, _, entries in WEAPONS):
        sys.exit("every weapon list needs one entry per tier (None for none)")
    for item_class, subclass, roles, entries in WEAPONS:
        # A class named anywhere in a line gets the whole line, the others falling back to class 0
        classes = sorted({0} | {player_class for choice in entries if isinstance(choice, dict) for player_class in
                                choice})
        for role in roles:
            for player_class in classes:
                for tier, choice in zip(TIERS, entries):
                    if choice is None:
                        continue
                    entry = choice.get(player_class, choice[0]) if isinstance(choice, dict) else choice
                    if entry not in items:
                        sys.exit("weapon %d is not in item_template" % entry)
                    add(item_class, subclass, role, player_class, tier, items[entry], ROLE_TYPES[role],
                        "Weapon tier %d" % tier.min_item_level)

    for _, enchant, _ in GLOWS:
        checker.check_glow(enchant)
    if [level for level, _, _ in GLOWS] != [tier.min_item_level for tier in TIERS]:
        print("  note: the glows do not start where the tiers do")

    if checker.errors:
        sys.exit("Not written:\n  " + "\n  ".join(checker.errors))

    write(rows)
    print("%d appearance rows, %d glows" % (len(rows), len(GLOWS)))
    print(OUTPUT)


def sql_string(text):
    return "'" + text.replace("\\", "\\\\").replace("'", "''") + "'"


def write(rows):
    tiers = ", ".join("%d %s" % (tier.min_item_level, tier.name) for tier in TIERS)
    lines = [
        "-- Generated by localTools/mythicAppearance/buildMythicAppearance.py -- do not edit by hand.",
        "--",
        "-- The looks of the Mythic+ generated gear (mod-stat-growth MythicAppearance.cpp): a generated item shows a",
        "-- real item of its slot and armour or weapon type, picked by its item level, and a generated weapon glows.",
        "-- Tiers: %s." % tiers,
        "",
        "-- Generated data with nothing of the players in it: dropped and rebuilt whole.",
        "DROP TABLE IF EXISTS `mythic_appearance_tier`;",
        "CREATE TABLE `mythic_appearance_tier` (",
        "    `item_class` TINYINT UNSIGNED NOT NULL COMMENT '2 weapon, 4 armour',",
        "    `item_subclass` TINYINT UNSIGNED NOT NULL,",
        "    `inventory_type` TINYINT UNSIGNED NOT NULL COMMENT 'robes read as chests (5), one-hand weapons as main "
        "(21) or off hand (22) by where they are worn',",
        "    `player_class` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'the wearer, 0 for any other',",
        "    `min_item_level` SMALLINT UNSIGNED NOT NULL COMMENT 'from this generated item level up',",
        "    `appearance_entry` INT UNSIGNED NOT NULL COMMENT 'the real item shown, in the client Item.dbc',",
        "    `comment` VARCHAR(255) NOT NULL DEFAULT '',",
        "    PRIMARY KEY (`item_class`, `item_subclass`, `inventory_type`, `player_class`, `min_item_level`)",
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
        "",
        "DROP TABLE IF EXISTS `mythic_appearance_glow`;",
        "CREATE TABLE `mythic_appearance_glow` (",
        "    `min_item_level` SMALLINT UNSIGNED NOT NULL COMMENT 'from this generated item level up',",
        "    `enchantment` SMALLINT UNSIGNED NOT NULL COMMENT 'SpellItemEnchantment shown, not applied',",
        "    `comment` VARCHAR(255) NOT NULL DEFAULT '',",
        "    PRIMARY KEY (`min_item_level`)",
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
        "",
        "DELETE FROM `mythic_appearance_tier`;",
        "INSERT INTO `mythic_appearance_tier` (`item_class`, `item_subclass`, `inventory_type`, `player_class`, "
        "`min_item_level`, `appearance_entry`, `comment`) VALUES",
    ]
    values = ["(%d, %d, %d, %d, %d, %d, %s)" % (key + (entry, sql_string(comment)))
              for key, (entry, comment) in sorted(rows.items())]
    lines.append(",\n".join(values) + ";")
    lines.append("")
    lines.append("DELETE FROM `mythic_appearance_glow`;")
    lines.append("INSERT INTO `mythic_appearance_glow` (`min_item_level`, `enchantment`, `comment`) VALUES")
    lines.append(",\n".join("(%d, %d, %s)" % (level, enchant, sql_string(comment))
                            for level, enchant, comment in GLOWS) + ";")
    with open(OUTPUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
