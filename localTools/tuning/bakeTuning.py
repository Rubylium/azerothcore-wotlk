"""Bakes live tuning into the code (src/server/game/Tuning/LiveTuning.h).

A knob overridden with `.tune set` while the server ran has its value written as its default in the source that
declares it, `LiveTuning::Knob const Name("key", <value>)`, and its override removed from the world database's
`live_tuning`, so the code is again the one place the number lives. Spell multipliers (`.tune spell`, table
`live_tuning_spell`) are listed, not baked: put the factor in the spell's data or script, then `.tune reset`.

    python localTools/tuning/bakeTuning.py            # bake every override
    python localTools/tuning/bakeTuning.py --dry-run  # show what it would change
    python localTools/tuning/bakeTuning.py --keep     # bake, but leave the overrides in the database

A server running from the old build keeps the override's value either way; build and restart after baking.
"""

import argparse
import os
import re
import subprocess
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SOURCE_ROOTS = [os.path.join(REPO_ROOT, 'src'), os.path.join(REPO_ROOT, 'modules')]
SOURCE_EXTENSIONS = ('.cpp', '.h')
MYSQL_ROOT = r'C:\laragon\bin\mysql'
WORLD_CONFIG = os.path.join(REPO_ROOT, 'server', 'configs', 'worldserver.conf')

# Knob const Name("key", value) / KnobInt / KnobUInt; the value: a number literal with an optional f / u suffix
DECLARATION = r'(LiveTuning::Knob(?:Int|UInt)?\s+const\s+\w+\s*\(\s*"{key}"\s*,\s*)(-?[0-9.]+(?:e-?[0-9]+)?[fFuU]?)(\s*\))'


def world_database():
    with open(WORLD_CONFIG, encoding='utf-8', errors='replace') as config:
        for line in config:
            match = re.match(r'\s*WorldDatabaseInfo\s*=\s*"([^"]+)"', line)
            if match:
                host, port, user, password, name = match.group(1).split(';')
                return host, port, user, password, name
    sys.exit(f'WorldDatabaseInfo was not found in {WORLD_CONFIG}')


def mysql_path():
    for root, _, files in os.walk(MYSQL_ROOT):
        if 'mysql.exe' in files:
            return os.path.join(root, 'mysql.exe')
    return 'mysql'


def query(sql):
    host, port, user, password, name = world_database()
    environment = dict(os.environ, MYSQL_PWD=password)
    result = subprocess.run([mysql_path(), f'--host={host}', f'--port={port}', f'--user={user}', '--protocol=TCP',
                             '-N', '-B', name, '-e', sql], capture_output=True, text=True, env=environment)
    if result.returncode != 0:
        sys.exit(result.stderr.strip())
    return [line.split('\t') for line in result.stdout.splitlines() if line.strip()]


def source_files():
    for root in SOURCE_ROOTS:
        for directory, subdirectories, files in os.walk(root):
            subdirectories[:] = [entry for entry in subdirectories if entry not in ('.git', 'build')]
            for file in files:
                if file.endswith(SOURCE_EXTENSIONS):
                    yield os.path.join(directory, file)


def literal(value, old):
    """The new value written the way the old one was: an integer stays one, a float keeps its f"""
    suffix = old[-1] if old[-1] in 'fFuU' else ''
    number = float(value)
    if suffix in 'uU' or (suffix == '' and '.' not in old):
        return f'{int(round(number))}{suffix}'
    text = f'{number:.6g}'
    if '.' not in text and 'e' not in text:
        text += '.0'
    return text + suffix


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='show the changes, write nothing')
    parser.add_argument('--keep', action='store_true', help='leave the overrides in the database')
    arguments = parser.parse_args()

    overrides = query('SELECT `Key`, `Value` FROM live_tuning ORDER BY `Key`')
    spells = query('SELECT `SpellId`, `Multiplier` FROM live_tuning_spell ORDER BY `SpellId`')
    if not overrides and not spells:
        print('Nothing to bake: no override in live_tuning or live_tuning_spell.')
        return

    files = {path: None for path in source_files()}
    baked = []
    for key, value in overrides:
        pattern = re.compile(DECLARATION.format(key=re.escape(key)))
        found = False
        for path in files:
            if files[path] is None:
                with open(path, encoding='utf-8', newline='') as source:
                    files[path] = source.read()
            match = pattern.search(files[path])
            if not match:
                continue
            new = literal(value, match.group(2))
            print(f'{os.path.relpath(path, REPO_ROOT)}: {key} {match.group(2)} -> {new}')
            files[path] = files[path][:match.start(2)] + new + files[path][match.end(2):]
            found = True
            break
        if found:
            baked.append(key)
        else:
            print(f'{key}: no declaration in the source (renamed or removed?), left in the database')

    for spell, multiplier in spells:
        print(f'spell {spell} x{multiplier}: not baked - put it in the spell\'s data or script, then .tune reset')

    if arguments.dry_run:
        return
    for path, content in files.items():
        if content is None:
            continue
        with open(path, encoding='utf-8', newline='') as source:
            if source.read() == content:
                continue
        with open(path, 'w', encoding='utf-8', newline='') as source:
            source.write(content)
    if baked and not arguments.keep:
        keys = ', '.join("'" + key.replace("'", "''") + "'" for key in baked)
        query(f'DELETE FROM live_tuning WHERE `Key` IN ({keys})')
    print(f'{len(baked)} knob(s) baked{" (overrides kept)" if arguments.keep else ""}; build and restart.')


if __name__ == '__main__':
    main()
