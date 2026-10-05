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
    models    [ "<archive path of a model>" or { "model": <that>, "tint": "#rrggbb" }, ... ] - models shipped with
              their skins and textures and no row behind them (a creature's look, which creature displays name), as
              they are or as a tinted copy (see below)
    sounds    { "<key>": { "clone": <Ascension SoundEntries id>, "files": [ "<wav, relative to the repository>" ],
              "volume": v, "minDistance": d, "cutoff": d, "normalize": <dBFS>, "id": <its own id> } } - sounds of
              our own (not Ascension's): a SoundEntries row copying clone's (its type: a creature's loop is 27, as
              1453 WaterElementalLoop), playing one of the files at random, shipped in soundFolder (an archive
              folder), each file brought to that RMS level if normalize is given (its peaks limited under -1 dBFS:
              Diablo IV's loot sounds, mixed for its own engine, are barely heard in the game's); a kit names one
              as "@<key>", and visuals.json lists every one's id under "sounds". A sound the server plays belongs
              to our own sound engine instead (localTools/audio/buildAudio.py, .agents/docs/systems/evolutions-audio.md)

A kit is an Ascension SpellVisualKit id (brought as it is) or { "from": <kit id>, "anim": <animation id>,
"effects": { "<attachment>": <effect> }, "sound": <sound>, "shake": <stock CameraShakes id>, "fields": { "<field>":
<value> }, "id": <its own id> }: the kit named by from (none: an empty one) with what is given replaced, an effect
given null removed; fields sets raw SpellVisualKit fields (a decimal value is written as a float: the CharProc
parameters, 21-36). Attachments: head, chest, base, lefthand,
righthand, hands (both), breath, leftweapon, rightweapon, weapons (both), special1-3, world. An effect is an Ascension
SpellVisualEffectName id or a model's file name without its folder and extension (the row at scale 1 if there is
one), or { "model": <that>, "scale": <factor>, "tint": "#rrggbb" } for a copy of it drawn that much larger or smaller
(an effect made for a raid boss, shrunk to a player's spell) and/or coloured: tinted, the model gets its own copy
(Spells/Evolutions/Tint/<name>_<rrggbb>) whose animated colours and textures all take that hue, whites included, each
keeping its brightness (a white light beam made a rarity's colour); a sound an Ascension SoundEntries id or name.

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
import colorsys
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
# The BLP reader and writer the Oathblade's blue effects use, for the tinted textures
spec = importlib.util.spec_from_file_location('blp', os.path.join(REPO, 'localTools', 'interface', 'spike', 'blp.py'))
BLP = importlib.util.module_from_spec(spec)
spec.loader.exec_module(BLP)
spec = importlib.util.spec_from_file_location('blp_writer', os.path.join(REPO, 'localTools', 'interface',
                                                                          'buildParagonArt.py'))
BLP_WRITER = importlib.util.module_from_spec(spec)
spec.loader.exec_module(BLP_WRITER)

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
        # Tinted copies to make once the models are read: {copy's model path: (source model path, rgb)}
        self.tints = {}
        # Sounds of our own: {key: SoundEntries id}, and their files {archive path: source path}
        self.own_sounds = {}
        self.own_files = {}
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

    def own_sound(self, key):
        """the SoundEntries row of one of the config's own sounds (see the module's doc), made once"""
        if key in self.own_sounds:
            return self.own_sounds[key]
        spec = self.config.get('sounds', {}).get(key)
        if spec is None:
            raise SystemExit(f'No sound {key} in the config\'s sounds')
        table = self.source['SoundEntries']
        if spec['clone'] not in table.rows:
            raise SystemExit(f'SoundEntries {spec["clone"]} is not in the Ascension client')
        files = spec['files']
        if not 1 <= len(files) <= 10:
            raise SystemExit(f'Sound {key}: one to ten files')
        entry = self.new_row('SoundEntries', table.rows[spec['clone']], spec.get('id'))
        fields = entry['fields']
        fields[SOUND_ADVANCED] = 0
        folder = archive_path(self.config['soundFolder'])
        entry['strings'] = {'2': key, '23': folder}
        for index in range(10):
            fields[13 + index] = 1 if index < len(files) else 0
            entry['strings'][str(3 + index)] = ntpath.basename(archive_path(files[index])) if index < len(files) else ''
        for index, source in enumerate(files):
            path = os.path.join(REPO, source)
            if not os.path.isfile(path):
                raise SystemExit(f'Sound {key}: {source} not found')
            self.own_files[f'{folder}\\{ntpath.basename(archive_path(source))}'] = (path, spec.get('normalize'))
        for name, field in (('volume', 24), ('minDistance', 26), ('cutoff', 27)):
            if name in spec:
                fields[field] = struct.unpack('<I', struct.pack('<f', float(spec[name])))[0]
        self.own_sounds[key] = entry['id']
        return entry['id']

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
        own_sound = spec.get('sound') if str(spec.get('sound', '')).startswith('@') else None
        if own_sound:
            fields[KIT_SOUND] = 0
        elif 'sound' in spec:
            fields[KIT_SOUND] = self.find_sound(spec['sound'])
        if 'shake' in spec:
            fields[KIT_SHAKE] = spec['shake'] or 0
        for field, value in spec.get('fields', {}).items():
            fields[int(field)] = struct.unpack('<I', struct.pack('<f', value))[0] if isinstance(value, float) else value
        entry = self.new_row('SpellVisualKit', fields, spec.get('id'))
        self.rewrite_kit(entry, source_id)
        if own_sound:
            entry['fields'][KIT_SOUND] = self.own_sound(own_sound[1:])
        for field, effect in scaled.items():
            entry['fields'][field] = self.scaled_effect(effect)
        return entry['id']

    def scaled_effect(self, spec):
        """a copy of an imported effect row drawn `scale` times its size (SpellVisualEffectName's scale and the range it
        is clamped to), and/or pointing at a `tint`ed copy of its model"""
        base_id = self.effect(self.find_effect(spec['model']))
        base = next(row for row in self.rows['SpellVisualEffectName'] if row['id'] == base_id)
        fields = list(base['fields'])
        if 'scale' in spec:
            scale = float(spec['scale'])
            bits = struct.unpack('<I', struct.pack('<f', scale))[0]
            fields[4] = bits
            fields[5] = struct.unpack('<I', struct.pack('<f', min(scale, 0.01)))[0]
            fields[6] = struct.unpack('<I', struct.pack('<f', max(scale, 10.0)))[0]
        entry = self.new_row('SpellVisualEffectName', fields)
        entry['strings'] = dict(base['strings'])
        if 'tint' in spec:
            source = archive_path(base['strings']['2'])
            entry['strings']['2'] = self.tinted_model(source, spec['tint'])[:-3] + ntpath.splitext(source)[1]
        return entry['id']

    def tinted_model(self, source, tint):
        """the path of a model's copy coloured `tint` (#rrggbb), made once the models are read"""
        colour = tint.lstrip('#').lower()
        rgb = tuple(int(colour[index:index + 2], 16) / 255 for index in (0, 2, 4))
        stem = ntpath.splitext(ntpath.basename(archive_path(source)))[0]
        copy = f'Spells\\Evolutions\\Tint\\{stem}_{colour}.m2'
        self.models.add(model_path(source))
        self.tints[copy] = (model_path(source), rgb, colour)
        return copy

    def tinted_files(self, models):
        """the tinted copies: each model with its animated colours and its textures coloured, its skins renamed"""
        files = {}
        for copy, (source, rgb, colour) in sorted(self.tints.items()):
            if source not in models:
                raise SystemExit(f'{source} was not read: it cannot be tinted')
            model = m2.M2(bytearray(models[source]))
            for name in model.particle_models():
                print(f'  warning: {source} particles draw {name}, which keeps its own colours')
            for offset, scale in model.colour_slots():
                model.set_colour(offset, tint_colour(tuple(c / scale for c in model.colour(offset)), rgb, scale))
            for record, kind, name in model.textures():
                if kind != 0 or not name:
                    continue
                texture = archive_path(name)
                if texture not in models:
                    raise SystemExit(f'{texture} (named by {source}) was not read: it cannot be tinted')
                tinted = f'Spells\\Evolutions\\Tint\\{ntpath.splitext(ntpath.basename(texture))[0]}_{colour}.blp'
                if tinted not in files:
                    files[tinted] = tint_texture(models[texture], rgb)
                model.rename_texture(record, tinted)
            files[copy] = bytes(model.data)
            for index in range(model.skin_count):
                skin = f'{source[:-3]}{index:02d}.skin'
                files[f'{copy[:-3]}{index:02d}.skin'] = models[skin]
        return files

    def composed_visual(self, spec):
        source_id = spec['from']
        if source_id not in self.source['SpellVisual'].rows:
            raise SystemExit(f'SpellVisual {source_id} is not in the Ascension client')
        source = self.source['SpellVisual'].rows[source_id]
        unknown = set(spec) - set(SLOTS) - {'from', 'missile', 'motion'}
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
        # The missile's path: the source's, or a SpellMissileMotion row named by id (a stock arc: 40 Fountain)
        fields[VISUAL_MOTION] = self.motion(spec['motion'] if 'motion' in spec else source[VISUAL_MOTION])
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


def tint_colour(rgb, target, scale=1.0):
    """an M2 colour (0-1 once divided by scale) given the target's hue and saturation, keeping its brightness"""
    hue, saturation, _ = colorsys.rgb_to_hsv(*target)
    value = min(1.0, max(rgb))
    return tuple(c * scale for c in colorsys.hsv_to_rgb(hue, saturation, value))


def tint_texture(data, target):
    """a texture coloured with the target's hue: brightness and alpha kept, the brightest pixels a little paler so a
    beam keeps a light core"""
    import numpy
    from PIL import Image
    with tempfile.TemporaryDirectory() as work:
        source = os.path.join(work, 'in.blp')
        with open(source, 'wb') as file:
            file.write(data)
        image = BLP.read_blp(source).convert('RGBA')
        hue, saturation, _ = colorsys.rgb_to_hsv(*target)
        alpha = image.getchannel('A')
        hsv = numpy.asarray(image.convert('RGB').convert('HSV')).astype(numpy.float32) / 255
        value = hsv[..., 2]
        hsv[..., 0] = hue
        hsv[..., 1] = saturation * (1.0 - 0.45 * value ** 3)
        result = Image.fromarray(numpy.round(hsv * 255).astype(numpy.uint8), 'HSV').convert('RGB')
        result.putalpha(alpha)
        target_path = os.path.join(work, 'out.blp')
        if result.width * result.height <= 256 * 256:
            BLP_WRITER.writeRawBlp(result, target_path)
        else:
            BLP_WRITER.writeDxt3Blp(result, target_path)
        with open(target_path, 'rb') as file:
            return file.read()


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


LIMIT_CEILING = 10 ** (-1.0 / 20)
LIMIT_LOOKAHEAD = 0.003
LIMIT_RELEASE = 0.060


def normalize_sound(source, target, level):
    """the WAV at source written to target at that RMS level (dBFS): raised as a whole, then its peaks held under
    -1 dBFS by a look-ahead limiter (the gain each sample needs, taken over the next 3 ms and released over 60 ms) -
    so a quiet sound with sharp transients still gets loud, not merely as loud as its loudest peak allowed"""
    import numpy
    import soundfile
    from scipy.ndimage import maximum_filter1d, minimum_filter1d, uniform_filter1d
    samples, rate = soundfile.read(source, dtype='float32', always_2d=True)
    rms = float(numpy.sqrt(numpy.mean(samples ** 2)))
    if rms <= 0.0:
        shutil.copyfile(source, target)
        return
    for _ in range(3):
        samples = samples * 10 ** ((level - 20 * numpy.log10(float(numpy.sqrt(numpy.mean(samples ** 2))))) / 20)
        envelope = numpy.max(numpy.abs(samples), axis=1)
        lookahead = max(1, int(rate * LIMIT_LOOKAHEAD))
        envelope = maximum_filter1d(envelope, size=2 * lookahead + 1)
        gain = numpy.minimum(1.0, LIMIT_CEILING / numpy.maximum(envelope, 1e-9))
        release = max(1, int(rate * LIMIT_RELEASE))
        gain = uniform_filter1d(minimum_filter1d(gain, size=release), size=release)
        samples = samples * gain[:, None]
    samples = numpy.clip(samples, -LIMIT_CEILING, LIMIT_CEILING)
    soundfile.write(target, samples if samples.shape[1] > 1 else samples[:, 0], rate, subtype='PCM_16')


def main(config_path):
    with open(config_path, encoding='utf-8') as source:
        config = json.load(source)
    output = os.path.join(REPO, config['output'])
    with tempfile.TemporaryDirectory() as work:
        importer = Importer(config, work)
        ids = {key: importer.visual(source_id) for key, source_id in config.get('visuals', {}).items()}
        ids.update({key: importer.composed_visual(spec) for key, spec in config.get('looks', {}).items()})
        kits = {key: importer.composed_kit(spec) for key, spec in config.get('kits', {}).items()}
        own_sounds = {key: importer.own_sound(key) for key in config.get('sounds', {})}
        for spec in config.get('models', []):
            if isinstance(spec, dict):
                importer.tinted_model(spec['model'], spec['tint'])
            else:
                importer.models.add(model_path(spec))
        for table, rows in importer.rows.items():
            numbers = [row['id'] for row in rows]
            if len(numbers) != len(set(numbers)):
                raise SystemExit(f"{table}: a kit's own id is one the import numbered too; give it a higher one")
        models, missing_models = importer.model_files()
        models.update(importer.tinted_files(models))
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
        for name, (source, level) in sorted(importer.own_files.items()):
            target = local(files_root, name)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if level is None:
                shutil.copyfile(source, target)
            else:
                normalize_sound(source, target, float(level))
            shipped += 1

        result = {'ids': ids, 'kits': kits, 'sounds': own_sounds,
                  'tables': {table: importer.rows[table] for table in TABLES}}
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
