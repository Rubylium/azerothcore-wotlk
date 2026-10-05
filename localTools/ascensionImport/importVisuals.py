"""Imports spell visuals from the Ascension client: the visual rows, their kits, effect models and sounds, and every
file they need that the stock 3.3.5 client lacks.

    python importVisuals.py <config.json>

config.json (see localTools/barbarian/ascensionVisuals.json):
    output    folder, relative to the repository, for visuals.json and files/
    idBase    first id of every table's imported rows (each table numbers its own from there)
    visuals   { "<key>": <Ascension SpellVisual id>, ... } - the looks to bring over as they are
    looks     { "<key>": { "from": <Ascension SpellVisual id>, "<slot>": <kit>, ... } } - looks put together: the
              visual named by from, each slot given replaced (a slot given null is emptied). Slots: precast, cast,
              impact, state, statedone, channel, casterimpact, targetimpact, missiletarget, instantarea, impactarea,
              persistentarea; "missile" replaces the missile's model (an effect, below)
    kits      { "<key>": <kit> } - kits of their own, for a server to play on a unit (SMSG_PLAY_SPELL_VISUAL);
              each one carries its "id", which the module's code names

A kit is an Ascension SpellVisualKit id (brought as it is) or { "from": <kit id>, "anim": <animation id>,
"effects": { "<attachment>": <effect> }, "sound": <sound>, "shake": <stock CameraShakes id>, "fields": { "<field>":
<value> }, "id": <its own id> }: the kit named by from (none: an empty one) with what is given replaced, an effect
given null removed; fields sets raw SpellVisualKit fields (a decimal value is written as a float: the CharProc
parameters, 21-36). Attachments: head, chest, base, lefthand,
righthand, hands (both), breath, leftweapon, rightweapon, weapons (both), special1-3, world. An effect is an Ascension
SpellVisualEffectName id or a model's file name without its folder and extension (the row at scale 1 if there is
one), or { "model": <that>, "scale": <factor> } for a copy of it drawn that much larger or smaller (an effect made for
a raid boss, shrunk to a player's spell); a sound an Ascension SoundEntries id or name.

Ascension's data is stock-layout 3.3.5 (its tables load as ours do, its models are version 264), and the effects it
plays are mostly retail ones it converted; the import keeps them as they are. Written:

- files/<archive path>: each model (.m2 with its .skin and .anim files), texture and sound the visuals use that the
  stock client does not have, at the path the data names it by, so nothing inside a model needs renaming. Sounds in
  OGG, which the 3.3.5 client does not play, become PCM WAV next to where they were.
- visuals.json: { "ids": { "<key>": <SpellVisual id> }, "tables": { "<table>": [ { "id", "fields", "strings" } ] } },
  every row renumbered from idBase and its references to the other imported rows renumbered with it. A reference
  to something the import does not bring (a camera shake, an animation the 3.3.5 client lacks) is cleared.
  localTools/patchSinisterStrike.ps1 appends the rows; a spell names its look with the key's id.

The Ascension client is read where ASCENSION_ROOT points (default D:\\ascension-live), the stock one at
STOCK_CLIENT (default the CleanWOTLK folder). Run it again after changing the config: the output is rebuilt whole.
"""
import importlib.util
import json
import ntpath
import os
import shutil
import struct
import subprocess
import sys
import tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ARCHIVES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ascensionArchives.js')
STOCK_DBC = os.path.join(REPO, 'server', 'Data', 'dbc')

spec = importlib.util.spec_from_file_location('m2', os.path.join(REPO, 'localTools', 'oathblade', 'm2.py'))
m2 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m2)

TABLES = ['SpellVisual', 'SpellVisualKit', 'SpellVisualEffectName', 'SoundEntries', 'SpellVisualKitModelAttach',
          'SpellMissileMotion']
