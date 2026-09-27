"""Draws the Infinite Dungeon's own art and compiles it into the BLP2 files the 3.3.5 client loads.

Everything is drawn in code, four times larger than it ships, then scaled down: a gradient and a spiral are not
paintings, and the shapes stay crisp at the sizes the pin is shown (14 to 30 pixels). Uncompressed BGRA (the
Paragon glow's writer), so the thin gold rim keeps its colour at every mip level.

- InfiniteDungeon-Pin      the keeper's map pin (world map and minimap): a gold map pin with a bronze portal swirl
                           in its head, its tip at the bottom centre of the texture (PIN_TIP_Y from the top)
- InfiniteDungeon-Emblem   the same head alone, centred: the tracker's and the banners' emblem
- InfiniteDungeon-Swirl    the swirl's arms alone on nothing, turned by the client over the emblem (added light)
- InfiniteDungeon-Heart    the healing heart, for the tracker

Usage: python localTools/interface/buildInfiniteDungeonArt.py
"""
import importlib.util
import math
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "InfiniteDungeon")
SCALE = 4

# The pin, at 512: the head's centre and radius, and the tip (InfiniteDungeon.lua PIN_TIP: 492 / 512 from the top)
HEAD_X, HEAD_Y, HEAD_R = 256, 190, 150
TIP_Y = 492
INNER_R = 110

GOLD_TOP = (255, 231, 160)
GOLD_MID = (222, 168, 74)
GOLD_LOW = (122, 78, 30)
OUTLINE = (28, 17, 8)
DARK = (20, 13, 8)
DARK_EDGE = (46, 30, 15)


def load_blp_writer():
    path = os.path.join(HERE, "buildParagonArt.py")
    spec = importlib.util.spec_from_file_location("buildParagonArt", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def vertical_gradient(size, stops):
    """stops: [(0..1, colour)], top to bottom"""
    width, height = size
    image = Image.new("RGBA", size)
    draw = ImageDraw.Draw(image)
    for y in range(height):
        t = y / max(1, height - 1)
        for index in range(len(stops) - 1):
            (t0, c0), (t1, c1) = stops[index], stops[index + 1]
            if t0 <= t <= t1:
                colour = lerp(c0, c1, (t - t0) / max(1e-6, t1 - t0))
                break
        else:
            colour = stops[-1][1]
        draw.line([(0, y), (width, y)], fill=colour + (255,))
    return image


def radial(size, centre, radius, inner, outer):
    """A disc fading from `inner` (RGBA) at the centre to `outer` (RGBA) at `radius`"""
    image = Image.new("RGBA", size, outer)
    pixels = image.load()
    cx, cy = centre
    for y in range(size[1]):
        for x in range(size[0]):
            distance = math.hypot(x - cx, y - cy) / radius
            if distance < 1:
                pixels[x, y] = lerp(inner, outer, distance)
    return image


def with_alpha(image, mask):
    result = image.copy()
    alpha = ImageChops.multiply(result.getchannel("A"), mask)
    result.putalpha(alpha)
    return result


def pin_mask(size, grow=0):
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    radius = HEAD_R + grow
    draw.ellipse([HEAD_X - radius, HEAD_Y - radius, HEAD_X + radius, HEAD_Y + radius], fill=255)
    # The point: tangent from the tip to the head
    distance = TIP_Y + grow - HEAD_Y
    theta = math.acos(radius / distance)
    left = (HEAD_X - radius * math.sin(theta), HEAD_Y + radius * math.cos(theta))
    right = (HEAD_X + radius * math.sin(theta), HEAD_Y + radius * math.cos(theta))
    draw.polygon([left, right, (HEAD_X, TIP_Y + grow)], fill=255)
    return mask


def disc_mask(size, centre, radius):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).ellipse([centre[0] - radius, centre[1] - radius, centre[0] + radius, centre[1] + radius],
                                 fill=255)
    return mask


def swirl_layer(size, centre, radius, arms=3):
    """Spiral arms turning in from the edge, bright at the core, and their glow"""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    cx, cy = centre
    steps = 90
    for arm in range(arms):
        base = arm * 2 * math.pi / arms
        previous = None
        for step in range(steps + 1):
            t = step / steps
            r = radius * (0.08 + 0.92 * t)
            angle = base + 4.4 * t
            point = (cx + r * math.cos(angle), cy + r * math.sin(angle))
            if previous:
                width = int(round(radius * (0.2 - 0.15 * t)))
                colour = lerp((255, 244, 205), (236, 158, 58), min(1, t * 1.6)) if t < 0.62 else \
                    lerp((236, 158, 58), (118, 62, 22), (t - 0.62) / 0.38)
                alpha = int(255 * (1 - 0.55 * t))
                draw.line([previous, point], fill=colour + (alpha,), width=max(2, width))
                draw.ellipse([point[0] - width / 2, point[1] - width / 2, point[0] + width / 2,
                              point[1] + width / 2], fill=colour + (alpha,))
            previous = point
    arms_layer = layer.filter(ImageFilter.GaussianBlur(radius * 0.02))
    glow = layer.filter(ImageFilter.GaussianBlur(radius * 0.12))
    result = Image.alpha_composite(glow, arms_layer)
    # The core, a hot spot where the arms meet
    core = radial(size, centre, radius * 0.42, (255, 240, 196, 255), (255, 200, 110, 0))
    return Image.alpha_composite(result, core)


