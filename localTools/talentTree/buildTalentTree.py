"""Builds the retail-style talent trees of every class that has one: the custom classes' (`talentTree` in
classes.json) and the stock classes moved to them (`stockTalentTrees` in classes.json, the Mage first).

A tree is data: the server reads it from the world database and the talent window from a Lua table shipped in
the interface patch, and both come out of this script from the same JSON, so they cannot drift apart without the
signature changing (the window compares it with the server's on open).

  modules/mod-custom-classes/data/sql/db-world/base/custom_talent_tree.sql
  clientPatcher/interface/Interface/FrameXML/TalentTreeData.lua

Every rank spell must already be in Spell.dbc: run localTools/patchSinisterStrike.ps1 first (it generates them
from the same JSON).

Usage: python localTools/talentTree/buildTalentTree.py
"""
import json
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLASSES = os.path.join(REPO, 'localTools', 'customClasses', 'classes.json')
SPELL_DBC = os.path.join(REPO, 'server', 'Data', 'dbc', 'Spell.dbc')
SQL_OUT = os.path.join(REPO, 'modules', 'mod-custom-classes', 'data', 'sql', 'db-world', 'base',
                       'custom_talent_tree.sql')
LUA_OUT = os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', 'FrameXML', 'TalentTreeData.lua')

ROWS, COLUMNS = 10, 7
KINDS = {'passive': 0, 'active': 1, 'choice': 2}
ICONS = 'Interface' + chr(92) + 'Icons' + chr(92)
TREE_KINDS = {'class': 0, 'spec': 1}
# A node's state travels as one digit, so a rank (or a choice) never goes past 9
MAX_VALUE = 9


def spell_ids():
    data = open(SPELL_DBC, 'rb').read()
    count, _, record_size, _ = struct.unpack_from('<4I', data, 4)
    return {struct.unpack_from('<I', data, 20 + index * record_size)[0] for index in range(count)}


def node_spells(node):
    """The spells behind a node's digit: rank 1..n, or choice option 1..n."""
    if node['kind'] == 'choice':
        return [option['spell'] for option in node['options']]
    return node['spells']


def check(definition, spells):
    seen_nodes, seen_spells = set(), set()
    kinds = [tree['kind'] for tree in definition['trees']]
    assert kinds.count('class') == 1, 'a class has exactly one class tree'
    assert kinds.count('spec') >= 1, 'and at least one spec tree'
    for tree in definition['trees']:
        assert tree['kind'] in TREE_KINDS, f"tree {tree['id']}: kind {tree['kind']}"
        assert tree['levelStep'] > 0 and tree['firstLevel'] > 0
        assert tree['kind'] == 'spec' or not tree.get('specSpells'), f"{tree['name']}: only a spec tree grants spells"
        for spell in tree.get('specSpells', []):
            assert spell in spells, f"{tree['name']}: spec spell {spell} is not in Spell.dbc"
        by_id = {node['id']: node for node in tree['nodes']}
        cells = set()
        for node in tree['nodes']:
            where = f"{tree['name']} node {node['id']}"
            assert node['id'] not in seen_nodes, f'{where}: id used twice'
            seen_nodes.add(node['id'])
            assert node['kind'] in KINDS, f"{where}: kind {node['kind']}"
            assert 0 <= node['row'] < ROWS and 0 <= node['col'] < COLUMNS, f'{where}: off the grid'
            cell = (node['row'], node['col'])
            assert cell not in cells, f'{where}: shares its cell'
            cells.add(cell)
            ranks = node_spells(node)
            assert 0 < len(ranks) <= MAX_VALUE, f'{where}: {len(ranks)} ranks'
            if node['kind'] == 'choice':
                assert len(ranks) >= 2, f'{where}: a choice needs two options'
            for spell in ranks:
                assert spell in spells, f'{where}: spell {spell} is not in Spell.dbc (run patchSinisterStrike.ps1)'
                assert spell not in seen_spells, f'{where}: spell {spell} used twice'
                seen_spells.add(spell)
            if node['kind'] == 'passive' and node.get('values') is not None:
                assert len(node['values']) == len(ranks), f'{where}: one value per rank'
            for parent in node.get('parents', []):
                assert parent in by_id, f'{where}: parent {parent} is not in its tree'
                assert by_id[parent]['row'] < node['row'], f'{where}: parent {parent} is not above it'
            if node['row'] > 0:
                assert node.get('parents'), f'{where}: only the first row may have no parent'
        for pick in tree.get('botBuild', []):
            node_id, option = bot_pick(pick)
            assert node_id in by_id, f"{tree['name']}: bot build names node {node_id}, not in the tree"
            if by_id[node_id]['kind'] == 'choice':
                assert 1 <= option <= len(by_id[node_id]['options']), f"{tree['name']}: bot build option {pick}"


