"""Shaped effects: a 3.3.5 effect model (an M2 of particle emitters) rebuilt to a fight's own form - a cone, a line,
a ring - keeping its look (its textures, colours, timing, motion): only where its particles go changes.

    python localTools/shapedEffects/buildShapedEffects.py

shapedEffects.json lists them: { "key", "source" (an M2 in the repository, with its .skin next to it), "form",
"kit" }. Each is written to client-assets/files/Spells/Evolutions/Shaped/<key>.m2 (+ <key>00.skin), shipped with the
module's files; localTools/patchSinisterStrike.ps1 gives each an effect row and the kit id given, which a fight plays
where its form lands (GroundIndicators::PlayKit, facing the form's axis).

Forms (yards; the model at scale 1):
- {"cone": {"arc": degrees, "radius": r}}: the particles thrown from the apex, spread over the arc only, reaching r
  in their lifespan.
- {"line": {"length": l, "width": w}}: every emitter a flat sheet over the strip (from the origin along the axis),
  the particles rising from it, as many more as the strip is larger.
- {"ring": {"inner": a, "outer": b}}: the particles born on the circle of radius a, thrown out flat to b.

The model's axis: its emitters' horizontal range is centred on its +Y (measured in the FX lab: a 90 degree cone
played on a creature facing south sprayed east), so a form's axis is +Y and the server turns a kit a quarter turn
clockwise from the form's facing (ForgeMaster ShapedImpact).

M2Particle (WotLK, 476 bytes; wowdev "M2"): emitter type at +41 (1 plane, 2 sphere), M2Track<float>s: emission speed
+52, speed variation +72, vertical range +92, horizontal range +112, gravity +132, lifespan +152, emission rate +176,
emission area length +200, width +220, z source +240 (lifespan and rate variations, floats, at +172 and +196); drag
(float) +372.
"""
import importlib.util
import json
import math
import os
import shutil
import struct

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CONFIG = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'shapedEffects.json')
spec = importlib.util.spec_from_file_location('m2', os.path.join(REPO, 'localTools', 'oathblade', 'm2.py'))
m2 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m2)

SPEED = 52
SPEED_VARIATION = 72
VERTICAL = 92
HORIZONTAL = 112
GRAVITY = 132
LIFESPAN = 152
RATE = 176
AREA_LENGTH = 200
AREA_WIDTH = 220
PLANE = 1
SPHERE = 2
POSITION = 8
DRAG = 372


def track_values(model, track):
    """offsets of every float a full M2Track<float> holds, across its sequences"""
    count, offset = model.array(track + 12)
    found = []
    for sequence in range(count):
        values, target = model.array(offset + sequence * 8)
        found += [target + 4 * index for index in range(values)]
    return found


def floats(model, track):
    return [struct.unpack_from('<f', model.data, offset)[0] for offset in track_values(model, track)]


def set_track(model, track, value):
    for offset in track_values(model, track):
        struct.pack_into('<f', model.data, offset, value)


def scale_track(model, track, factor):
    for offset in track_values(model, track):
        struct.pack_into('<f', model.data, offset, struct.unpack_from('<f', model.data, offset)[0] * factor)


# A thrown particle's band round the horizon (radians either side of it): flat along the ground; and what is left of
# its gravity (a burst's falls back before it travels)
CONE_VERTICAL = 0.3
CONE_GRAVITY = 0.3
CONE_DENSITY = 6.0
SPRITE_SPILL = 1.5
LINE_YARDS_PER_BURST = 16.0
LINE_DENSITY_MAX = 5.0


def reach(model, record):
    """how far the emitter throws at most: its fastest particle (speed times one plus its variation) over its life,
    slowed by its drag (speed falling as e^-drag.t: v/drag (1 - e^-drag.life))"""
    lifespans = [value for value in floats(model, record + LIFESPAN) if value > 0]
    speeds = [value for value in floats(model, record + SPEED) if value > 0]
    if not lifespans or not speeds:
        return 0.0
    variation = max(floats(model, record + SPEED_VARIATION) or [0.0])
    drag = struct.unpack_from('<f', model.data, record + DRAG)[0]
    life = max(lifespans)
    travel = life if drag <= 0 else (1.0 - math.exp(-drag * life)) / drag
    return max(speeds) * (1.0 + variation) * travel