# Which fields of each table hold a string (an offset into its string block)
STRING_FIELDS = {
    'SpellVisualEffectName': (1, 2),
    'SoundEntries': (2, *range(3, 13), 23),
    'SpellMissileMotion': (1, 2),
}
# SpellVisual: the kit slots (precast, cast, impact, state, state done, channel, caster impact, target impact,
# missile targeting, instant area, impact area, persistent area), the missile's model and motion, its sounds
VISUAL_KITS = (1, 2, 3, 4, 5, 6, 14, 15, 22, 23, 24, 25)
VISUAL_MISSILE_MODEL = 8
VISUAL_SOUNDS = (11, 12)
VISUAL_MOTION = 21
# SpellVisualKit: the two animations, the effect models by attachment (head ... special, world), sound, camera shake
KIT_ANIMATIONS = (1, 2)
KIT_EFFECTS = tuple(range(3, 15))
KIT_SOUND = 15
KIT_SHAKE = 16
KIT_CHAR_PROCS = (17, 18, 19, 20)
KIT_FIELDS = 38
# SoundEntries: its advanced settings row
SOUND_ADVANCED = 29
NONE = 0xFFFFFFFF
SLOTS = {'precast': 1, 'cast': 2, 'impact': 3, 'state': 4, 'statedone': 5, 'channel': 6, 'casterimpact': 14,
         'targetimpact': 15, 'missiletarget': 22, 'instantarea': 23, 'impactarea': 24, 'persistentarea': 25}
ATTACHMENTS = {'head': (3,), 'chest': (4,), 'base': (5,), 'lefthand': (6,), 'righthand': (7,), 'hands': (6, 7),
               'breath': (8,), 'leftweapon': (9,), 'rightweapon': (10,), 'weapons': (9, 10), 'special1': (11,),
               'special2': (12,), 'special3': (13,), 'world': (14,)}


class Dbc:
    def __init__(self, path):
        data = open(path, 'rb').read()
        magic, rows, self.field_count, self.record_size, strings = struct.unpack_from('<4sIIII', data, 0)
        if magic != b'WDBC':
            raise ValueError(f'{path} is not a DBC')
        self.data = data
        self.string_base = 20 + rows * self.record_size
        self.rows = {}
        for index in range(rows):
            fields = list(struct.unpack_from(f'<{self.field_count}I', data, 20 + index * self.record_size))
            self.rows[fields[0]] = fields

    def string(self, offset):
        start = self.string_base + offset
        return self.data[start:self.data.index(b'\0', start)].decode('utf-8', 'replace')

    @property
    def max_id(self):
        return max(self.rows)


def node(*args):
    subprocess.run(['node', ARCHIVES, *args], check=True)


def archive_path(name):
    return name.replace('/', '\\')


def model_path(name):
    """a model as the archives hold it: effect rows name the .mdx the 3.3.5 client swaps for its .m2"""
    stem, extension = ntpath.splitext(archive_path(name))
    return stem + '.m2' if extension.lower() in ('.mdx', '.mdl', '.m2') else archive_path(name)


def extract(paths, directory):
    if not paths:
        return set()
    os.makedirs(directory, exist_ok=True)
    list_path = os.path.join(directory, 'list.json')
    with open(list_path, 'w', encoding='utf-8') as output:
        json.dump(sorted(set(paths)), output)
    node('files', list_path, directory)
    with open(os.path.join(directory, 'missing.json'), encoding='utf-8') as missing:
        return set(json.load(missing))


def local(directory, name):
    return os.path.join(directory, *name.split('\\'))


