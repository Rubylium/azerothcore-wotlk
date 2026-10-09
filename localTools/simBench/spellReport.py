"""Per-spell tables of a simulation sweep (sim-bench.md): for every profile, layout and spec of a sweep's rows CSV,
each spell's casts, hits, hits per cast, crit rate and share of the spec's damage, pooled over the spec's trusted
bots - read from the telemetry of the worker each row came from (acore_sim<n>_characters.mod_combat_bench_spell).
Hits per cast flat across pack5 and pack12 show a target cap; an area spell missing on packs, or one used on a
single target, shows a rotation to fix.

  python localTools/simBench/spellReport.py var/combatBench/sweep-<stamp>-rows.csv [--spec Subtlety] [--top 10]
"""
import argparse
import csv
import os
import struct
import subprocess
from collections import defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MYSQL = r'C:\laragon\bin\mysql\mysql-8.0.30-winx64\bin\mysql.exe'


def spell_names():
    data = open(os.path.join(REPO, 'server', 'Data', 'dbc', 'Spell.dbc'), 'rb').read()
    _, records, fields, size, _ = struct.unpack('<4s4I', data[:20])
    strings = data[20 + records * size:]
    names = {}
    for row in range(records):
        offset = 20 + row * size
        values = struct.unpack('<%dI' % fields, data[offset:offset + size])
        for field in range(136, 152):
            if values[field]:
                end = strings.index(b'\0', values[field])
                raw = strings[values[field]:end]
                try:
                    names[values[0]] = raw.decode('utf-8')
                except UnicodeDecodeError:
                    names[values[0]] = raw.decode('cp1252', 'replace')
                break
    return names


def query(database, sql):
    out = subprocess.run([MYSQL, '-h127.0.0.1', '-P3307', '-uacore', '-pacore', '-N', '-B', database, '-e', sql],
                         capture_output=True, text=True, encoding='utf-8')
    return [line.split('\t') for line in out.stdout.splitlines() if line]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('rows')
    parser.add_argument('--spec', default='')
    parser.add_argument('--profile', default='')
    parser.add_argument('--top', type=int, default=8)
    arguments = parser.parse_args()

    rows = [row for row in csv.DictReader(open(arguments.rows, encoding='utf-8')) if row['Trusted'] == 'True']
    if arguments.spec:
        rows = [row for row in rows if row['Spec'].lower() == arguments.spec.lower()]
    if arguments.profile:
        rows = [row for row in rows if row['Profile'] == arguments.profile]

    # The runs and names a worker holds
    by_worker = defaultdict(set)
    for row in rows:
        by_worker[row['Context'].split('/')[0][1:]].add(row['Run'])
    spells = {}
    for worker, runs in by_worker.items():
        database = 'acore_sim%s_characters' % worker
        sql = ('SELECT s.run_id, c.name, s.spell_id, s.from_pet, s.casts, s.hits, s.crits, s.amount '
               'FROM mod_combat_bench_spell s JOIN characters c ON c.guid = s.guid '
               'WHERE s.kind = 1 AND s.run_id IN (%s)' % ','.join(sorted(runs)))
        for run, name, spell, pet, casts, hits, crits, amount in query(database, sql):
            spells.setdefault((worker, run, name), []).append(
                (int(spell), pet == '1', int(casts), int(hits), int(crits), int(amount)))

    names = spell_names()
    names[0] = 'Melee'
    groups = defaultdict(list)
    for row in rows:
        groups[(row['Profile'], row['Layout'], row['Spec'])].append(row)
    for (profile, layout, spec), members in sorted(groups.items()):
        total = defaultdict(lambda: [0, 0, 0, 0])
        damage = 0
        seconds = 0.0
        for row in members:
            for spell, pet, casts, hits, crits, amount in spells.get((row['Context'].split('/')[0][1:], row['Run'],
                                                                     row['Name']), []):
                entry = total[(spell, pet)]
                entry[0] += casts
                entry[1] += hits
                entry[2] += crits
                entry[3] += amount
                damage += amount
            seconds += float(row['Seconds'])
        dps = damage / seconds if seconds else 0.0
        print('\n== %s %s %s: %d bot(s), %.0f DPS ==' % (profile, layout, spec, len(members), dps))
        for (spell, pet), (casts, hits, crits, amount) in sorted(total.items(), key=lambda item: -item[1][3])[
                :arguments.top]:
            per_cast = '%.1f' % (hits / casts) if casts else '-'
            crit = 100.0 * crits / hits if hits else 0.0
            print('  %5.1f%%  %-34s %s %6d casts %7d hits  %5s/cast  %4.0f%% crit' % (
                100.0 * amount / damage if damage else 0.0, names.get(spell, str(spell))[:34], 'pet' if pet else '   ',
                casts, hits, per_cast, crit))


if __name__ == '__main__':
    main()