def cone(model, arc, radius, vertical=None):
    """every emitter spread over the arc only (around the model's facing); the thrown ones flat along the ground,
    reaching radius"""
    half = math.radians(arc) / 2.0
    for record in model.records('particles', m2.PARTICLE_SIZE):
        set_track(model, record + HORIZONTAL, half)
        if model.data[record + 41] != SPHERE:
            continue
        set_track(model, record + VERTICAL, CONE_VERTICAL if vertical is None else vertical)
        scale_track(model, record + GRAVITY, CONE_GRAVITY)
        # No drag: thrown at an even pace, the flames spread over the whole cone (with it they bunched up near the
        # apex, the far half bare)
        struct.pack_into('<f', model.data, record + DRAG, 0.0)
        throws = reach(model, record)
        if not throws:
            continue
        # How far it throws now, brought to the cone's radius; thicker (a burst's few particles thrown over a whole
        # cone read as scattered drops)
        scale_track(model, record + SPEED, radius / throws)
        scale_track(model, record + RATE, CONE_DENSITY)


def line(model, length, width):
    """every emitter a sheet over the strip along +Y, its particles rising from it"""
    area = length * width
    for record in model.records('particles', m2.PARTICLE_SIZE):
        model.data[record + 41] = PLANE
        x, y, z = struct.unpack_from('<3f', model.data, record + POSITION)
        struct.pack_into('<3f', model.data, record + POSITION, 0.0, length / 2.0, z)
        # A plane's area, full extents: its length along the model's +Y, its width along +X; less the flames'
        # own size, which spill past it (measured in the lab: 16 by 4 drew 9 by 3.5 from 8 by 2)
        set_track(model, record + AREA_LENGTH, max(0.5, length - SPRITE_SPILL))
        set_track(model, record + AREA_WIDTH, max(0.5, width - SPRITE_SPILL))
        set_track(model, record + VERTICAL, CONE_VERTICAL)
        set_track(model, record + HORIZONTAL, math.pi)
        scale_track(model, record + SPEED, 0.35)
        scale_track(model, record + GRAVITY, 0.5)
        # More of them as the strip is larger, but not a solid sheet (additive flames over each other burn white)
        scale_track(model, record + RATE, min(LINE_DENSITY_MAX, max(1.0, area / LINE_YARDS_PER_BURST)))


def ring(model, inner, outer):
    """the particles born on the circle of radius inner, thrown out flat to outer"""
    middle = (inner + outer) / 2.0
    for record in model.records('particles', m2.PARTICLE_SIZE):
        x, y, z = struct.unpack_from('<3f', model.data, record + POSITION)
        struct.pack_into('<3f', model.data, record + POSITION, 0.0, 0.0, z)
        set_track(model, record + HORIZONTAL, math.pi)
        if model.data[record + 41] != SPHERE:
            continue
        # Born a little outside inner: the flames' own size spills back in (6 to 10 drew from 4.4)
        born = inner + SPRITE_SPILL
        set_track(model, record + AREA_LENGTH, born)
        set_track(model, record + AREA_WIDTH, born)
        set_track(model, record + VERTICAL, CONE_VERTICAL)
        scale_track(model, record + GRAVITY, CONE_GRAVITY)
        throws = reach(model, record)
        if throws:
            scale_track(model, record + SPEED, max(1.0, outer - born) / throws)
        # As many more as the ring is long (a burst covers about 4 yards of it)
        scale_track(model, record + RATE, min(20.0, max(1.0, 2.0 * math.pi * middle / 4.0)))


def build(entry, output):
    source = os.path.join(REPO, entry['source'])
    model = m2.M2(open(source, 'rb').read())
    form = entry['form']
    if 'cone' in form:
        cone(model, float(form['cone']['arc']), float(form['cone']['radius']), form['cone'].get('vertical'))
    elif 'line' in form:
        line(model, float(form['line']['length']), float(form['line']['width']))
    elif 'ring' in form:
        ring(model, float(form['ring']['inner']), float(form['ring']['outer']))
    else:
        raise SystemExit(f'{entry["key"]}: unknown form {form}')
    target = os.path.join(output, entry['key'] + '.m2')
    open(target, 'wb').write(model.data)
    for index in range(model.skin_count):
        shutil.copyfile(source[:-3] + f'{index:02}.skin', os.path.join(output, entry['key'] + f'{index:02}.skin'))
    return target


def main():
    config = json.load(open(CONFIG, encoding='utf-8'))
    output = os.path.join(REPO, config['output'], *config['folder'].split('/'))
    # The folder is all ours: rebuilt from nothing, none left of an effect taken out of the config
    shutil.rmtree(output, ignore_errors=True)
    os.makedirs(output)
    for entry in config['effects']:
        print(build(entry, output))


if __name__ == '__main__':
    main()