class Importer:
    def __init__(self, config, work):
        self.config = config
        self.work = work
        node('dbc', os.path.join(work, 'dbc'), *TABLES)
        self.source = {name: Dbc(os.path.join(work, 'dbc', f'{name}.dbc')) for name in TABLES}
        self.stock_max = {name: Dbc(os.path.join(STOCK_DBC, f'{name}.dbc')).max_id for name in
                          ('AnimationData', 'CameraShakes')}
        self.next_id = {name: config['idBase'] for name in TABLES}
        self.ids = {name: {} for name in TABLES}
        self.rows = {name: [] for name in TABLES}
        self.models = set()
        self.sounds = set()
        self.attach_by_kit = {}
        for row in self.source['SpellVisualKitModelAttach'].rows.values():
            self.attach_by_kit.setdefault(row[1], []).append(row[0])

    def add_row(self, table, source_id, rewrite):
        """the imported copy of a row, made once; rewrite(fields) renumbers its references"""
        if source_id in self.ids[table]:
            return self.ids[table][source_id]
        row = self.source[table].rows.get(source_id)
        if row is None:
            return 0
        new_id = self.next_id[table]
        self.next_id[table] += 1
        self.ids[table][source_id] = new_id
        fields = list(row)
        fields[0] = new_id
        strings = {str(field): self.source[table].string(fields[field]) for field in STRING_FIELDS.get(table, ())}
        entry = {'id': new_id, 'source': source_id, 'fields': fields, 'strings': strings}
        self.rows[table].append(entry)
        rewrite(entry)
        return new_id

    def sound(self, source_id):
        if not source_id or source_id == NONE:
            return source_id

        def rewrite(entry):
            entry['fields'][SOUND_ADVANCED] = 0
            directory = archive_path(entry['strings']['23'])
            for field in range(3, 13):
                name = entry['strings'][str(field)]
                if not name:
                    continue
                path = f'{directory}\\{name}' if directory else name
                self.sounds.add(path)
                if name.lower().endswith('.ogg'):
                    entry['strings'][str(field)] = name[:-4] + '.wav'
        return self.add_row('SoundEntries', source_id, rewrite)

    def effect(self, source_id):
        if not source_id or source_id == NONE:
            return source_id

        def rewrite(entry):
            self.models.add(model_path(entry['strings']['2']))
        return self.add_row('SpellVisualEffectName', source_id, rewrite)

    def motion(self, source_id):
        if not source_id or source_id == NONE:
            return source_id
        return self.add_row('SpellMissileMotion', source_id, lambda entry: None)

    def kit(self, source_id):
        if not source_id or source_id == NONE:
            return source_id
        return self.add_row('SpellVisualKit', source_id, lambda entry: self.rewrite_kit(entry, source_id))

    def rewrite_kit(self, entry, source_id):
        """a kit's references renumbered to the imported rows, what the 3.3.5 client lacks cleared"""
        fields = entry['fields']
        for field in KIT_ANIMATIONS:
            if fields[field] != NONE and fields[field] > self.stock_max['AnimationData']:
                fields[field] = NONE
        for field in KIT_EFFECTS:
            fields[field] = self.effect(fields[field])
        fields[KIT_SOUND] = self.sound(fields[KIT_SOUND])
        if fields[KIT_SHAKE] > self.stock_max['CameraShakes']:
            fields[KIT_SHAKE] = 0
        for attach in self.attach_by_kit.get(source_id, []):
            self.add_row('SpellVisualKitModelAttach', attach, self.attach_rewriter(entry['id']))

    def find_effect(self, spec):
        """an effect named by id or by its model's file name: the row at scale 1, else the first"""
        if spec is None or isinstance(spec, int):
            return spec or 0
        table = self.source['SpellVisualEffectName']
        found = sorted(row for row in table.rows.values()
                       if ntpath.splitext(ntpath.basename(table.string(row[2])))[0].lower() == spec.lower())
        if not found:
            raise SystemExit(f'No effect model named {spec} in the Ascension client')
        at_scale = [row for row in found if struct.unpack('<f', struct.pack('<I', row[4]))[0] == 1.0]
        return (at_scale or found)[0][0]

    def find_sound(self, spec):
        if spec is None or isinstance(spec, int):
            return spec or 0
        table = self.source['SoundEntries']
        found = sorted(row[0] for row in table.rows.values() if table.string(row[2]).lower() == spec.lower())
        if not found:
            raise SystemExit(f'No sound named {spec} in the Ascension client')
        return found[0]

    def new_row(self, table, fields, own_id=None):
        """a row of the import's own (no Ascension row behind it), numbered next or with the id it asks for"""
        if own_id is None:
            own_id = self.next_id[table]
            self.next_id[table] += 1
        fields = list(fields)
        fields[0] = own_id
        entry = {'id': own_id, 'source': None, 'fields': fields, 'strings': {}}
        self.rows[table].append(entry)
        return entry

    def composed_kit(self, spec):
        """a kit as the config gives it: an Ascension kit id, null, or one put together (see the module's doc)"""
        if spec is None:
            return 0
        if isinstance(spec, int):
            return self.kit(spec)
        source_id = spec.get('from')
        if source_id:
            if source_id not in self.source['SpellVisualKit'].rows:
                raise SystemExit(f'SpellVisualKit {source_id} is not in the Ascension client')
            fields = list(self.source['SpellVisualKit'].rows[source_id])
        else:
            fields = [0] * KIT_FIELDS
            for field in KIT_ANIMATIONS + KIT_CHAR_PROCS:
                fields[field] = NONE
        if 'anim' in spec:
            fields[KIT_ANIMATIONS[1]] = NONE if spec['anim'] is None else spec['anim']
        scaled = {}
        for attachment, effect in spec.get('effects', {}).items():
            for field in ATTACHMENTS[attachment]:
                if isinstance(effect, dict):
                    # Imported after the others: the kit's fields hold Ascension ids until rewrite_kit renumbers them
                    fields[field] = 0
                    scaled[field] = effect
                else:
                    fields[field] = self.find_effect(effect)
        if 'sound' in spec:
            fields[KIT_SOUND] = self.find_sound(spec['sound'])
        if 'shake' in spec:
            fields[KIT_SHAKE] = spec['shake'] or 0
        for field, value in spec.get('fields', {}).items():
            fields[int(field)] = struct.unpack('<I', struct.pack('<f', value))[0] if isinstance(value, float) else value
        entry = self.new_row('SpellVisualKit', fields, spec.get('id'))
        self.rewrite_kit(entry, source_id)
        for field, effect in scaled.items():
            entry['fields'][field] = self.scaled_effect(effect)
        return entry['id']

    def scaled_effect(self, spec):
        """a copy of an imported effect row drawn `scale` times its size (SpellVisualEffectName's scale and the range it
        is clamped to)"""
        base_id = self.effect(self.find_effect(spec['model']))
        base = next(row for row in self.rows['SpellVisualEffectName'] if row['id'] == base_id)
        fields = list(base['fields'])
        scale = float(spec['scale'])
        bits = struct.unpack('<I', struct.pack('<f', scale))[0]
        fields[4] = bits
        fields[5] = struct.unpack('<I', struct.pack('<f', min(scale, 0.01)))[0]
        fields[6] = struct.unpack('<I', struct.pack('<f', max(scale, 10.0)))[0]
        entry = self.new_row('SpellVisualEffectName', fields)
        entry['strings'] = dict(base['strings'])
        return entry['id']

    def composed_visual(self, spec):
        source_id = spec['from']
        if source_id not in self.source['SpellVisual'].rows:
            raise SystemExit(f'SpellVisual {source_id} is not in the Ascension client')
        source = self.source['SpellVisual'].rows[source_id]
        unknown = set(spec) - set(SLOTS) - {'from', 'missile'}
        if unknown:
            raise SystemExit(f'Unknown look slots {sorted(unknown)}')
        entry = self.new_row('SpellVisual', source)
        fields = entry['fields']
        for name, field in SLOTS.items():
            fields[field] = self.composed_kit(spec[name]) if name in spec else self.kit(source[field])
        missile = self.find_effect(spec['missile']) if 'missile' in spec else source[VISUAL_MISSILE_MODEL]
        fields[VISUAL_MISSILE_MODEL] = self.effect(missile)
        for field in VISUAL_SOUNDS:
            fields[field] = self.sound(source[field])
        fields[VISUAL_MOTION] = self.motion(source[VISUAL_MOTION])
        return entry['id']

    def attach_rewriter(self, kit_id):
        def rewrite(entry):
            entry['fields'][1] = kit_id
            entry['fields'][2] = self.effect(entry['fields'][2])
        return rewrite

    def visual(self, source_id):
        def rewrite(entry):
            fields = entry['fields']
            for field in VISUAL_KITS:
                fields[field] = self.kit(fields[field])
            fields[VISUAL_MISSILE_MODEL] = self.effect(fields[VISUAL_MISSILE_MODEL])
            for field in VISUAL_SOUNDS:
                fields[field] = self.sound(fields[field])
            fields[VISUAL_MOTION] = self.motion(fields[VISUAL_MOTION])
        new_id = self.add_row('SpellVisual', source_id, rewrite)
        if not new_id:
            raise SystemExit(f'SpellVisual {source_id} is not in the Ascension client')
        return new_id

    def model_files(self):
        """every model, with its skins, animation files, textures and the models its particles draw"""
        pending = set(self.models)
        seen = set()
        textures = set()
        files = {}
        missing = set()
        while pending:
            batch = sorted(pending - seen)
            pending = set()
            if not batch:
                break
            seen.update(batch)
            directory = os.path.join(self.work, 'models')
            missing |= extract(batch, directory)
            companions = []
            for path in batch:
                if path in missing:
                    continue
                data = open(local(directory, path), 'rb').read()
                files[path] = data
                model = m2.M2(data)
                stem = path[:-3]
                companions += [f'{stem}{index:02d}.skin' for index in range(model.skin_count)]
                companions += [f'{stem}{animation:04d}-{sub:02d}.anim' for animation, sub in
                               model.external_sequences()]
                textures |= {archive_path(name) for _, kind, name in model.textures() if kind == 0 and name}
                pending |= {model_path(name) for name in model.particle_models()}
            missing |= extract(companions, directory)
            for name in companions:
                if name not in missing:
                    files[name] = open(local(directory, name), 'rb').read()
        texture_directory = os.path.join(self.work, 'textures')
        missing |= extract(sorted(textures), texture_directory)
        for name in textures - missing:
            files[name] = open(local(texture_directory, name), 'rb').read()
        return files, missing

    def sound_files(self):
        directory = os.path.join(self.work, 'sounds')
        missing = extract(sorted(self.sounds), directory)
        return {name: open(local(directory, name), 'rb').read() for name in self.sounds - missing}, missing


