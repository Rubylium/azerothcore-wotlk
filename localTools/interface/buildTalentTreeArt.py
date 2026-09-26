"""Builds the art of the talent tree window (clientPatcher/interface/Interface/FrameXML/TalentTree.lua).

Everything lands in clientPatcher/interface/Interface/TalentTree, described by the generated
clientPatcher/interface/Interface/FrameXML/TalentTreeArt.lua, which the window reads:

- retail talent pieces: the node frames in every state, their glows, the gate, the reset and undo buttons, the bottom
  bar and the apply animations, cropped from the retail client the way extractRetailUi.py does (TACTTool, by
  Marlamin). The sheets and the atlas CSVs are cached under localTools/interface/cache/talents (gitignored), so
  only the first run needs retail and wago.tools.
- each class's background: a retail talent background recoloured to the class (`backgroundArt` in its talentTree JSON)
- node icons: the WotLK client's own icons cut to the node shapes - round for a passive, octagonal for a choice, and
  for each choice node its two options side by side, as retail shows a choice not made yet. 3.3.5 has no texture
  masks, so the shapes are baked here.
- the links between nodes, with their arrowhead. Rotated SetTexCoord does not render on this client (Paragon.lua
  explains), so each shape a tree uses is drawn already at its angle, white, and tinted by the window.
- the rank badge in a node's corner

The grid spacing is decided here and handed to the window through TalentTreeArt.lua, because the link art is drawn
for it.

Usage: python localTools/interface/buildTalentTreeArt.py [--tacttool <TACTTool.exe>] [--retail <retail WoW>]
"""
import argparse
import colorsys
import csv
import json
import math
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from buildParagonArt import writeDxt3Blp, writeRawBlp  # noqa: E402
from extractRetailUi import read_blp  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLASSES = os.path.join(REPO, 'localTools', 'customClasses', 'classes.json')
OUTPUT = os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', 'TalentTree')
LUA_OUT = os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', 'FrameXML', 'TalentTreeArt.lua')
CACHE = os.path.join(REPO, 'localTools', 'interface', 'cache', 'talents')
EXTRACT_CLIENT = os.path.join(REPO, 'localTools', 'mpq-builder', 'extractClientFiles.js')
GAME_PATH = 'Interface' + chr(92) + 'TalentTree' + chr(92)
ICON_PATH = 'Interface' + chr(92) + 'Icons' + chr(92)

# The window's grid, in UI units: centre to centre between columns and between rows
COLUMN, ROW = 55, 55
# Nodes are drawn at this share of retail's size (40 units at 1.0): the rows need about twice a node's height between
# them, as retail has, or the links shrink to stubs and their arrowheads run into the frames
NODE_SCALE = 0.9
# Where a link's arrowhead stops short of the centre of the node it points at, by that node's kind
REACH = {kind: round(units * NODE_SCALE) for kind, units in {'passive': 21, 'active': 23, 'choice': 28}.items()}
# Art is drawn at two texels per UI unit
SCALE = 2

