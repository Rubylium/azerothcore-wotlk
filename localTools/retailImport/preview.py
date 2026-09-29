"""Renders the models RetailImport converted next to their retail originals, to check a conversion by eye.

For each model in the work folder (.deps/work/<name>/, kept by `RetailImport import`): the retail file (MD21, its own
.skin) on the left, the converted 3.3.5 file from the output root on the right, both drawn with the same software
rasterizer: static bind pose, one light, each batch's first texture with its material's blend mode (opaque, alpha
key, alpha, additive). Environment-mapped and second-texture layers are left out, so shine and glow effects look
flatter than in game; the check is that geometry, UVs, textures and which parts are see-through survived.

Usage: python localTools/retailImport/preview.py [--out DIR]    (writes <name>.png, default .deps/previews)
"""
import argparse
import glob
import os
import struct

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
WORK = os.path.join(HERE, '.deps', 'work')
SIZE = 640


def array(data, offset):
    return struct.unpack_from('<II', data, offset)


def md20(path):
    data = open(path, 'rb').read()
    if data[:4] == b'MD21':
        return data[8:8 + struct.unpack_from('<I', data, 4)[0]]
    return data


def load_model(m2_path, skin_path, textures_by_index):
    """Triangles grouped by batch: (positions, normals, uvs, image, blend, priority)."""
    data = md20(m2_path)
    count, offset = array(data, 0x3C)
    raw = np.frombuffer(data, dtype=np.uint8, count=count * 48, offset=offset).reshape(count, 48)
    positions = raw[:, 0:12].copy().view(np.float32).reshape(count, 3)
    normals = raw[:, 20:32].copy().view(np.float32).reshape(count, 3)
    uvs = raw[:, 32:40].copy().view(np.float32).reshape(count, 2)
    materials = [struct.unpack_from('<HH', data, array(data, 0x70)[1] + 4 * i) for i in range(array(data, 0x70)[0])]
    n, o = array(data, 0x80)
    texture_lookup = struct.unpack_from('<%dh' % n, data, o)

    skin = open(skin_path, 'rb').read()
    n, o = array(skin, 0x04)
    skin_vertices = np.array(struct.unpack_from('<%dH' % n, skin, o))
    n, o = array(skin, 0x0C)
    indices = np.array(struct.unpack_from('<%dH' % n, skin, o))
    n, o = array(skin, 0x1C)
    submeshes = [struct.unpack_from('<6H', skin, o + 48 * i) for i in range(n)]
    n, o = array(skin, 0x24)
    batches = []
    for i in range(n):
        flags, priority, shader, submesh, _, _, material, layer, _, texture_combo = struct.unpack_from(
            '<BbHHHhHHHH', skin, o + 24 * i)
        if shader == 0x8000 and texture_combo >= len(texture_lookup):
            continue
        _, _, _, _, index_start, index_count = submeshes[submesh]
        triangles = skin_vertices[indices[index_start:index_start + index_count]].reshape(-1, 3)
        image = textures_by_index(texture_lookup[texture_combo])
        blend = materials[material][1]
        batches.append((triangles, image, blend, priority * 16 + layer))
    batches.sort(key=lambda batch: (batch[2] > 1, batch[3]))
    return positions, normals, uvs, batches


def texture_loader(m2_path, display_texture, texture_root):
    """Texture index -> RGBA float array: type 2 is the display texture, type 0 is found by file name."""
    data = md20(m2_path)
    n, o = array(data, 0x50)
    cache = {}

    def load(index):
        if index in cache:
            return cache[index]
        kind, _, length, name_offset = struct.unpack_from('<IIII', data, o + 16 * index)
        if kind == 2:
            path = display_texture
        else:
            name = data[name_offset:name_offset + length].split(b'\0')[0].decode('latin-1')
            base = os.path.basename(name.replace('\\', '/')) or 'missing'
            found = glob.glob(os.path.join(texture_root, '**', base), recursive=True)
            path = found[0] if found else None
        image = np.asarray(Image.open(path).convert('RGBA'), dtype=np.float32) / 255 if path else None
        cache[index] = image
        return image

    return load


