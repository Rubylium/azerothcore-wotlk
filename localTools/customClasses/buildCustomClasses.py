"""Generates everything a custom playable class needs, from localTools/customClasses/classes.json.

Client data (written to clientPatcher/interface/DBFilesClient, packed into the interface patch):
  ChrClasses.dbc          the class itself: name, power type, class token
  CharBaseInfo.dbc        which races may pick it
  SkillRaceClassInfo.dbc  its skills: weapons, armor, defense, languages (copied from the template class)
  CharStartOutfit.dbc     the gear new characters start with (copied from the template class)
  TalentTab.dbc           its talent trees: its own when `talentTabs` declares them, else the template's
  Talent.dbc              the grid of its own trees (no strings, so the same bytes go to the server)

Server data:
  server/Data/dbc/*.dbc                                     the same four files for the server
  modules/mod-custom-classes/data/sql/db-world/base/...     custom_class, playercreateinfo, player_class_stats,
                                                            playercreateinfo_skills and a class trainer row
  clientPatcher/interface/Interface/FrameXML/CustomClasses.lua   class list for the client interface

Each file is rebuilt from an untouched backup, so running it again after a change is safe.

Run localTools/patchSinisterStrike.ps1 first: the spells a talent tree learns and the icons a tab and a
spellbook tab point at are authored there, and this script checks that they exist.

Usage: python buildCustomClasses.py [--client <WotLK client path>]
"""
import argparse
import json
import os
import struct
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFINITIONS = os.path.join(REPO, 'localTools', 'customClasses', 'classes.json')
SERVER_DBC = os.path.join(REPO, 'server', 'Data', 'dbc')
CLIENT_DBC_OUT = os.path.join(REPO, 'clientPatcher', 'interface', 'DBFilesClient')
CLIENT_DBC_CACHE = os.path.join(REPO, 'clientPatcher', '.dbccache')
# The same class list is needed on the login screen (class buttons) and in game (colors, names for addons)
LUA_OUT = [os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', folder, 'CustomClasses.lua')
           for folder in ('FrameXML', 'GlueXML')]
SQL_OUT = os.path.join(REPO, 'modules', 'mod-custom-classes', 'data', 'sql', 'db-world', 'base',
                       'custom_classes_generated.sql')
DBC_NAMES = ['ChrClasses.dbc', 'CharBaseInfo.dbc', 'SkillRaceClassInfo.dbc', 'CharStartOutfit.dbc']
REFERENCE_DBC_NAMES = ['Item.dbc']
# Per-class stat tables the client reads by class index, with the rows each class owns
GT_TABLES = {'gtChanceToMeleeCrit.dbc': 100, 'gtChanceToMeleeCritBase.dbc': 1, 'gtChanceToSpellCrit.dbc': 100,
             'gtChanceToSpellCritBase.dbc': 1, 'gtOCTRegenHP.dbc': 100, 'gtOCTRegenMP.dbc': 100,
             'gtRegenHPPerSpt.dbc': 100, 'gtRegenMPPerSpt.dbc': 100, 'gtOCTClassCombatRatingScalar.dbc': 32}
DBC_NAMES += list(GT_TABLES) + ['TalentTab.dbc', 'SkillLine.dbc']
# TalentTab.dbc (3.3.5, 24 fields): one row per talent tree
TALENTTAB_FIELDS = 24
TALENTTAB_NAME = 1               # 16 locales, then a flags field
TALENTTAB_ICON = 18              # SpellIcon.dbc id: the icon on the tab button
TALENTTAB_RACEMASK = 19
TALENTTAB_CLASSMASK = 20
TALENTTAB_PETMASK = 21
TALENTTAB_TABPAGE = 22           # 0-2: the tree's slot in the talent frame
TALENTTAB_BACKGROUND = 23        # Interface\TalentFrame\<name>-{Top,Bottom}{Left,Right}
RACEMASK_ALL = 2047
# Talent.dbc (3.3.5, 23 fields): the grid of a tree. It holds no strings - the talent frame reads the names
# and descriptions from Spell.dbc - so the client and the server read the very same bytes.
TALENT_FIELDS = 23
TALENT_TAB = 1
TALENT_TIER = 2                  # row, 0-based
TALENT_COLUMN = 3                # 0-3
TALENT_RANKS = 4                 # rank spell ids; the core reads the first MAX_TALENT_RANK of them
TALENT_MAX_RANKS = 5
TALENT_PREREQ_TALENT = 13
TALENT_PREREQ_RANK = 16          # 0-based: rank 1 of the prerequisite is 0
TALENT_ADD_TO_SPELLBOOK = 19     # 1: learning the talent learns its rank spell as a castable ability
SPELL_ATTRIBUTES = 4             # Spell.dbc
SPELL_ATTR0_PASSIVE = 0x40
TALENT_COLUMNS = 4
# SkillLine.dbc (3.3.5, 56 fields): the spellbook tabs a class carries
SKILL_CATEGORY_CLASS = 7
SKILL_CLASS_TEMPLATE = 26        # "Arms": a plain class skill line to copy the flags and tier from
SKILLLINE_NAME = 3               # 16 locales
SKILLLINE_DESCRIPTION = 20       # 16 locales
SKILLLINE_ICON = 37              # SpellIcon.dbc id: the icon on the spellbook tab
# Class skill lines every class keeps, whatever its own spellbook tab is
SKILLS_ALWAYS_KEPT = {769, 777, 778}
BACKUP_SUFFIX = '.before-custom-classes'
CUSTOM_SLOTS = (10, 12, 13, 14, 15)  # class ids Blizzard leaves free