# Retail atlas members -> file name here. Frames are drawn by the window at 0.8 (circles) or 0.5 (squares and
# choices) of these pixel sizes, which gives every kind the same 30-unit opening.
PIECES = {
    'talents-node-circle-yellow': 'node-circle-yellow', 'talents-node-circle-green': 'node-circle-green',
    'talents-node-circle-gray': 'node-circle-gray', 'talents-node-circle-locked': 'node-circle-locked',
    'talents-node-circle-red': 'node-circle-red', 'talents-node-circle-shadow': 'node-circle-shadow',
    'talents-node-square-yellow': 'node-square-yellow', 'talents-node-square-green': 'node-square-green',
    'talents-node-square-gray': 'node-square-gray', 'talents-node-square-locked': 'node-square-locked',
    'talents-node-square-red': 'node-square-red', 'talents-node-square-shadow': 'node-square-shadow',
    'talents-node-choice-yellow': 'node-choice-yellow', 'talents-node-choice-green': 'node-choice-green',
    'talents-node-choice-gray': 'node-choice-gray', 'talents-node-choice-locked': 'node-choice-locked',
    'talents-node-choice-red': 'node-choice-red', 'talents-node-choice-shadow': 'node-choice-shadow',
    'talents-gate': 'gate', 'talents-gate-open': 'gate-open',
    'talents-button-reset': 'button-reset', 'talents-button-undo': 'button-undo',
    'talents-icon-learnableplus': 'icon-learnableplus', 'talents-search-match': 'search-match',
    'talents-background-bottombar': 'bottombar',
}
# Glows, turned white so the window can tint them (gold when learned, blue when it can be, red when refused)
GLOWS = {
    'talents-node-circle-greenglow': 'glow-circle', 'talents-node-square-greenglow': 'glow-square',
    'talents-node-choice-greenglow': 'glow-choice',
}
# Big additive art for the moment a build is applied, scaled down: it only ever plays blurred by motion
ANIMATIONS = {
    'talents-animations-gridburst': ('anim-burst', 0.5),
    'talents-animations-orb-activated': ('anim-orb', 512 / 600),
    'talents-animations-particles': ('anim-particles', 0.5),
}
MASKS = {'talents-node-circle-mask': 'circle', 'talents-node-choice-mask': 'choice'}
BACKGROUNDS = {'rogue-outlaw': 'talents-background-rogue-outlaw',
               'rogue-assassination': 'talents-background-rogue-assassination',
               'rogue-subtlety': 'talents-background-rogue-subtlety',
               'mage-arcane': 'talents-background-mage-arcane', 'mage-fire': 'talents-background-mage-fire',
               'mage-frost': 'talents-background-mage-frost'}
# The specialization page's figures: the right of each spec's painting, from this fraction of its width
SPEC_ART_LEFT = 0.62
SPEC_ART_WIDTH = 400


def power_of_two(value):
    size = 1
    while size < value:
        size *= 2
    return size


def canvas_for(image):
    canvas = Image.new('RGBA', (power_of_two(image.width), power_of_two(image.height)), (0, 0, 0, 0))
    canvas.alpha_composite(image.convert('RGBA'), (0, 0))
    return canvas


class Retail:
    """The retail atlas, and the sheets it points into (cached after the first extraction)."""

    def __init__(self, args):
        self.args = args
        folder = args.atlas_csv
        self.atlases = {row['ID']: row for row in csv.DictReader(
            open(os.path.join(folder, 'UiTextureAtlas.csv'), encoding='utf8'))}
        self.members = {row['CommittedName']: row for row in csv.DictReader(
            open(os.path.join(folder, 'UiTextureAtlasMember.csv'), encoding='utf8'))}
        self.sheets = {}
        os.makedirs(CACHE, exist_ok=True)

    def sheet(self, file_data_id):
        if file_data_id not in self.sheets:
            target = os.path.join(CACHE, '%s.blp' % file_data_id)
            if not os.path.exists(target):
                if not self.args.tacttool:
                    raise SystemExit('Retail sheet %s is not cached: pass --tacttool' % file_data_id)
                subprocess.run([self.args.tacttool, '-r', self.args.region, '-m', 'fdid', '-i', str(file_data_id),
                                '-d', self.args.retail, '-o', target], check=True, capture_output=True)
            self.sheets[file_data_id] = read_blp(target).convert('RGBA')
        return self.sheets[file_data_id]

    def piece(self, name):
        member = self.members[name]
        sheet = self.sheet(self.atlases[member['UiTextureAtlasID']]['FileDataID'])
        return sheet.crop(tuple(int(member[key]) for key in
                                ('CommittedLeft', 'CommittedTop', 'CommittedRight', 'CommittedBottom')))


class Art:
    """What gets written, and the Lua lines describing it."""

    def __init__(self):
        os.makedirs(OUTPUT, exist_ok=True)
        os.makedirs(os.path.join(OUTPUT, 'Icons'), exist_ok=True)
        self.pieces = []
        self.written = set()

    def save(self, image, name, compressed=False, record=True):
        canvas = canvas_for(image)
        path = os.path.join(OUTPUT, *name.split('/')) + '.blp'
        (writeDxt3Blp if compressed else writeRawBlp)(canvas, path)
        self.written.add(os.path.normcase(path))
        if record:
            self.pieces.append((name, image.width, image.height, image.width / canvas.width,
                                image.height / canvas.height))
        return GAME_PATH + name.replace('/', chr(92))