# A spec tree that holds a tank and a damage dealer (the Druid's Combat farouche) adds a tank build to its two
PRESET_KINDS = ('single', 'aoe', 'tank')
MAX_LEVEL = 80


def points_at(tree, level):
    if level < tree['firstLevel']:
        return 0
    return (level - tree['firstLevel']) // tree['levelStep'] + 1


def preset_values(definition, preset):
    """A preset's build as {node id: value}: "nodes" maps node ids to a rank (or, for a choice, the option)."""
    return {int(node_id): int(value) for node_id, value in preset['nodes'].items()}


def check_preset(definition, preset):
    """A recommended build (TalentTree.lua shows it in the loadout picker): legal at the level cap for the class tree
    and its specialization's tree, and spending every point both have there."""
    trees = {tree['id']: tree for tree in definition['trees']}
    where = f"class {definition['classId']} preset {preset.get('spec')}/{preset.get('kind')}"
    assert preset.get('kind') in PRESET_KINDS, f'{where}: kind must be one of {PRESET_KINDS}'
    spec = trees.get(preset.get('spec'))
    assert spec and spec['kind'] == 'spec', f'{where}: spec must be a spec tree id'
    class_tree = next(tree for tree in definition['trees'] if tree['kind'] == 'class')
    values = preset_values(definition, preset)
    nodes = {}
    for tree in (class_tree, spec):
        for node in tree['nodes']:
            nodes[node['id']] = (tree, node)
    for node_id, value in values.items():
        assert node_id in nodes, f'{where}: node {node_id} is not in the class tree or {spec["name"]}'
        tree, node = nodes[node_id]
        assert 1 <= value <= len(node_spells(node)), f'{where}: node {node_id} value {value}'
        assert node.get('level', 0) <= MAX_LEVEL, f'{where}: node {node_id} level'

    def cost(node, value):
        return 0 if value <= 0 else (1 if node['kind'] == 'choice' else value)

    def maxed(node, value):
        return value > 0 if node['kind'] == 'choice' else value >= len(node_spells(node))

    for tree in (class_tree, spec):
        spent = sum(cost(node, values.get(node['id'], 0)) for node in tree['nodes'])
        available = points_at(tree, MAX_LEVEL)
        assert spent == available, f"{where}: {tree['name']} spends {spent} of its {available} points"
        for node in tree['nodes']:
            value = values.get(node['id'], 0)
            if not value:
                continue
            if node.get('parents'):
                assert any(maxed(nodes[parent][1], values.get(parent, 0)) for parent in node['parents']), \
                    f"{where}: node {node['id']} ({node['name'] if 'name' in node else 'choice'}) has no maxed parent"
            for gate in tree.get('gates', []):
                if node['row'] >= gate['row']:
                    above = sum(cost(other, values.get(other['id'], 0)) for other in tree['nodes']
                                if other['row'] < gate['row'])
                    assert above >= gate['cost'], \
                        f"{where}: node {node['id']} is past the row {gate['row']} gate ({above}/{gate['cost']})"


def preset_build(definition, preset):
    """The build string (every node of the class, in order) the window reads"""
    values = preset_values(definition, preset)
    return ''.join(str(values.get(node['id'], 0)) for _, node in ordered_nodes(definition))


def preset_pairs(definition, tree, kind):
    """A spec tree's preset of a kind for the server, "node:value" pairs in build order (its class tree nodes
    included): what a bot takes when that build suits the content (the "aoe" one in a dungeon, the "tank" one for a
    bot on the tank build). Empty without one."""
    if tree['kind'] != 'spec':
        return ''
    preset = next((preset for preset in definition.get('presets', [])
                   if preset['spec'] == tree['id'] and preset['kind'] == kind), None)
    if not preset:
        return ''
    values = preset_values(definition, preset)
    return ','.join(f"{node['id']}:{values[node['id']]}" for _, node in ordered_nodes(definition)
                    if values.get(node['id']))


def bot_pick(pick):
    """A bot build entry: a node id (every rank of it, in order), or "<node>:<option>" for a choice."""
    text = str(pick)
    node_id, _, option = text.partition(':')
    return int(node_id), int(option or 1)