# ChrClasses.dbc field layout (3.3.5, 60 fields)
CHRCLASSES_FIELDS = 60
CHRCLASSES_POWER = 2
CHRCLASSES_NAME_GROUPS = (4, 21, 38)  # name, female name, male name: 16 locales each
CHRCLASSES_TOKEN = 55
CHRCLASSES_SPELL_FAMILY = 56
CHRCLASSES_CINEMATIC = 58
CHRCLASSES_EXPANSION = 59
# Item.dbc (3.3.5, 8 fields): enough client item data to dress a preview without granting the items.
ITEM_DISPLAY_INFO = 5
ITEM_INVENTORY_TYPE = 6
# CharStartOutfit.dbc packs race, class, gender and outfit into one physical field. Its raw records therefore
# store 24 item ids at 2-25, followed by their display ids at 26-49 and inventory types at 50-73.
START_OUTFIT_ITEM_COUNT = 24
START_OUTFIT_ITEMS = 2
START_OUTFIT_DISPLAY = 26
START_OUTFIT_INVENTORY_TYPE = 50

# Item.dbc inventory types that fight over the same equipment slot: a start outfit item replaces a copied one of
# the same group (a two-hander replaces the copied sword and shield). 0 is not equipment and never replaced.
SLOT_GROUPS = {5: 'chest', 20: 'chest', 13: 'weapon', 14: 'weapon', 17: 'weapon', 21: 'weapon', 22: 'weapon',
               23: 'weapon', 15: 'ranged', 25: 'ranged', 26: 'ranged'}


def slot_group(inventory_type):
    return SLOT_GROUPS.get(inventory_type, inventory_type)