def head(size, centre, with_point):
    """The pin's head (and point): shadow, outline, gold rim, dark well, swirl, highlight"""
    cx, cy = centre
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))

    if with_point:
        shape = pin_mask(size)
        outline = pin_mask(size, grow=10)
    else:
        shape = disc_mask(size, centre, HEAD_R)
        outline = disc_mask(size, centre, HEAD_R + 10)

    shadow = Image.new("RGBA", size, (0, 0, 0, 170))
    shadow_mask = ImageChops.offset(outline, 8, 12).filter(ImageFilter.GaussianBlur(12))
    canvas = Image.alpha_composite(canvas, with_alpha(shadow, shadow_mask))

    canvas = Image.alpha_composite(canvas, with_alpha(Image.new("RGBA", size, OUTLINE + (255,)), outline))

    top = cy - HEAD_R
    bottom = TIP_Y if with_point else cy + HEAD_R
    gradient = vertical_gradient(size, [(0, GOLD_LOW), (max(0.0, (top - 4) / size[1]), GOLD_TOP),
                                        ((top + (bottom - top) * 0.45) / size[1], GOLD_MID),
                                        (bottom / size[1], GOLD_LOW), (1, GOLD_LOW)])
    canvas = Image.alpha_composite(canvas, with_alpha(gradient, shape))

    # A bevel: a dark groove inside the rim, then the well
    groove = disc_mask(size, centre, INNER_R + 8)
    canvas = Image.alpha_composite(canvas, with_alpha(Image.new("RGBA", size, (70, 44, 16, 255)), groove))
    well = disc_mask(size, centre, INNER_R)
    ground = radial(size, centre, INNER_R, DARK + (255,), DARK_EDGE + (255,))
    canvas = Image.alpha_composite(canvas, with_alpha(ground, well))

    swirl = swirl_layer(size, centre, INNER_R - 6)
    canvas = Image.alpha_composite(canvas, with_alpha(swirl, well))

    # Light on the rim's upper left
    shine = Image.new("L", size, 0)
    ImageDraw.Draw(shine).arc([cx - HEAD_R + 12, cy - HEAD_R + 12, cx + HEAD_R - 12, cy + HEAD_R - 12], 190, 280,
                              fill=150, width=12)
    shine = shine.filter(ImageFilter.GaussianBlur(4))
    canvas = Image.alpha_composite(canvas, with_alpha(Image.new("RGBA", size, (255, 250, 230, 255)), shine))
    return canvas


def build_pin():
    return head((512, 512), (HEAD_X, HEAD_Y), True)


def build_emblem():
    return head((512, 512), (256, 256), False)


def build_swirl():
    size = (512, 512)
    swirl = swirl_layer(size, (256, 256), 236)
    # Fades out towards its edge, so it can be added over anything round
    return with_alpha(swirl, radial(size, (256, 256), 250, (255, 255, 255, 255), (0, 0, 0, 0)).getchannel("A"))


def heart_mask(size, grow=0):
    mask = Image.new("L", size, 0)
    points = []
    scale = size[0] / 38.0 + grow / 16.0
    for step in range(360):
        t = step / 360 * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        points.append((size[0] / 2 + x * scale, size[1] * 0.46 - y * scale))
    ImageDraw.Draw(mask).polygon(points, fill=255)
    return mask


def build_heart():
    size = (256, 256)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    outline = heart_mask(size, grow=14)
    shadow_mask = ImageChops.offset(outline, 4, 6).filter(ImageFilter.GaussianBlur(6))
    canvas = Image.alpha_composite(canvas, with_alpha(Image.new("RGBA", size, (0, 0, 0, 160)), shadow_mask))
    rim = vertical_gradient(size, [(0, GOLD_TOP), (0.5, GOLD_MID), (1, GOLD_LOW)])
    canvas = Image.alpha_composite(canvas, with_alpha(rim, outline))
    body = vertical_gradient(size, [(0, (206, 74, 62)), (0.55, (150, 30, 34)), (1, (84, 14, 20))])
    canvas = Image.alpha_composite(canvas, with_alpha(body, heart_mask(size)))
    shine = Image.new("L", size, 0)
    ImageDraw.Draw(shine).ellipse([70, 60, 118, 100], fill=120)
    shine = shine.filter(ImageFilter.GaussianBlur(8))
    canvas = Image.alpha_composite(canvas, with_alpha(Image.new("RGBA", size, (255, 235, 220, 255)), shine))
    return canvas


def main():
    write_raw_blp = load_blp_writer()
    os.makedirs(OUT, exist_ok=True)
    assets = {
        "InfiniteDungeon-Pin": (build_pin(), 128),
        "InfiniteDungeon-Emblem": (build_emblem(), 128),
        "InfiniteDungeon-Swirl": (build_swirl(), 128),
        "InfiniteDungeon-Heart": (build_heart(), 64),
    }
    for name, (image, side) in assets.items():
        image = image.resize((side, side), Image.Resampling.LANCZOS)
        image.save(os.path.join(OUT, name + ".png"), optimize=True)
        write_raw_blp(image, os.path.join(OUT, name + ".blp"))
        print(f"Built {name}: {side}x{side}")


if __name__ == "__main__":
    main()
