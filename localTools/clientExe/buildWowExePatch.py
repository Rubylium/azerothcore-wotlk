"""Builds clientPatcher/template/WowExePatch.json: every change made to Wow.exe 3.3.5a (build 12340).

Wow.exe is patched in one place only, clientPatcher/template/Patch-WowExe.ps1, which the installer runs on the
player's machine. Each entry below is a byte range with its stock and its patched bytes: the script refuses to
touch a Wow.exe whose ranges are neither, keeps the original, and does nothing when everything is applied.

Two sources feed it:
  - awesome_wotlk (https://github.com/Rubylium/awesome_wotlk): the loader that makes Wow.exe load
    AwesomeWotlkLib.dll (MSDF font rendering, bug fixes, extra Lua API). Its bytes are read from the fork's own
    src/AwesomeWotlkPatch/Patch.h, so the fork stays the single source of truth for them.
  - the custom classes (localTools/customClasses/classes.json): the Dungeon Finder role of each class, which
    Wow.exe reads from a table of its own (see WOW_EXE_ROLE_TABLE_VA).

Stock bytes are read from a reference Wow.exe that has never been patched.

Usage: python buildWowExePatch.py [--stock-exe <Wow.exe>] [--awesome-wotlk <repo>]
"""
import argparse
import json
import os
import re
import sys

import pefile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, 'localTools', 'customClasses'))
from buildCustomClasses import DEFINITIONS, LFG_ROLE_LEADER, role_mask  # noqa: E402

OUTPUT = os.path.join(REPO, 'clientPatcher', 'template', 'WowExePatch.json')
DEFAULT_STOCK_EXE = r'C:\Users\alexi\Documents\GitHub\CleanWOTLK\Wow.exe.before-custom-classes'
DEFAULT_AWESOME_WOTLK = r'C:\Users\alexi\Documents\GitHub\awesome_wotlk'
BUILD = 12340

# Wow.exe takes a class's Dungeon Finder roles from a static table, one byte per class id (bit 1 leader, 2 tank,
# 4 healer, 8 damage). Every role decision reads it: the buttons GetAvailableRoles enables, the filter
# SetLFGRoles applies before storing, and the check before JoinLFG sends. A class without a byte there can take
# no role at all, whatever the interface shows. The table covers ids 0-11. Slot 10 is an unused zero, and id 12
# lands on the alignment padding before the next (dword) table, so both can be written; ids 13-15 would
# overwrite that next table.
WOW_EXE_ROLE_TABLE_VA = 0xA394AF
WOW_EXE_ROLE_TABLE_SIZE = 16
WOW_EXE_PATCHABLE_ROLE_SLOTS = (10, 12)


class StockExe:
    """The reference Wow.exe: turns virtual addresses into file offsets and reads the stock bytes there."""

    def __init__(self, path):
        self.pe = pefile.PE(path)
        version = self.pe.VS_FIXEDFILEINFO[0]
        assert version.FileVersionLS & 0xFFFF == BUILD, f'{path} is not Wow.exe build {BUILD}'
        self.data = open(path, 'rb').read()
        self.base = self.pe.OPTIONAL_HEADER.ImageBase

    def offset(self, virtual_address):
        return self.pe.get_offset_from_rva(virtual_address - self.base)

    def read(self, virtual_address, size):
        offset = self.offset(virtual_address)
        return self.data[offset:offset + size]


def patch(exe, name, virtual_address, patched):
    stock = exe.read(virtual_address, len(patched))
    return {
        'name': name,
        'va': f'0x{virtual_address:08X}',
        'offset': exe.offset(virtual_address),
        'stock': stock.hex(),
        'patched': patched.hex(),
    }


def read_awesome_wotlk_patches(repo):
    """The loader patches of awesome_wotlk, parsed from src/AwesomeWotlkPatch/Patch.h.

    Each entry there is `{ 0x<virtual address>, // <label>` followed by adjacent hex string literals.
    """
    path = os.path.join(repo, 'src', 'AwesomeWotlkPatch', 'Patch.h')
    lines = []
    labels = {}
    for line in open(path, encoding='utf8').read().splitlines():
        code, _, comment = line.partition('//')
        address = re.search(r'0x([0-9A-Fa-f]+)', code)
        if address:
            labels[int(address.group(1), 16)] = comment.strip()
        lines.append(code)
    source = '\n'.join(lines)

    block = re.search(r's_patches\s*\[\]\s*=\s*\{(.*)\};', source, re.S)
    assert block, f'no s_patches table in {path}'
    patches = []
    for match in re.finditer(r'\{\s*0x([0-9A-Fa-f]+)\s*,((?:\s*"[0-9A-Fa-f]*")+)\s*\}', block.group(1)):
        virtual_address = int(match.group(1), 16)
        hex_bytes = ''.join(re.findall(r'"([0-9A-Fa-f]*)"', match.group(2)))
        patches.append((virtual_address, labels.get(virtual_address, ''), bytes.fromhex(hex_bytes)))
    assert patches, f'no patches parsed from {path}'
    return patches


def role_table_patch(exe):
    definitions = json.load(open(DEFINITIONS, encoding='utf8'))['classes']
    table = bytearray(exe.read(WOW_EXE_ROLE_TABLE_VA, WOW_EXE_ROLE_TABLE_SIZE))
    names = []
    for definition in definitions:
        assert definition['id'] in WOW_EXE_PATCHABLE_ROLE_SLOTS, \
            f"class {definition['id']} has no free byte in Wow.exe's role table: only {WOW_EXE_PATCHABLE_ROLE_SLOTS}"
        table[definition['id']] = LFG_ROLE_LEADER | role_mask(definition)
        names.append(f"{definition['name']} ({definition['id']})")
    return patch(exe, 'Dungeon Finder roles: ' + ', '.join(names), WOW_EXE_ROLE_TABLE_VA, bytes(table))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--stock-exe', default=DEFAULT_STOCK_EXE)
    parser.add_argument('--awesome-wotlk', default=DEFAULT_AWESOME_WOTLK)
    args = parser.parse_args()

    exe = StockExe(args.stock_exe)
    patches = [patch(exe, f'awesome_wotlk loader: {label}', virtual_address, patched)
               for virtual_address, label, patched in read_awesome_wotlk_patches(args.awesome_wotlk)]
    patches.append(role_table_patch(exe))

    # Two ranges writing the same bytes would each see the other's work as a foreign modification
    ranges = sorted((p['offset'], p['offset'] + len(p['patched']) // 2, p['name']) for p in patches)
    for (_, end, first), (start, _, second) in zip(ranges, ranges[1:]):
        assert end <= start, f'{first} and {second} overlap'

    spec = {
        '_comment': 'Generated by localTools/clientExe/buildWowExePatch.py; applied by Patch-WowExe.ps1.',
        'file': 'Wow.exe',
        'build': BUILD,
        'patches': patches,
    }
    with open(OUTPUT, 'w', encoding='utf8', newline='\n') as file:
        json.dump(spec, file, indent=2, ensure_ascii=False)
        file.write('\n')

    for entry in patches:
        print(f"{entry['va']}  {len(entry['patched']) // 2:3} bytes  {entry['name']}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