def render(positions, normals, uvs, batches, rotation, scale, center):
    color = np.zeros((SIZE, SIZE, 3), dtype=np.float32) + np.array([0.13, 0.12, 0.11])
    depth = np.full((SIZE, SIZE), np.inf, dtype=np.float32)
    view = (positions - center) @ rotation.T
    screen = np.empty((len(view), 2))
    screen[:, 0] = SIZE / 2 + view[:, 0] * scale
    screen[:, 1] = SIZE / 2 - view[:, 1] * scale
    lit_normals = normals @ rotation.T
    light = np.array([-0.4, 0.5, 0.75])
    light /= np.linalg.norm(light)
    for triangles, image, blend, _ in batches:
        for a, b, c in triangles:
            p = screen[[a, b, c]]
            x0, y0 = np.floor(p.min(axis=0)).astype(int)
            x1, y1 = np.ceil(p.max(axis=0)).astype(int)
            x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, SIZE - 1), min(y1, SIZE - 1)
            if x1 < x0 or y1 < y0:
                continue
            area = (p[1, 0] - p[0, 0]) * (p[2, 1] - p[0, 1]) - (p[2, 0] - p[0, 0]) * (p[1, 1] - p[0, 1])
            if abs(area) < 1e-9:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((p[1, 0] - xs) * (p[2, 1] - ys) - (p[2, 0] - xs) * (p[1, 1] - ys)) / area
            w1 = ((p[2, 0] - xs) * (p[0, 1] - ys) - (p[0, 0] - xs) * (p[2, 1] - ys)) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            z = w0 * view[a, 2] + w1 * view[b, 2] + w2 * view[c, 2]
            region = depth[y0:y1 + 1, x0:x1 + 1]
            visible = inside & (z < region)
            if not visible.any():
                continue
            uv = w0[..., None] * uvs[a] + w1[..., None] * uvs[b] + w2[..., None] * uvs[c]
            if image is not None:
                h, w = image.shape[:2]
                tx = (np.mod(uv[..., 0], 1) * w).astype(int).clip(0, w - 1)
                ty = (np.mod(uv[..., 1], 1) * h).astype(int).clip(0, h - 1)
                texel = image[ty, tx]
            else:
                texel = np.ones(xs.shape + (4,), dtype=np.float32) * np.array([1, 0, 1, 1])
            n = w0[..., None] * lit_normals[a] + w1[..., None] * lit_normals[b] + w2[..., None] * lit_normals[c]
            n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9
            shade = 0.45 + 0.75 * np.abs(n @ light)
            rgb = texel[..., :3] * shade[..., None]
            alpha = texel[..., 3]
            target = color[y0:y1 + 1, x0:x1 + 1]
            if blend == 0:
                mask = visible
                target[mask] = rgb[mask]
            elif blend == 1:
                mask = visible & (alpha > 0.5)
                target[mask] = rgb[mask]
            elif blend == 2:
                mask = visible
                target[mask] = target[mask] * (1 - alpha[mask, None]) + rgb[mask] * alpha[mask, None]
            else:
                mask = visible
                target[mask] = np.minimum(target[mask] + texel[..., :3][mask] * alpha[mask, None], 1)
            if blend <= 1:
                region[mask] = z[mask]
    return Image.fromarray((color.clip(0, 1) * 255).astype(np.uint8))


def rotation_for(positions, yaw, pitch):
    # Look along the model's thinnest axis, so a blade or a pauldron shows its broad side
    extent = positions.max(axis=0) - positions.min(axis=0)
    thin = int(np.argmin(extent))
    longest = int(np.argmax(extent))
    up = np.eye(3)[longest]
    forward = np.eye(3)[thin]
    right = np.cross(up, forward)
    base = np.stack([right, up, forward])
    cy, sy, cp, sp = np.cos(yaw), np.sin(yaw), np.cos(pitch), np.sin(pitch)
    spin = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]) @ np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
    return spin @ base


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', default=os.path.join(HERE, '.deps', 'previews'))
    parser.add_argument('--output-root', default=os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets',
                                                              'compiled', 'retail-items'))
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    import json
    generated = json.load(open(os.path.join(HERE, 'retailItems.generated.json'), encoding='utf-8'))
    for display in generated['displays']:
        for slot, model in enumerate(display['modelName']):
            if not model:
                continue
            name = model[:-4]
            converted = glob.glob(os.path.join(args.output_root, 'Item', 'ObjectComponents', '*', name + '.m2'))[0]
            folder = os.path.dirname(converted)
            texture = os.path.join(folder, display['modelTexture'][slot] + '.blp')
            retail_m2 = os.path.join(WORK, name, name + '.retail.m2')
            retail_skin = os.path.join(WORK, name, name + '00.retail.skin')
            panels = []
            for m2, skin in ((retail_m2, retail_skin), (converted, converted[:-3] + '00.skin')):
                positions, normals, uvs, batches = load_model(m2, skin, texture_loader(m2, texture, args.output_root))
                # Framed on what the skin draws: a model can hold vertices no batch uses
                drawn = positions[np.unique(np.concatenate([batch[0].ravel() for batch in batches]))]
                center = (drawn.max(axis=0) + drawn.min(axis=0)) / 2
                scale = 0.9 * SIZE / np.linalg.norm(drawn.max(axis=0) - drawn.min(axis=0))
                for yaw, pitch in ((0.0, 0.0), (0.8, 0.35)):
                    panels.append(render(positions, normals, uvs, batches, rotation_for(drawn, yaw, pitch),
                                         scale, center))
            sheet = Image.new('RGB', (SIZE * 2, SIZE * 2), (0, 0, 0))
            for index, panel in enumerate(panels):
                sheet.paste(panel, ((index % 2) * SIZE, (index // 2) * SIZE))
            out = os.path.join(args.out, f'{display["id"]}_{name}.png')
            sheet.save(out)
            print(f'{out}: top row retail, bottom row converted')


if __name__ == '__main__':
    main()