def whiten(image):
    """A glow's brightness as white, keeping its alpha, so a vertex colour decides its hue."""
    red, green, blue, alpha = image.split()
    value = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    return Image.merge('RGBA', (value, value, value, alpha))


def recolour(image, hue):
    """Moves the background's greens and teals to `hue` (degrees), leaving skin, steel and shadow where they are."""
    pixels = image.convert('RGBA').load()
    result = image.convert('RGBA').copy()
    out = result.load()
    target = hue / 360.0
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels[x, y]
            h, s, v = colorsys.rgb_to_hsv(red / 255, green / 255, blue / 255)
            # Greens and teals (75-200 degrees) all the way, fading out over 25 degrees either side
            degrees = h * 360
            weight = 1.0 if 75 <= degrees <= 200 else max(0.0, 1 - min(abs(degrees - 75), abs(degrees - 200)) / 25)
            if weight <= 0 or s < 0.05:
                continue
            shifted = h + (target - h) * weight
            r, g, b = colorsys.hsv_to_rgb(shifted % 1.0, min(1.0, s * (1 + 0.15 * weight)), v)
            out[x, y] = (int(r * 255), int(g * 255), int(b * 255), alpha)
    return result


def shade(image, left, right, vignette):
    """Darkens a painting to the retail backgrounds' level, where the trees have to stay readable: by `left` at its
    left edge easing to `right` at its right edge (where the figure stands), and a further `vignette` towards the
    top, bottom and left edges."""
    width, height = image.size
    ramp = Image.linear_gradient('L').rotate(90, expand=True).resize((width, height))
    factor = Image.new('L', (width, height))
    ramp_pixels = ramp.load()
    pixels = factor.load()
    for y in range(height):
        edge_y = min(y, height - 1 - y) / (height * 0.22)
        for x in range(width):
            across = 1 - ramp_pixels[x, y] / 255.0
            edge = min(1.0, edge_y, x / (width * 0.18))
            value = (left + (right - left) * across) * (1 - vignette * (1 - edge) ** 2)
            pixels[x, y] = max(0, min(255, round(255 * value)))
    red, green, blue, alpha = image.split()
    return Image.merge('RGBA', tuple(ImageChops.multiply(channel, factor) for channel in (red, green, blue)) + (alpha,))


def compiled_icon(name):
    """A class's own icon (Necromancer_*, PestifereTalent_*...), compiled to TGA by its module's asset build and
    packed into patch-Z: read from the module, so a client with an older patch still gets the new one."""
    for root, _, files in os.walk(os.path.join(REPO, 'modules')):
        if os.path.basename(root) in ('icons', 'compiled') and name + '.tga' in files:
            return os.path.join(root, name + '.tga')
    return None


def client_icons(names, client):
    """Every icon named: the classes' own from their modules, the stock ones from the WotLK client's archives."""
    icons = {}
    for name in names:
        path = compiled_icon(name)
        if path:
            icons[name] = Image.open(path).convert('RGBA')
    stock = sorted(name for name in names if name not in icons)
    if not stock:
        return icons

    work = tempfile.mkdtemp(prefix='talenticons-')
    wanted = [ICON_PATH + name + '.blp' for name in stock]
    listing = os.path.join(work, 'list.json')
    json.dump(wanted, open(listing, 'w'))
    subprocess.run(['node', EXTRACT_CLIENT, listing, work, '--client', client], check=True,
                   cwd=os.path.dirname(EXTRACT_CLIENT))
    missing = json.load(open(os.path.join(work, 'missing.json')))
    if missing:
        raise SystemExit('Icons not in the client: %s' % ', '.join(missing))
    for name in stock:
        icons[name] = Image.open(os.path.join(work, 'Interface', 'Icons', name + '.blp')).convert('RGBA')
    return icons


def trimmed(icon, size=64):
    """The icon without the bevel every WotLK icon carries in its outer 7%."""
    w, h = icon.size
    return icon.crop((round(w * 0.07), round(h * 0.07), round(w * 0.93), round(h * 0.93))).resize(
        (size, size), Image.Resampling.LANCZOS)