def write_sound(target, data, name):
    if not name.lower().endswith('.ogg'):
        with open(target, 'wb') as output:
            output.write(data)
        return
    import io
    import numpy
    import soundfile
    samples, rate = soundfile.read(io.BytesIO(data), dtype='float32')
    samples = numpy.nan_to_num(samples)
    soundfile.write(target[:-4] + '.wav', samples, rate, subtype='PCM_16')


def main(config_path):
    with open(config_path, encoding='utf-8') as source:
        config = json.load(source)
    output = os.path.join(REPO, config['output'])
    with tempfile.TemporaryDirectory() as work:
        importer = Importer(config, work)
        ids = {key: importer.visual(source_id) for key, source_id in config.get('visuals', {}).items()}
        ids.update({key: importer.composed_visual(spec) for key, spec in config.get('looks', {}).items()})
        kits = {key: importer.composed_kit(spec) for key, spec in config.get('kits', {}).items()}
        for table, rows in importer.rows.items():
            numbers = [row['id'] for row in rows]
            if len(numbers) != len(set(numbers)):
                raise SystemExit(f"{table}: a kit's own id is one the import numbered too; give it a higher one")
        models, missing_models = importer.model_files()
        sounds, missing_sounds = importer.sound_files()

        stock_list = os.path.join(work, 'stock-list.json')
        stock_out = os.path.join(work, 'stock.json')
        with open(stock_list, 'w', encoding='utf-8') as listing:
            json.dump(sorted(set(models) | set(sounds)), listing)
        node('stock', stock_list, stock_out)
        with open(stock_out, encoding='utf-8') as listing:
            stock = set(json.load(listing))

        shutil.rmtree(output, ignore_errors=True)
        files_root = os.path.join(output, 'files')
        shipped = 0
        for name, data in sorted({**models, **sounds}.items()):
            if name in stock:
                continue
            target = local(files_root, name)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if name in sounds:
                write_sound(target, data, name)
            else:
                with open(target, 'wb') as file:
                    file.write(data)
            shipped += 1

        result = {'ids': ids, 'kits': kits, 'tables': {table: importer.rows[table] for table in TABLES}}
        with open(os.path.join(output, 'visuals.json'), 'w', encoding='utf-8') as file:
            json.dump(result, file, indent=1, ensure_ascii=False)
            file.write('\n')
    counts = ', '.join(f'{len(rows)} {table}' for table, rows in result['tables'].items())
    print(f'{len(ids)} visuals, {len(kits)} kits: {counts}; {shipped} files shipped, {len(stock)} already stock')
    for name in sorted(missing_models | missing_sounds):
        print(f'  missing from the Ascension client: {name}')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
