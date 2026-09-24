"""Builds the retail-style talent trees of every custom class that has one (`talentTree` in classes.json).

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
    values = node.get('values')
    return node['text'].replace('{0}', str(values[rank])) if values else node['text']


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
        "    `Name` VARCHAR(64) NOT NULL DEFAULT '',",
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
                     '`Gate1Row`, `Gate1Cost`, `Gate2Row`, `Gate2Cost`, `Signature`, `SpecSpells`, `Name`) VALUES')
        rows = []
        for tree in definition['trees']:
            gates = (tree.get('gates', []) + [{'row': 0, 'cost': 0}] * 2)[:2]
            rows.append(f"({class_id}, {tree['id']}, {TREE_KINDS[tree['kind']]}, {tree['firstLevel']}, "
                        f"{tree['levelStep']}, {gates[0]['row']}, {gates[0]['cost']}, {gates[1]['row']}, "
                        f"{gates[1]['cost']}, {stamp}, "
                        f"'{','.join(str(spell) for spell in tree.get('specSpells', []))}', {sql_string(tree['name'])})")
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
        lines.append('    },')
    lines.append('}')
    lines.append('')
    return '\n'.join(lines)


def main():
    classes = json.load(open(CLASSES, encoding='utf8'))['classes']
    definitions = []
    for entry in classes:
        if not entry.get('talentTree'):
            continue
        definition = json.load(open(os.path.join(REPO, entry['talentTree']), encoding='utf8'))
        assert definition['classId'] == entry['id'], f"{entry['talentTree']} is for class {definition['classId']}"
        definitions.append(definition)

    spells = spell_ids()
    for definition in definitions:
        check(definition, spells)

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