def masked(icon, mask):
    shape = mask.getchannel('A').resize(icon.size, Image.Resampling.LANCZOS)
    result = icon.copy()
    result.putalpha(ImageChops.multiply(icon.getchannel('A'), shape))
    return result


def split(first, second, mask):
    """A choice not made: the left half of one option beside the right half of the other, a dark seam between."""
    size = first.size[0]
    face = Image.new('RGBA', first.size)
    face.paste(first.crop((0, 0, size // 2, size)), (0, 0))
    face.paste(second.crop((size // 2, 0, size, size)), (size // 2, 0))
    draw = ImageDraw.Draw(face)
    draw.line([(size // 2, 0), (size // 2, size)], fill=(0, 0, 0, 220), width=3)
    return masked(face, mask)


def draw_link(dx, dy, reach):
    """A white line from a parent's centre to where a child `dx` columns across and `dy` rows down ends, an arrowhead
    at `reach` units short of the child's centre. Drawn four times over and scaled down, for clean edges."""
    over = 4
    pad = 14
    width_units = dx * COLUMN + 2 * pad
    height_units = dy * ROW + 2 * pad
    big = Image.new('RGBA', (width_units * SCALE * over, height_units * SCALE * over), (0, 0, 0, 0))
    unit = SCALE * over
    start = (pad * unit, pad * unit)
    end = ((pad + dx * COLUMN) * unit, (pad + dy * ROW) * unit)
    length = math.hypot(end[0] - start[0], end[1] - start[1])
    ux, uy = (end[0] - start[0]) / length, (end[1] - start[1]) / length
    tip = (end[0] - ux * reach * unit, end[1] - uy * reach * unit)
    head_length, head_half = 7 * unit, 5 * unit
    base = (tip[0] - ux * head_length, tip[1] - uy * head_length)
    left = (base[0] - uy * head_half, base[1] + ux * head_half)
    right = (base[0] + uy * head_half, base[1] - ux * head_half)
    shaft_end = (base[0] + ux * unit, base[1] + uy * unit)

    shadow = Image.new('RGBA', big.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(shadow)
    draw.line([start, shaft_end], fill=(0, 0, 0, 170), width=int(5 * unit))
    draw.polygon([tip, left, right], fill=(0, 0, 0, 170))
    shadow = shadow.filter(ImageFilter.GaussianBlur(unit * 1.2))
    big.alpha_composite(shadow)

    draw = ImageDraw.Draw(big)
    draw.line([start, shaft_end], fill=(205, 205, 205, 255), width=int(3 * unit))
    draw.line([start, shaft_end], fill=(255, 255, 255, 255), width=int(1.6 * unit))
    draw.polygon([tip, left, right], fill=(235, 235, 235, 255))
    image = big.resize((width_units * SCALE, height_units * SCALE), Image.Resampling.LANCZOS)
    return image, pad


def rank_badge():
    """The small plate a node's rank sits on: dark, with a white rim the window tints."""
    over = 4
    width, height = 22 * SCALE * over, 15 * SCALE * over
    image = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    radius = 5 * SCALE * over
    draw.rounded_rectangle([0, 0, width - 1, height - 1], radius=radius, fill=(255, 255, 255, 255))
    rim = int(1.2 * SCALE * over)
    draw.rounded_rectangle([rim, rim, width - 1 - rim, height - 1 - rim], radius=radius - rim, fill=(12, 10, 8, 235))
    return image.resize((22 * SCALE, 15 * SCALE), Image.Resampling.LANCZOS)


def lua_string(text):
    return '"' + text.replace(chr(92), chr(92) * 2).replace('"', chr(92) + '"') + '"'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tacttool')
    parser.add_argument('--retail', default='C:/Program Files (x86)/World of Warcraft')
    parser.add_argument('--region', default='eu')
    # UiTextureAtlas.csv and UiTextureAtlasMember.csv (wago.tools exports), kept beside the cached sheets
    parser.add_argument('--atlas-csv', default=CACHE)
    parser.add_argument('--client', default='C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK')
    args = parser.parse_args()

    retail = Retail(args)
    art = Art()

    for member, name in PIECES.items():
        # Uncompressed, the bottom bar's long dark gradient included: DXT bands it into visible steps
        art.save(retail.piece(member), name)
    for member, name in GLOWS.items():
        art.save(whiten(retail.piece(member)), name)
    for member, (name, factor) in ANIMATIONS.items():
        image = retail.piece(member)
        image = image.resize((round(image.width * factor), round(image.height * factor)), Image.Resampling.LANCZOS)
        art.save(image, name, compressed=True)
    art.save(rank_badge(), 'rank-badge')
    masks = {kind: retail.piece(member) for member, kind in MASKS.items()}

    # The trees: which icons they need, which link shapes, which backgrounds
    trees = []
    classes = json.load(open(CLASSES, encoding='utf8'))
    for entry in classes['classes'] + classes.get('stockTalentTrees', []):
        if entry.get('talentTree'):
            trees.append(json.load(open(os.path.join(REPO, entry['talentTree']), encoding='utf8')))

    def painting(style):
        """A background: a painting of the class's own, or a retail background recoloured to it."""
        if style.get('file'):
            # The window shows it cut to 1.6:1 from its right edge, so the figure belongs in the right third; it
            # is scaled to retail's 1612 texels wide
            image = Image.open(os.path.join(REPO, style['file'])).convert('RGBA')
            image = image.resize((1612, round(image.height * 1612 / image.width)), Image.Resampling.LANCZOS)
            # Darkened only when asked: a painting made dark enough needs none
            if style.get('shade'):
                dark = style['shade']
                image = shade(image, dark['left'], dark['right'], dark['vignette'])
            return image
        piece = retail.piece(BACKGROUNDS[style['from']])
        # A class that has retail backgrounds of its own (the Mage) keeps their colours
        return recolour(piece, style['hue']) if 'hue' in style else piece.convert('RGBA')

    # Keyed by class id, and by "<class>-<tree>" for a spec tree with a painting of its own (shown while that
    # specialization is the one in view). A class with several specializations also gets each one's figure, cut
    # from the right of its painting, for the specialization page.
    backgrounds, spec_art = {}, {}
    for definition in trees:
        class_id = definition['classId']
        backgrounds[class_id] = art.save(painting(definition.get('backgroundArt', {'from': 'rogue-outlaw',
                                                                                   'hue': 212})),
                                         'background-%d' % class_id, compressed=True)
        specs = [tree for tree in definition['trees'] if tree['kind'] == 'spec']
        for tree in specs:
            if not tree.get('backgroundArt'):
                continue
            key = '%d-%d' % (class_id, tree['id'])
            image = painting(tree['backgroundArt'])
            backgrounds[key] = art.save(image, 'background-' + key, compressed=True)
            if len(specs) > 1:
                figure = image.crop((round(image.width * SPEC_ART_LEFT), 0, image.width, image.height))
                figure = figure.resize((SPEC_ART_WIDTH, round(figure.height * SPEC_ART_WIDTH / figure.width)),
                                       Image.Resampling.LANCZOS)
                spec_art[key] = art.save(figure, 'spec-' + key, compressed=True)

    circle_icons, octagon_icons, split_icons, link_shapes = {}, {}, {}, set()
    names = set()
    for definition in trees:
        for tree in definition['trees']:
            by_id = {node['id']: node for node in tree['nodes']}
            for node in tree['nodes']:
                for option in node.get('options', [node]):
                    names.add(option['icon'])
                reach = REACH[node['kind']]
                for parent in node.get('parents', []):
                    link_shapes.add((abs(node['col'] - by_id[parent]['col']), node['row'] - by_id[parent]['row'],
                                     reach))
    icons = client_icons(names, args.client)

    for definition in trees:
        for tree in definition['trees']:
            for node in tree['nodes']:
                if node['kind'] == 'passive':
                    circle_icons[node['icon']] = art.save(masked(trimmed(icons[node['icon']]), masks['circle']),
                                                          'Icons/c-' + node['icon'], record=False)
                elif node['kind'] == 'choice':
                    faces = [masked(trimmed(icons[option['icon']]), masks['choice']) for option in node['options']]
                    for option, face in zip(node['options'], faces):
                        octagon_icons[option['icon']] = art.save(face, 'Icons/o-' + option['icon'], record=False)
                    # Named by its two faces, not by the node: the classes' trees share node ids, and a name by id
                    # let the last class built overwrite every other class's face of that node
                    pair = tuple(option['icon'] for option in node['options'][:2])
                    first, second = (trimmed(icons[icon]) for icon in pair)
                    split_icons[pair] = art.save(split(first, second, masks['choice']),
                                                 'Icons/s-%s-%s' % pair, record=False)

    links = {}
    for dx, dy, reach in sorted(link_shapes):
        image, pad = draw_link(dx, dy, reach)
        name = 'link-%dx%d-%d' % (dx, dy, reach)
        canvas = canvas_for(image)
        art.save(image, name, compressed=True, record=False)
        links[name] = (canvas.width // SCALE, canvas.height // SCALE, pad)

    # Anything left from an earlier build (an icon or a link shape no tree uses any more) goes
    for folder, _, files in os.walk(OUTPUT):
        for file in files:
            path = os.path.join(folder, file)
            if file.lower().endswith('.blp') and os.path.normcase(path) not in art.written:
                os.remove(path)

    lines = [
        '-- Generated by localTools/interface/buildTalentTreeArt.py -- do not edit by hand.',
        '--',
        '-- The art of the talent tree window (TalentTree.lua): retail talent pieces, recoloured backgrounds,',
        '-- node icons cut to shape, and link art drawn for this grid. `w`/`h` are texels, `r`/`b` the texture',
        '-- coordinates of the piece\'s bottom-right corner in its power-of-two canvas.',
        '',
        'TalentTreeArt = {',
        '    grid = { column = %d, row = %d, scale = %s,' % (COLUMN, ROW, NODE_SCALE),
        '        reach = { passive = %d, active = %d, choice = %d } },' % (
            REACH['passive'], REACH['active'], REACH['choice']),
        '    pieces = {',
    ]
    for name, width, height, right, bottom in art.pieces:
        lines.append('        [%s] = { file = %s, w = %d, h = %d, r = %.6f, b = %.6f },' % (
            lua_string(name.split('/')[-1]), lua_string(GAME_PATH + name.replace('/', chr(92))), width, height,
            right, bottom))
    lines.append('    },')
    lines.append('    -- Canvas size in UI units, and the parent centre\'s offset from its top-left corner')
    lines.append('    links = {')
    for name, (width, height, pad) in links.items():
        lines.append('        [%s] = { file = %s, w = %d, h = %d, pad = %d },' % (
            lua_string(name), lua_string(GAME_PATH + name), width, height, pad))
    lines.append('    },')
    lines.append('    backgrounds = {')
    for key, path in backgrounds.items():
        lines.append('        [%s] = %s,' % (key if isinstance(key, int) else lua_string(key), lua_string(path)))
    lines.append('    },')
    lines.append('    specArt = {')
    for key, path in spec_art.items():
        lines.append('        [%s] = %s,' % (lua_string(key), lua_string(path)))
    lines.append('    },')
    for label, table in (('circle', circle_icons), ('octagon', octagon_icons)):
        lines.append('    %s = {' % label)
        for icon, path in sorted(table.items()):
            lines.append('        [%s] = %s,' % (lua_string(icon), lua_string(path)))
        lines.append('    },')
    lines.append('    split = {')
    for pair, path in sorted(split_icons.items()):
        lines.append('        [%s] = %s,' % (lua_string('%s|%s' % pair), lua_string(path)))
    lines.append('    },')
    lines.append('}')
    lines.append('')
    with open(LUA_OUT, 'w', encoding='utf-8', newline='\n') as handle:
        handle.write('\n'.join(lines))

    print('%d pieces, %d round and %d octagonal icons, %d split faces, %d link shapes' % (
        len(art.pieces), len(circle_icons), len(octagon_icons), len(split_icons), len(links)))
    print(LUA_OUT)


if __name__ == '__main__':
    sys.exit(main())