def signature(definition):
    """Order-independent and cheap. The server reads it from custom_talent_tree, the window from its own copy."""
    total = 0
    for tree in definition['trees']:
        total += tree['id'] * 7919 + tree['firstLevel'] * 31 + tree['levelStep'] * 17
        for gate in tree.get('gates', []):
            total += gate['row'] * 131 + gate['cost'] * 137
        for spell in tree.get('specSpells', []):
            total += spell * 3
        for node in tree['nodes']:
            total += node['id'] * 31 + node['row'] * 7 + node['col'] * 3 + KINDS[node['kind']] * 11
            total += node.get('level', 0) * 13
            for index, spell in enumerate(node_spells(node)):
                total += spell * (index + 1)
            for parent in node.get('parents', []):
                total += parent * 17 + node['id'] * 19
    return total % 2 ** 31


def ordered_nodes(definition):
    """Every node in build-string order: the trees in order, each tree's nodes in file order."""
    return [(tree, node) for tree in definition['trees'] for node in tree['nodes']]


def sql_string(text):
    return "'" + text.replace('\\', '\\\\').replace("'", "''") + "'"


def lua_string(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def format_text(node, rank):
    """A rank's text: {0} takes the rank's value, or {0}, {1}... its values when the rank has a list of them."""
    values = node.get('values')
    if not values:
        return node['text']
    text = node['text']
    for index, value in enumerate(values[rank] if isinstance(values[rank], list) else [values[rank]]):
        text = text.replace('{%d}' % index, str(value))
    return text


# The classes whose every damage spec casts: mage, priest, warlock, and the Necromancer (on the warlock)
CASTER_CLASSES = {5, 8, 9, 13}


def spec_role(definition, tree):
    """A spec tree's way of fighting, for the server (what the paragon board answers to, how a bot walks it): 1 melee
    damage, 2 caster damage, 3 healer, 4 tank; 0 for the class tree. A hybrid's casting damage spec says `caster`."""
    if tree['kind'] != 'spec':
        return 0
    role = tree.get('role', 'damage')
    if role == 'healer':
        return 3
    if role == 'tank':
        return 4
    return 2 if tree.get('caster') or definition['classId'] in CASTER_CLASSES else 1


def build_sql(definitions):
    lines = [
        '-- Generated by localTools/talentTree/buildTalentTree.py from each class\'s talentTree JSON. Do not edit.',
        '',
        '-- Schema lives here too, so this file never depends on another one having run first. The trees table is',
        '-- rebuilt whole: every class with trees is in this file, and a column added here reaches old databases.',
        'DROP TABLE IF EXISTS `custom_talent_tree`;',
        'CREATE TABLE `custom_talent_tree` (',
        '    `ClassId` TINYINT UNSIGNED NOT NULL,',
        '    `TreeId` TINYINT UNSIGNED NOT NULL,',
        "    `Kind` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 class tree, 1 spec tree',",
        "    `FirstLevel` TINYINT UNSIGNED NOT NULL DEFAULT 10 COMMENT 'level of the first point',",
        "    `LevelStep` TINYINT UNSIGNED NOT NULL DEFAULT 2 COMMENT 'one more point every this many levels',",
        "    `Gate1Row` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'rows from here down need Gate1Cost points above',",
        '    `Gate1Cost` TINYINT UNSIGNED NOT NULL DEFAULT 0,',
        '    `Gate2Row` TINYINT UNSIGNED NOT NULL DEFAULT 0,',
        '    `Gate2Cost` TINYINT UNSIGNED NOT NULL DEFAULT 0,',
        "    `Signature` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'of the whole class, matched by the client',",
        "    `SpecSpells` VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'a spec tree: learned while it is the chosen one',",
        "    `BotOrder` VARCHAR(512) NOT NULL DEFAULT '' COMMENT 'the order a bot takes its nodes in (node or node:option)',",
        "    `SingleBuild` VARCHAR(512) NOT NULL DEFAULT '' COMMENT 'spec tree: single-target preset',",
        "    `AoeBuild` VARCHAR(512) NOT NULL DEFAULT '' COMMENT 'spec tree: AoE preset (bots, dungeons)',",
        "    `TankBuild` VARCHAR(512) NOT NULL DEFAULT '' COMMENT 'spec tree: tank preset (with a damage dealer)',",
        "    `Name` VARCHAR(64) NOT NULL DEFAULT '',",
        "    `Role` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT "
        "'spec tree: 1 melee damage, 2 caster damage, 3 healer, 4 tank (spec_role); 0 class tree',",
        '    PRIMARY KEY (`ClassId`, `TreeId`)',
        ') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;',
        '',
        'CREATE TABLE IF NOT EXISTS `custom_talent_node` (',
        '    `ClassId` TINYINT UNSIGNED NOT NULL,',
        '    `NodeId` SMALLINT UNSIGNED NOT NULL,',
        '    `TreeId` TINYINT UNSIGNED NOT NULL,',
        "    `Position` SMALLINT UNSIGNED NOT NULL COMMENT 'index of its digit in the build string',",
        '    `Row` TINYINT UNSIGNED NOT NULL,',
        '    `Col` TINYINT UNSIGNED NOT NULL,',
        "    `Kind` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 passive, 1 active, 2 choice',",
        "    `MinLevel` TINYINT UNSIGNED NOT NULL DEFAULT 0,",
        "    `Spells` VARCHAR(128) NOT NULL DEFAULT '' COMMENT 'rank (or choice option) spells, comma separated',",
        "    `Parents` VARCHAR(64) NOT NULL DEFAULT '' COMMENT 'any one fully ranked opens the node',",
        "    `Name` VARCHAR(64) NOT NULL DEFAULT '',",
        '    PRIMARY KEY (`ClassId`, `NodeId`)',
        ') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;',
        '',
    ]
    for definition in definitions:
        class_id = definition['classId']
        stamp = signature(definition)
        lines.append(f'DELETE FROM `custom_talent_tree` WHERE `ClassId` = {class_id};')
        lines.append('INSERT INTO `custom_talent_tree` (`ClassId`, `TreeId`, `Kind`, `FirstLevel`, `LevelStep`, '
                     '`Gate1Row`, `Gate1Cost`, `Gate2Row`, `Gate2Cost`, `Signature`, `SpecSpells`, `BotOrder`, '
                     '`SingleBuild`, `AoeBuild`, `TankBuild`, `Name`, `Role`) VALUES')
        rows = []
        for tree in definition['trees']:
            gates = (tree.get('gates', []) + [{'row': 0, 'cost': 0}] * 2)[:2]
            rows.append(f"({class_id}, {tree['id']}, {TREE_KINDS[tree['kind']]}, {tree['firstLevel']}, "
                        f"{tree['levelStep']}, {gates[0]['row']}, {gates[0]['cost']}, {gates[1]['row']}, "
                        f"{gates[1]['cost']}, {stamp}, "
                        f"'{','.join(str(spell) for spell in tree.get('specSpells', []))}', "
                        f"'{','.join(str(pick) for pick in tree.get('botBuild', []))}', "
                        f"'{preset_pairs(definition, tree, 'single')}', '{preset_pairs(definition, tree, 'aoe')}', "
                        f"'{preset_pairs(definition, tree, 'tank')}', "
                        f"{sql_string(tree['name'])}, {spec_role(definition, tree)})")
        lines.append(',\n'.join(rows) + ';')
        lines.append('')
        lines.append(f'DELETE FROM `custom_talent_node` WHERE `ClassId` = {class_id};')
        lines.append('INSERT INTO `custom_talent_node` (`ClassId`, `NodeId`, `TreeId`, `Position`, `Row`, `Col`, '
                     '`Kind`, `MinLevel`, `Spells`, `Parents`, `Name`) VALUES')
        rows = []
        for position, (tree, node) in enumerate(ordered_nodes(definition)):
            name = node.get('name') or ' / '.join(option['name'] for option in node['options'])
            rows.append(f"({class_id}, {node['id']}, {tree['id']}, {position}, {node['row']}, {node['col']}, "
                        f"{KINDS[node['kind']]}, {node.get('level', 0)}, "
                        f"'{','.join(str(spell) for spell in node_spells(node))}', "
                        f"'{','.join(str(parent) for parent in node.get('parents', []))}', {sql_string(name)})")
        lines.append(',\n'.join(rows) + ';')
        lines.append('')
    return '\n'.join(lines)


def build_lua(definitions):
    lines = [
        '-- Generated by localTools/talentTree/buildTalentTree.py -- do not edit by hand.',
        '--',
        '-- The retail-style talent trees, by class id, for FrameXML/TalentTree.lua. The server holds the same trees',
        '-- (custom_talent_* in the world database) and decides what may be learned; this copy is what the window',
        '-- draws. The signature is checked against the server\'s, so a client patched out of step says so.',
        '',
        'TalentTreeData = {',
    ]
    for definition in definitions:
        lines.append(f"    [{definition['classId']}] = {{")
        lines.append(f"        signature = {signature(definition)},")
        lines.append('        trees = {')
        for tree in definition['trees']:
            gates = ', '.join(f"{{ row = {gate['row']}, cost = {gate['cost']} }}" for gate in tree.get('gates', []))
            lines.append(f"            {{ id = {tree['id']}, kind = {lua_string(tree['kind'])}, "
                         f"name = {lua_string(tree['name'])}, firstLevel = {tree['firstLevel']}, "
                         f"levelStep = {tree['levelStep']}, gates = {{ {gates} }},")
            if tree['kind'] == 'spec':
                lines.append(f"              role = {lua_string(tree.get('role', 'damage'))}, "
                             f"description = {lua_string(tree.get('description', ''))},")
            lines.append('              nodes = {')
            for node in tree['nodes']:
                parents = ', '.join(str(parent) for parent in node.get('parents', []))
                head = (f"                {{ id = {node['id']}, row = {node['row']}, col = {node['col']}, "
                        f"kind = {lua_string(node['kind'])}, level = {node.get('level', 0)}, "
                        f"parents = {{ {parents} }},")
                lines.append(head)
                if node['kind'] == 'choice':
                    lines.append('                  options = {')
                    for option in node['options']:
                        lines.append(f"                    {{ spell = {option['spell']}, "
                                     f"name = {lua_string(option['name'])}, "
                                     f"icon = {lua_string(ICONS + option['icon'])}, "
                                     f"text = {lua_string(option['text'])} }},")
                    lines.append('                  } },')
                else:
                    spells = ', '.join(str(spell) for spell in node['spells'])
                    texts = ', '.join(lua_string(format_text(node, rank)) for rank in range(len(node['spells'])))
                    lines.append(f"                  name = {lua_string(node['name'])}, "
                                 f"icon = {lua_string(ICONS + node['icon'])},")
                    lines.append(f"                  spells = {{ {spells} }},")
                    lines.append(f"                  texts = {{ {texts} }} }},")
            lines.append('              } },')
        lines.append('        },')
        # Recommended builds for the loadout picker, as build strings
        lines.append('        presets = {')
        for preset in definition.get('presets', []):
            lines.append(f"            {{ spec = {preset['spec']}, kind = {lua_string(preset['kind'])}, "
                         f"build = {lua_string(preset_build(definition, preset))} }},")
        lines.append('        },')
        lines.append('    },')
    lines.append('}')
    lines.append('')
    return '\n'.join(lines)


def main():
    classes = json.load(open(CLASSES, encoding='utf8'))
    definitions = []
    for entry in classes['classes'] + classes.get('stockTalentTrees', []):
        if not entry.get('talentTree'):
            continue
        definition = json.load(open(os.path.join(REPO, entry['talentTree']), encoding='utf8'))
        assert definition['classId'] == entry['id'], f"{entry['talentTree']} is for class {definition['classId']}"
        definitions.append(definition)

    spells = spell_ids()
    for definition in definitions:
        check(definition, spells)
        for preset in definition.get('presets', []):
            check_preset(definition, preset)

    # --check <talentTree.json>: validate one class's file (its presets included) and write nothing
    if len(sys.argv) > 2 and sys.argv[1] == '--check':
        print('ok')
        return 0

    os.makedirs(os.path.dirname(SQL_OUT), exist_ok=True)
    with open(SQL_OUT, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(build_sql(definitions))
    with open(LUA_OUT, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write(build_lua(definitions))

    for definition in definitions:
        for tree in definition['trees']:
            ranks = sum(len(node_spells(node)) if node['kind'] != 'choice' else 1 for node in tree['nodes'])
            top = tree['firstLevel'] + tree['levelStep'] * ((80 - tree['firstLevel']) // tree['levelStep'])
            points = (top - tree['firstLevel']) // tree['levelStep'] + 1
            print(f"class {definition['classId']} {tree['name']}: {len(tree['nodes'])} nodes, {ranks} ranks, "
                  f"{points} points at 80")
        print(f"  signature {signature(definition)}")
    print(SQL_OUT)
    print(LUA_OUT)


if __name__ == '__main__':
    sys.exit(main())
