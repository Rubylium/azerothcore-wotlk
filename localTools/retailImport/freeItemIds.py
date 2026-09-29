"""Lists item entries free to carry a new custom item: below 65536 (above, the client DLL reads a Mythic+ generated
item), present in the client's Item.dbc (else no icon or model), and with no item_template on the server nor a claim
in the repository's SQL (an import or a generator that has not reached the live database yet).

The server's Item.dbc (server/Data/dbc) is the client's, patched by localTools/patchSinisterStrike.ps1: rows it has
already repurposed show their new class and display.

Usage: python localTools/retailImport/freeItemIds.py [--class 2] [--limit 40]
"""
import argparse
import os
import re
import struct
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ITEM_DBC = os.path.join(REPO, 'server', 'Data', 'dbc', 'Item.dbc')
MYSQL = r'C:\laragon\bin\mysql\mariadb-12.0.2-winx64\bin\mysql.exe'
CLASSES = {0: 'consumable', 1: 'container', 2: 'weapon', 3: 'gem', 4: 'armour', 7: 'trade goods', 9: 'recipe',
           12: 'quest', 15: 'misc'}


def read_item_dbc():
    data = open(ITEM_DBC, 'rb').read()
    magic, records, fields, size, _ = data[:4], *struct.unpack('<4I', data[4:20])
    if magic != b'WDBC' or fields != 8:
        raise SystemExit(f'unexpected Item.dbc layout: {magic} {fields} fields')
    rows = {}
    for index in range(records):
        entry, item_class, subclass, _sound, material, display, inventory, sheath = struct.unpack_from(
            '<8i', data, 20 + index * size)
        rows[entry] = (item_class, subclass, display, inventory)
    return rows


def templated_entries():
    output = subprocess.run([MYSQL, '--user=root', '--port=3307', '--host=localhost', '--protocol=TCP', '-N', '-e',
                             'SELECT entry FROM acore_world.item_template WHERE entry < 65536'],
                            capture_output=True, text=True, check=True).stdout
    return {int(line) for line in output.split() if line.strip().isdigit()}


def claimed_in_repo():
    """Entries an item_template INSERT in the repository's SQL names, applied or not"""
    claimed = set()
    for root, _dirs, files in os.walk(os.path.join(REPO, 'modules')):
        for name in files:
            if not name.endswith('.sql'):
                continue
            text = open(os.path.join(root, name), encoding='utf-8', errors='ignore').read()
            for block in re.findall(r'INSERT INTO `?item_template`?.*?;', text, re.S | re.I):
                claimed.update(int(value) for value in re.findall(r'\(\s*(\d{1,5})\s*,', block))
    return claimed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--class', dest='item_class', type=int, help='only rows of this Item.dbc class')
    parser.add_argument('--limit', type=int, default=40)
    args = parser.parse_args()

    rows = read_item_dbc()
    taken = templated_entries() | claimed_in_repo()
    free = [(entry, row) for entry, row in sorted(rows.items())
            if entry < 65536 and entry not in taken and (args.item_class is None or row[0] == args.item_class)]
    print(f'{len(free)} free entries (Item.dbc rows below 65536 with no item_template and no claim in the repo)')
    for entry, (item_class, subclass, display, inventory) in free[:args.limit]:
        print(f'  {entry:5d}  class {item_class} ({CLASSES.get(item_class, "?")}), subclass {subclass}, '
              f'display {display}, inventory type {inventory}')


if __name__ == '__main__':
    main()