class Dbc:
    """A DBC file as records plus its string block, with helpers to append rows and strings."""

    def __init__(self, path):
        data = open(path, 'rb').read()
        assert data[:4] == b'WDBC', f'{path} is not a DBC'
        self.count, self.fields, self.record_size, string_size = struct.unpack_from('<4I', data, 4)
        start = 20
        self.records = [bytearray(data[start + i * self.record_size:start + (i + 1) * self.record_size])
                        for i in range(self.count)]
        self.strings = bytearray(data[start + self.count * self.record_size:])
        assert len(self.strings) == string_size, f'{path}: string block size mismatch'

    def field(self, record, index):
        return struct.unpack_from('<I', record, index * 4)[0]

    def set_field(self, record, index, value):
        struct.pack_into('<i', record, index * 4, int(value))

    def add_string(self, value):
        offset = len(self.strings)
        self.strings += value.encode('utf8') + b'\0'
        return offset

    def max_id(self):
        return max((self.field(record, 0) for record in self.records), default=0)

    def write(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as file:
            file.write(b'WDBC' + struct.pack('<4I', len(self.records), self.fields, self.record_size,
                                             len(self.strings)))
            for record in self.records:
                file.write(record)
            file.write(self.strings)


def backup(path):
    """Returns the untouched copy of a DBC, creating it on first run."""
    saved = path + BACKUP_SUFFIX
    if not os.path.exists(saved):
        os.makedirs(os.path.dirname(saved), exist_ok=True)
        with open(path, 'rb') as source, open(saved, 'wb') as target:
            target.write(source.read())
    return saved


def ensure_client_dbc(client_path):
    required = DBC_NAMES + REFERENCE_DBC_NAMES
    missing = [name for name in required if not os.path.exists(os.path.join(CLIENT_DBC_CACHE, name))]
    if not missing:
        return
    builder = os.path.join(REPO, 'localTools', 'mpq-builder')
    subprocess.run(['node', 'extractClientDbc.js', CLIENT_DBC_CACHE, *required, '--client', client_path],
                   cwd=builder, check=True)


def add_class_row(dbc, definition):
    """Clones the template class row and turns it into the custom class."""
    template = next((record for record in dbc.records
                     if dbc.field(record, 0) == definition['templateClass']), None)
    assert template is not None, f"template class {definition['templateClass']} not found in ChrClasses"
    assert dbc.fields == CHRCLASSES_FIELDS, 'unexpected ChrClasses layout'
    assert all(dbc.field(record, 0) != definition['id'] for record in dbc.records), 'class id already present'

    record = bytearray(template)
    dbc.set_field(record, 0, definition['id'])
    dbc.set_field(record, CHRCLASSES_POWER, definition['powerType'])
    name = dbc.add_string(definition['name'])
    token = dbc.add_string(definition['token'])
    for group in CHRCLASSES_NAME_GROUPS:
        for locale in range(16):
            dbc.set_field(record, group + locale, name)
    dbc.set_field(record, CHRCLASSES_TOKEN, token)
    dbc.set_field(record, CHRCLASSES_SPELL_FAMILY, definition.get('spellFamily', 0))
    dbc.set_field(record, CHRCLASSES_CINEMATIC, 0)
    dbc.set_field(record, CHRCLASSES_EXPANSION, 0)
    dbc.records.append(record)


def add_race_pairs(dbc, definition):
    existing = {(record[0], record[1]) for record in dbc.records}
    for race in definition['races']:
        if (race, definition['id']) not in existing:
            dbc.records.append(bytearray([race, definition['id']]))


def add_skills(dbc, definition):
    """Copies the template class's skill rows (weapons, armor, defense, languages) to the new class.

    A class with its own class skill (`classSkill`) does not take the template's: those are the template's
    spellbook tabs - a Pestiféré must not carry Blood, Frost and Unholy around.
    """
    template_bit = 1 << (definition['templateClass'] - 1)
    class_bit = 1 << (definition['id'] - 1)
    own_skill = definition.get('classSkill')
    dropped = definition.get('_templateClassSkills', set()) if own_skill else set()
    next_id = dbc.max_id() + 1
    for record in list(dbc.records):
        if not dbc.field(record, 3) & template_bit or dbc.field(record, 1) in dropped:
            continue
        row = bytearray(record)
        dbc.set_field(row, 0, next_id)
        dbc.set_field(row, 3, class_bit)
        dbc.records.append(row)
        next_id += 1

    # Some custom classes use the template's combat formulas but a different armor
    # progression. These rows must exist before inventory loads, not only at login.
    for skill_id in definition.get('extraSkills', []):
        if any(dbc.field(record, 1) == skill_id and dbc.field(record, 3) & class_bit
               for record in dbc.records):
            continue
        source = next((record for record in dbc.records
                       if dbc.field(record, 1) == skill_id and dbc.field(record, 3) & (1 << 1)), None)
        assert source, f'no Paladin skill row for {skill_id}'
        row = bytearray(source)
        dbc.set_field(row, 0, next_id)
        dbc.set_field(row, 3, class_bit)
        dbc.records.append(row)
        next_id += 1

    if not own_skill:
        return

    # The class's own skill line, copied from one of the template's so the flags and tier stay valid
    source = next((record for record in dbc.records
                   if dbc.field(record, 3) & template_bit and dbc.field(record, 1) in dropped), None)
    source = source or next(record for record in dbc.records if dbc.field(record, 3) & template_bit)
    row = bytearray(source)
    dbc.set_field(row, 0, next_id)
    dbc.set_field(row, 1, own_skill['id'])
    dbc.set_field(row, 2, -1)
    dbc.set_field(row, 3, class_bit)
    dbc.records.append(row)


def find_spell_icon(icon_name):
    """SpellIcon.dbc id of a custom icon, by file name (the rows come from patchSinisterStrike.ps1)."""
    dbc = Dbc(os.path.join(SERVER_DBC, 'SpellIcon.dbc'))
    # Icon paths are "Interface<sep>Icons<sep><name>", with the separator Windows uses
    wanted = (chr(92) + icon_name).lower().encode('utf8')
    terminator = bytes([0])
    for record in dbc.records:
        offset = dbc.field(record, 1)
        if not offset or offset >= len(dbc.strings):
            continue
        path = dbc.strings[offset:dbc.strings.index(terminator, offset)]
        if path.lower().endswith(wanted):
            return dbc.field(record, 0)

    return None


def read_class_skill_lines():
    """Class skill line ids (the spellbook tabs), minus the ones every class needs."""
    dbc = Dbc(os.path.join(CLIENT_DBC_CACHE, 'SkillLine.dbc'))
    return {dbc.field(record, 0) for record in dbc.records
            if dbc.field(record, 1) == SKILL_CATEGORY_CLASS} - SKILLS_ALWAYS_KEPT


def add_class_skill_line(dbc, definition):
    """Adds the class's own skill line, which is the name of its spellbook tab."""
    own_skill = definition.get('classSkill')
    if not own_skill:
        return

    assert all(dbc.field(record, 0) != own_skill['id'] for record in dbc.records), 'skill line id already used'
    source = next(record for record in dbc.records
                  if dbc.field(record, 0) == own_skill.get('copyFrom', SKILL_CLASS_TEMPLATE))
    row = bytearray(source)
    dbc.set_field(row, 0, own_skill['id'])
    dbc.set_field(row, 1, SKILL_CATEGORY_CLASS)
    name = dbc.add_string(own_skill.get('name', definition['name']))
    for locale in range(16):
        dbc.set_field(row, SKILLLINE_NAME + locale, name)
        dbc.set_field(row, SKILLLINE_DESCRIPTION + locale, 0)
    if own_skill.get('icon'):
        icon = find_spell_icon(own_skill['icon'])
        assert icon, f"spellbook tab icon {own_skill['icon']} is not in SpellIcon.dbc"
        dbc.set_field(row, SKILLLINE_ICON, icon)
    dbc.records.append(row)


def start_class(definition):
    """The class a new character's starting point and starting gear are copied from.

    It is the template class unless the class says otherwise: a class built on the Death Knight for its
    combat rules but created at level 1 must not start in Ebon Hold wearing its level 55 set.
    """
    return definition.get('startClass', definition['templateClass'])


def add_start_outfit(dbc, definition):
    """Copies real starter gear while optionally giving the creation model a separate visual outfit.

    `startOutfit` items are added to the copied gear, and replace any copied item of the same slot, so a class
    that starts above level 1 is not left in its template's level-1 gear.
    """
    preview_items = definition.get('previewOutfit', [])
    start_items = definition.get('startOutfit', [])
    by_id = {}
    items = None
    if preview_items or start_items:
        items, by_id = client_items()
    for item_id in start_items:
        assert item_id in by_id, f'start outfit item {item_id} is not in Item.dbc'
    preview = preview_displays(preview_items)

    source_records = list(dbc.records)
    exact = {}
    same_race = {}
    for record in source_records:
        key = dbc.field(record, 1)
        race, class_id, gender = key & 0xFF, (key >> 8) & 0xFF, (key >> 16) & 0xFF
        same_race.setdefault((race, gender), record)
        if class_id == start_class(definition):
            exact[(race, gender)] = record

    next_id = dbc.max_id() + 1
    for race in definition['races']:
        for gender in (0, 1):
            record = exact.get((race, gender), same_race.get((race, gender)))
            assert record is not None, f'no starting outfit for race {race}, gender {gender}'
            row = bytearray(record)
            dbc.set_field(row, 0, next_id)
            dbc.set_field(row, 1, race | (definition['id'] << 8) | (gender << 16))
            if start_items:
                replaced = {slot_group(items.field(by_id[item_id], ITEM_INVENTORY_TYPE)) for item_id in start_items}
                copied = [dbc.field(row, START_OUTFIT_ITEMS + index) for index in range(START_OUTFIT_ITEM_COUNT)]
                # An empty slot holds 0 or -1 (read unsigned here)
                kept = [item_id for item_id in copied if item_id not in (0, 0xFFFFFFFF) and not (
                    item_id in by_id and items.field(by_id[item_id], ITEM_INVENTORY_TYPE)
                    and slot_group(items.field(by_id[item_id], ITEM_INVENTORY_TYPE)) in replaced)]
                outfit = start_items + kept
                assert len(outfit) <= START_OUTFIT_ITEM_COUNT, 'start outfit has too many items'
                for index in range(START_OUTFIT_ITEM_COUNT):
                    item_id = outfit[index] if index < len(outfit) else 0
                    item = by_id.get(item_id)
                    dbc.set_field(row, START_OUTFIT_ITEMS + index, item_id if item_id else -1)
                    dbc.set_field(row, START_OUTFIT_DISPLAY + index,
                                  items.field(item, ITEM_DISPLAY_INFO) if item else -1)
                    dbc.set_field(row, START_OUTFIT_INVENTORY_TYPE + index,
                                  items.field(item, ITEM_INVENTORY_TYPE) if item else -1)
            if preview:
                dress(dbc, row, preview)
            dbc.records.append(row)
            next_id += 1


_client_items = None


def client_items():
    """The client's Item.dbc, and its rows by item id"""
    global _client_items
    if _client_items is None:
        items = Dbc(os.path.join(CLIENT_DBC_CACHE, 'Item.dbc'))
        _client_items = items, {items.field(record, 0): record for record in items.records}
    return _client_items


def preview_displays(item_ids):
    """(display id, inventory type) of each item, to dress a creation model with"""
    if not item_ids:
        return []
    assert len(item_ids) <= START_OUTFIT_ITEM_COUNT, 'a preview outfit has too many items'
    items, by_id = client_items()
    preview = []
    for entry in item_ids:
        # [item id, inventory type] shows the item in another slot than its own: the same off-hand sword in the
        # main hand (21) makes a matching pair, which two copies of a one-hander cannot (the second is not shown)
        item_id, slot = (entry[0], entry[1]) if isinstance(entry, list) else (entry, None)
        assert item_id in by_id, f'preview outfit item {item_id} is not in Item.dbc'
        item = by_id[item_id]
        display_id = items.field(item, ITEM_DISPLAY_INFO)
        inventory_type = slot or items.field(item, ITEM_INVENTORY_TYPE)
        assert display_id and inventory_type, f'preview outfit item {item_id} cannot be displayed'
        preview.append((display_id, inventory_type))
    return preview


def dress(dbc, row, preview):
    """Replaces what a start outfit row shows (never the items it gives, fields 2-25) with the preview"""
    for index in range(START_OUTFIT_ITEM_COUNT):
        dbc.set_field(row, START_OUTFIT_DISPLAY + index, 0)
        dbc.set_field(row, START_OUTFIT_INVENTORY_TYPE + index, 0)
    for index, (display_id, inventory_type) in enumerate(preview):
        dbc.set_field(row, START_OUTFIT_DISPLAY + index, display_id)
        dbc.set_field(row, START_OUTFIT_INVENTORY_TYPE + index, inventory_type)


def dress_stock_previews(outfits):
    """CharStartOutfit.dbc pass for the stock classes: stockPreviewOutfits (class id -> item ids) dresses every
    race and gender of that class on the creation screen. Only the display fields change, so a new character
    still starts in its real gear, and the server (which reads only the item ids) is unaffected."""
    previews = {int(class_id): preview_displays(item_ids) for class_id, item_ids in outfits.items()
                if not class_id.startswith('_')}

    def finish(dbc):
        for index, record in enumerate(dbc.records):
            class_id = (dbc.field(record, 1) >> 8) & 0xFF
            if class_id in previews:
                row = bytearray(record)
                dress(dbc, row, previews[class_id])
                dbc.records[index] = row
    return finish


def talent_tabs(definition):
    """The trees a class brings of its own: `talentTabs`, or the single `talentTab` of an older definition."""
    if definition.get('talentTabs'):
        return definition['talentTabs']
    return [definition['talentTab']] if definition.get('talentTab') else []


def add_talent_tab(dbc, definition):
    """Gives the class its talent trees: its own tabs when `talentTabs` declares them, the template's otherwise.

    A class that brings its own tree must not also be added to the template's, which is what made the
    Pestiféré show the Death Knight's Blood, Frost and Unholy.
    """
    class_bit = 1 << (definition['id'] - 1)
    tabs = talent_tabs(definition)
    if not tabs:
        template_bit = 1 << (definition['templateClass'] - 1)
        for record in dbc.records:
            mask = dbc.field(record, TALENTTAB_CLASSMASK)
            if mask & template_bit:
                dbc.set_field(record, TALENTTAB_CLASSMASK, mask | class_bit)
        return

    assert dbc.fields == TALENTTAB_FIELDS, 'unexpected TalentTab layout'
    # The client's inspect code keeps a slot per tree and resizes those slots (corrupting them) when a class with
    # another count is inspected after a stock three-tree class: a class brings exactly three, empty ones included
    assert len(tabs) == 3, f"class {definition['id']} declares {len(tabs)} talent trees, the client needs exactly 3"
    pages = [own.get('tabPage', 0) for own in tabs]
    assert len(set(pages)) == len(pages), 'two talent tabs share a tab page'
    for own in tabs:
        assert all(dbc.field(record, 0) != own['id'] for record in dbc.records), 'talent tab id already used'
        icon = find_spell_icon(own['icon'])
        assert icon, f"talent tab icon {own['icon']} is not in SpellIcon.dbc"
        row = bytearray(dbc.records[0])
        dbc.set_field(row, 0, own['id'])
        name = dbc.add_string(own['name'])
        for locale in range(16):
            dbc.set_field(row, TALENTTAB_NAME + locale, name)
        dbc.set_field(row, TALENTTAB_NAME + 16, 0)
        dbc.set_field(row, TALENTTAB_ICON, icon)
        dbc.set_field(row, TALENTTAB_RACEMASK, RACEMASK_ALL)
        dbc.set_field(row, TALENTTAB_CLASSMASK, class_bit)
        dbc.set_field(row, TALENTTAB_PETMASK, 0)
        dbc.set_field(row, TALENTTAB_TABPAGE, own.get('tabPage', 0))
        dbc.set_field(row, TALENTTAB_BACKGROUND, dbc.add_string(own['background']))
        dbc.records.append(row)


def read_spell_ids():
    """Every spell id in Spell.dbc, with whether it is passive.

    The keys stop a talent grid pointing at a rank that was never generated; the values say which talents
    teach a castable ability.
    """
    dbc = Dbc(os.path.join(SERVER_DBC, 'Spell.dbc'))
    return {dbc.field(record, 0): bool(dbc.field(record, SPELL_ATTRIBUTES) & SPELL_ATTR0_PASSIVE)
            for record in dbc.records}


def build_talent_grid(definitions):
    """Writes Talent.dbc: the rows of every tree a class brings of its own.

    Talent.dbc carries no string block, so one build serves both the client and the server.
    """
    dbc = Dbc(backup(os.path.join(SERVER_DBC, 'Talent.dbc')))
    assert dbc.fields == TALENT_FIELDS, 'unexpected Talent layout'
    known_ids = {dbc.field(record, 0) for record in dbc.records}
    spells = read_spell_ids()
    added = 0
    for own in (tab for definition in definitions for tab in talent_tabs(definition)):
        talents = own['talents']
        by_name = {talent['name']: talent for talent in talents}
        assert len(by_name) == len(talents), f"duplicate talent name in tab {own['id']}"
        taken = set()
        for talent in talents:
            assert talent['id'] not in known_ids, f"talent id {talent['id']} is already used"
            known_ids.add(talent['id'])
            cell = (talent['tier'], talent['column'])
            assert cell not in taken, f'two talents share cell {cell}'
            assert 0 <= talent['column'] < TALENT_COLUMNS, f'column {talent["column"]} is off the grid'
            taken.add(cell)
            ranks = talent['ranks']
            assert 0 < len(ranks) <= TALENT_MAX_RANKS, f"{talent['name']} has {len(ranks)} ranks"
            missing = [spell for spell in ranks if spell not in spells]
            assert not missing, f"{talent['name']}: spells {missing} are not in Spell.dbc"

            record = bytearray(dbc.record_size)
            dbc.set_field(record, 0, talent['id'])
            dbc.set_field(record, TALENT_TAB, own['id'])
            dbc.set_field(record, TALENT_TIER, talent['tier'])
            dbc.set_field(record, TALENT_COLUMN, talent['column'])
            for index, spell in enumerate(ranks):
                dbc.set_field(record, TALENT_RANKS + index, spell)
            # A talent whose rank is an ability (Blizzard's Mortal Strike pattern) must say so, or the core only
            # shows the spell to the client and never learns it: it sits in the spellbook and does nothing.
            # The core itself refuses the flag for passive spells, so the same rule decides it here.
            if not any(spells[spell] for spell in ranks):
                dbc.set_field(record, TALENT_ADD_TO_SPELLBOOK, 1)
            requires = talent.get('requires')
            if requires:
                prerequisite = by_name.get(requires['talent'])
                assert prerequisite, f"{talent['name']} requires unknown talent {requires['talent']}"
                assert prerequisite['column'] == talent['column'], \
                    f"{talent['name']} must share its prerequisite's column for the client to draw the line"
                dbc.set_field(record, TALENT_PREREQ_TALENT, prerequisite['id'])
                dbc.set_field(record, TALENT_PREREQ_RANK, requires.get('rank', len(prerequisite['ranks'])) - 1)
            dbc.records.append(record)
            added += 1

    dbc.write(os.path.join(SERVER_DBC, 'Talent.dbc'))
    dbc.write(os.path.join(CLIENT_DBC_OUT, 'Talent.dbc'))
    return added


def extend_gt_table(block_size):
    """Fills the custom class's block in a gt* table (crit from agility, regen per spirit, rating scalars).

    These tables are flat arrays the client indexes by (class - 1) * block + row, with no id to look up, so a
    class past the last block is an out-of-bounds read: the client crashes as soon as the character sheet
    asks for a stat. The block is copied from the template class, which is what the server computes with.
    """
    def mutate(dbc, definition):
        assert len(dbc.records) % block_size == 0, 'unexpected gt table length'
        template_start = (definition['templateClass'] - 1) * block_size
        template_block = [bytearray(record) for record
                          in dbc.records[template_start:template_start + block_size]]
        # Class slots between the last stock class and this one belong to no class; class 1 keeps them sane
        while len(dbc.records) < (definition['id'] - 1) * block_size:
            dbc.records.extend(bytearray(record) for record in dbc.records[:block_size])
        start = (definition['id'] - 1) * block_size
        dbc.records[start:start + block_size] = template_block
        # Tables with an id column number their rows from 1
        if dbc.fields > 1:
            for index, record in enumerate(dbc.records):
                dbc.set_field(record, 0, index + 1)

    return mutate


def build_dbc(source_dir, target_path, name, definitions, mutate, finish=None):
    dbc = Dbc(backup(os.path.join(source_dir, name)) if source_dir == SERVER_DBC
              else os.path.join(source_dir, name))
    for definition in definitions:
        mutate(dbc, definition)
    if finish:
        finish(dbc)
    dbc.write(target_path)
    return dbc


def sql_value(text):
    return "'" + text.replace('\\', '\\\\').replace("'", "\\'") + "'"


# lfg::LfgRoles: the bits the client and the server exchange for a Dungeon Finder role
LFG_ROLE_LEADER = 1
LFG_ROLE_BITS = {'tank': 2, 'healer': 4, 'damage': 8}

# Wow.exe's own role table is patched by localTools/clientExe/buildWowExePatch.py, from role_mask below


def role_mask(definition):
    """The Dungeon Finder roles a class may queue as, as the server's role mask."""
    return sum(LFG_ROLE_BITS[role] for role in set(definition.get('roles', ['damage'])))


def build_sql(definitions):
    lines = ['-- Generated by localTools/customClasses/buildCustomClasses.py from classes.json. Do not edit.',
             '']
    ids = ', '.join(str(definition['id']) for definition in definitions)
    # A slot no longer in classes.json is not playable any more, so nothing of it stays behind
    dropped = [slot for slot in CUSTOM_SLOTS if slot not in {d['id'] for d in definitions}]
    if dropped:
        slots = ', '.join(str(slot) for slot in dropped)
        masks = ', '.join(str(1 << (slot - 1)) for slot in dropped)
        lines += [
            f'-- Custom class slots no class uses any more: {slots}',
            f'DELETE FROM `custom_class` WHERE `ClassId` IN ({slots});',
            f'DELETE FROM `playercreateinfo` WHERE `class` IN ({slots});',
            f'DELETE FROM `player_class_stats` WHERE `Class` IN ({slots});',
            f'DELETE FROM `playercreateinfo_skills` WHERE `classMask` IN ({masks});',
            f'DELETE FROM `playercreateinfo_spell_custom` WHERE `classmask` IN ({masks});',
            f'DELETE FROM `trainer` WHERE `Type` = 0 AND `Requirement` IN ({slots});',
            '',
        ]
    lines.append(f'DELETE FROM `custom_class` WHERE `ClassId` IN ({ids});')
    lines.append('INSERT INTO `custom_class` (`ClassId`, `TemplateClass`, `InheritSpells`, `Roles`, `StartLevel`,'
                 ' `Name`, `Comment`) VALUES')
    lines.append(',\n'.join(
        f"({d['id']}, {d['templateClass']}, {int(d.get('inheritSpells', True))}, {role_mask(d)},"
        f" {int(d.get('startLevel', 0))}, {sql_value(d['name'])}, {sql_value(d.get('comment', ''))})"
        for d in definitions) + ';')
    lines.append('')

    for definition in definitions:
        class_id = definition['id']
        template = definition['templateClass']
        start = start_class(definition)
        races = ', '.join(str(race) for race in definition['races'])
        class_bit = 1 << (class_id - 1)
        template_bit = 1 << (template - 1)
        lines += [
            f'-- Class {class_id} ({definition["name"]}), built on class {template}',
            f'DELETE FROM `playercreateinfo` WHERE `class` = {class_id};',
        ]
        shared = definition.get('startLike')
        if shared:
            # Every race of the class starts where one race and class of the stock game does
            lines += [
                f'-- Every race starts where race {shared["race"]} class {shared["class"]} does',
                'INSERT INTO `playercreateinfo` (`race`, `class`, `map`, `zone`, `position_x`, `position_y`,'
                ' `position_z`, `orientation`)',
                f'  SELECT `races`.`race`, {class_id}, `start`.`map`, `start`.`zone`, `start`.`position_x`,'
                ' `start`.`position_y`, `start`.`position_z`, `start`.`orientation`',
                f'  FROM (SELECT * FROM `playercreateinfo` WHERE `race` = {shared["race"]}'
                f' AND `class` = {shared["class"]}) `start`',
                '  JOIN (SELECT DISTINCT `race` FROM `playercreateinfo`'
                f' WHERE `race` IN ({races})) `races`;',
            ]
        else:
            lines += [
                'INSERT INTO `playercreateinfo` (`race`, `class`, `map`, `zone`, `position_x`, `position_y`,'
                ' `position_z`, `orientation`)',
                f'  SELECT `race`, {class_id}, `map`, `zone`, `position_x`, `position_y`, `position_z`,'
                f' `orientation` FROM `playercreateinfo` WHERE `class` = {start} AND `race` IN ({races});',
                '-- Races the start class cannot be: a starting point belongs to the race, so any of its'
                ' classes gives the same one',
                'INSERT INTO `playercreateinfo` (`race`, `class`, `map`, `zone`, `position_x`, `position_y`,'
                ' `position_z`, `orientation`)',
                f'  SELECT `p`.`race`, {class_id}, `p`.`map`, `p`.`zone`, `p`.`position_x`, `p`.`position_y`,'
                ' `p`.`position_z`, `p`.`orientation` FROM (SELECT * FROM `playercreateinfo`) `p`',
                '  JOIN (SELECT `race`, MIN(`class`) AS `class` FROM `playercreateinfo`'
                f' WHERE `race` IN ({races}) GROUP BY `race`) `pick`'
                ' ON `pick`.`race` = `p`.`race` AND `pick`.`class` = `p`.`class`',
                f'  WHERE `p`.`race` NOT IN (SELECT `race` FROM (SELECT `race` FROM `playercreateinfo`'
                f' WHERE `class` = {class_id}) `taken`);',
            ]
        lines += [
            f'DELETE FROM `player_class_stats` WHERE `Class` = {class_id};',
            'INSERT INTO `player_class_stats` (`Class`, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`,'
            ' `Stamina`, `Intellect`, `Spirit`)',
            f'  SELECT {class_id}, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`, `Stamina`,'
            f' `Intellect`, `Spirit` FROM `player_class_stats` WHERE `Class` = {template};',
            '-- Levels the template class never sees (the Death Knight starts at 55) come from the start'
            ' class, or the server refuses to start',
            'INSERT INTO `player_class_stats` (`Class`, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`,'
            ' `Stamina`, `Intellect`, `Spirit`)',
            f'  SELECT {class_id}, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`, `Stamina`,'
            f' `Intellect`, `Spirit` FROM (SELECT * FROM `player_class_stats` WHERE `Class` = {start})'
            ' `fallback`',
            f'  WHERE `Level` NOT IN (SELECT `Level` FROM (SELECT `Level` FROM `player_class_stats`'
            f' WHERE `Class` = {class_id}) `have`);',
            f'DELETE FROM `playercreateinfo_skills` WHERE `classMask` = {class_bit};',
            'INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`)',
            f'  SELECT `raceMask`, {class_bit}, `skill`, `rank`, CONCAT({sql_value(definition["name"])},'
            " ' - ', `comment`) FROM `playercreateinfo_skills`"
            f'  WHERE `classMask` & {template_bit} AND `classMask` != 0;',
            f'DELETE FROM `playercreateinfo_spell_custom` WHERE `classmask` = {class_bit};',
        ]
        own_skill = definition.get('classSkill')
        if own_skill:
            dropped = ', '.join(str(skill) for skill in sorted(definition['_templateClassSkills']))
            lines += [
                "-- The class's own spellbook tab replaces the template's class skill lines",
                f'DELETE FROM `playercreateinfo_skills` WHERE `classMask` = {class_bit}'
                f' AND `skill` IN ({dropped});',
                'INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`)'
                f' VALUES (0, {class_bit}, {own_skill["id"]}, 1,'
                f' {sql_value(own_skill.get("name", definition["name"]))});',
            ]
        # A class that does not start from its template's spellbook is given its own, listed in classes.json
        if definition.get('startSpells'):
            values = ',\n'.join(f"  (0, {class_bit}, {spell}, {sql_value(definition['name'])})"
                                for spell in definition['startSpells'])
            lines += [
                'INSERT INTO `playercreateinfo_spell_custom` (`racemask`, `classmask`, `Spell`, `Note`)'
                ' VALUES',
                values + ';',
            ]
        lines += [
            f'DELETE FROM `trainer` WHERE `Type` = 0 AND `Requirement` = {class_id};',
            'INSERT INTO `trainer` (`Id`, `Type`, `Requirement`, `Greeting`, `VerifiedBuild`)',
            f'  SELECT COALESCE(MAX(`Id`), 0) + 1, 0, {class_id},'
            f' {sql_value("Ready for some training?")}, 0 FROM `trainer`;',
            '',
        ]
    return '\n'.join(lines) + '\n'


def details_cell(definition):
    """The class's cell in Details' icon sheet, painted by localTools/interface/buildDetailsClassIcons.py."""
    cell = definition.get('detailsCell')
    return ', detailsCell = { %d, %d }' % (cell[0], cell[1]) if cell else ''


def build_lua(definitions):
    lines = ['-- Generated by localTools/customClasses/buildCustomClasses.py from classes.json. Do not edit.',
             '-- Custom classes known to the client: token, name, color, class icon cell and the Dungeon',
             '-- Finder roles it may queue as.',
             'CustomClasses = {']
    for definition in definitions:
        colour = definition.get('color', [0.7, 0.7, 0.7])
        column, row = definition.get('iconCell', [0, 0])
        roles = set(definition.get('roles', ['damage']))
        assert roles <= {'tank', 'healer', 'damage'}, f"unknown role in {definition['roles']}"
        lines.append('    [%d] = { token = "%s", name = "%s", color = { %.3f, %.3f, %.3f },'
                     ' iconCell = { %d, %d },'
                     ' roles = { tank = %s, healer = %s, damage = %s }%s },'
                     % (definition['id'], definition['token'], definition['name'], colour[0], colour[1],
                        colour[2], column, row, str('tank' in roles).lower(), str('healer' in roles).lower(),
                        str('damage' in roles).lower(), details_cell(definition)))
    lines.append('}')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client', default='C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK')
    args = parser.parse_args()

    root = json.load(open(DEFINITIONS, encoding='utf8'))
    definitions = root['classes']
    for definition in definitions:
        assert definition['id'] in CUSTOM_SLOTS, f"class id {definition['id']} is not a custom slot"

    ensure_client_dbc(args.client)

    # A class with its own spellbook tab drops the template's class skill lines: which ids those are is
    # only known from SkillLine.dbc, so it is resolved once here and handed to the SkillRaceClassInfo pass
    class_skills = read_class_skill_lines()
    for definition in definitions:
        definition['_templateClassSkills'] = class_skills

    builders = [('ChrClasses.dbc', add_class_row), ('CharBaseInfo.dbc', add_race_pairs),
                ('SkillLine.dbc', add_class_skill_line),
                ('SkillRaceClassInfo.dbc', add_skills),
                ('CharStartOutfit.dbc', add_start_outfit, dress_stock_previews(root.get('stockPreviewOutfits', {})))]
    builders += [(name, extend_gt_table(block)) for name, block in GT_TABLES.items()]
    builders.append(('TalentTab.dbc', add_talent_tab))
    for name, mutate, *finish in builders:
        # The client copy keeps the client's own localized strings; the server copy keeps the server's
        build_dbc(CLIENT_DBC_CACHE, os.path.join(CLIENT_DBC_OUT, name), name, definitions, mutate, *finish)
        build_dbc(SERVER_DBC, os.path.join(SERVER_DBC, name), name, definitions, mutate, *finish)
    talent_rows = build_talent_grid(definitions)

    os.makedirs(os.path.dirname(SQL_OUT), exist_ok=True)
    with open(SQL_OUT, 'w', encoding='utf8', newline='\n') as file:
        file.write(build_sql(definitions))
    for path in LUA_OUT:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf8', newline='\n') as file:
            file.write(build_lua(definitions))

    names = ', '.join(f"{d['name']} ({d['id']})" for d in definitions)
    print(f'Custom classes generated: {names}')
    own_trees = ', '.join(f"{tab['name']} ({tab['id']})" for d in definitions for tab in talent_tabs(d))
    print(f'  talent trees: {own_trees or "none"} - {talent_rows} talents')
    print(f'  client data : {CLIENT_DBC_OUT}')
    print(f'  server data : {SERVER_DBC}')
    print(f'  world SQL   : {SQL_OUT}')


if __name__ == '__main__':
    sys.exit(main())
