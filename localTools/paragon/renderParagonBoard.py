"""Renders the whole paragon board - all four zones - to a PNG, the way the frame's zoom-out shows it.

It reads the board straight from buildParagonTree.build(), so what it draws is what the generator writes: the three
inner zones as a small bright disc, the Panthéon's ray, ring and twelve sigils around it, sockets and hearts marked.
Used to check the sigils' geometry (clean, symmetric, nothing crossing) before anything ships.

Usage: python localTools/paragon/renderParagonBoard.py [output.png] [--size 2400] [--sigil N] [--allocated]
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import buildParagonTree as tree  # noqa: E402

GOLD = (255, 205, 110)
PALE = (150, 170, 230)
ZONE_TINT = {0: (215, 200, 170), 1: (220, 180, 120), 2: (235, 160, 90), 3: (200, 190, 255)}


def font(size):
    for name in ("MORPHEUS.TTF", "georgia.ttf", "times.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def main():
    args = sys.argv[1:]
    output = next((a for a in args if a.endswith(".png")), os.path.join(tree.REPO, "paragon-board.png"))
    size = int(args[args.index("--size") + 1]) if "--size" in args else 2400
    only_sigil = int(args[args.index("--sigil") + 1]) if "--sigil" in args else 0
    allocated = "--allocated" in args

    nodes, links, sigils = tree.build()
    by_id = {n["id"]: n for n in nodes}

    if only_sigil:
        # A single rune, turned upright (its outward side up) so its symmetry can be checked by eye
        sigil = next(s for s in sigils if s["id"] == only_sigil)
        turn = math.radians(90.0) - math.atan2(sigil["y"], sigil["x"])
        for n in nodes:
            dx, dy = n["x"] - sigil["x"], n["y"] - sigil["y"]
            n["x"] = sigil["x"] + dx * math.cos(turn) - dy * math.sin(turn)
            n["y"] = sigil["y"] + dx * math.sin(turn) + dy * math.cos(turn)
        centre = (sigil["x"], sigil["y"])
        half = 800
    else:
        centre = (0.0, 0.0)
        half = max(max(abs(n["x"]), abs(n["y"])) for n in nodes) + tree.BOARD_MARGIN
    scale = size / (2.0 * half)

    def to_px(x, y):
        return ((x - centre[0]) * scale + size / 2.0, size / 2.0 - (y - centre[1]) * scale)

    image = Image.new("RGB", (size, size), (6, 6, 14))

    # The sky: a faint nebula and scattered stars in the Panthéon's annulus, the inner zones on marble-dark ground
    if not only_sigil:
        nebula = Image.new("RGB", (size, size), (0, 0, 0))
        draw = ImageDraw.Draw(nebula)
        rng = random.Random(7)
        for _ in range(260):
            r = rng.uniform(2400, 4800)
            a = rng.uniform(0, 2 * math.pi)
            x, y = to_px(r * math.cos(a), r * math.sin(a))
            radius = rng.uniform(80, 260) * scale
            tint = rng.choice([(40, 30, 80), (20, 40, 90), (60, 30, 60), (30, 30, 50)])
            draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=tint)
        nebula = nebula.filter(ImageFilter.GaussianBlur(size / 60))
        image = Image.blend(image, nebula, 0.6)
        draw = ImageDraw.Draw(image)
        for _ in range(3000):
            r = rng.uniform(2300, 4900)
            a = rng.uniform(0, 2 * math.pi)
            x, y = to_px(r * math.cos(a), r * math.sin(a))
            v = rng.randint(90, 230)
            dot = rng.choice([0, 0, 0, 1])
            draw.ellipse((x - dot, y - dot, x + dot, y + dot), fill=(v, v, min(255, v + 25)))
        inner = 2350 * scale
        cx, cy = to_px(0, 0)
        draw.ellipse((cx - inner, cy - inner, cx + inner, cy + inner), fill=(22, 19, 16))
    draw = ImageDraw.Draw(image)

    complete = set()
    if allocated:
        complete = {s["id"] for s in sigils if s["id"] % 2}

    lw = max(1, int(round(7 * scale)))
    for a, b in links:
        na, nb = by_id[a], by_id[b]
        if only_sigil and not (na.get("sigil") == only_sigil or nb.get("sigil") == only_sigil):
            continue
        sky = na.get("zone", 0) == 3 or nb.get("zone", 0) == 3
        same_sigil = na.get("sigil") and na.get("sigil") == nb.get("sigil")
        if same_sigil:
            colour = GOLD if na["sigil"] in complete else (120, 110, 190)
            width = max(1, int(round((10 if na["sigil"] in complete else 8) * scale)))
        elif sky:
            colour, width = (90, 100, 150), lw
        else:
            colour, width = (150, 130, 95), lw
        draw.line([to_px(na["x"], na["y"]), to_px(nb["x"], nb["y"])], fill=colour, width=width)

    for n in nodes:
        if only_sigil and n.get("sigil") != only_sigil:
            continue
        radius = {0: 22, 1: 30, 2: 40}[n["type"]] * scale
        radius = max(radius, 1.2)
        x, y = to_px(n["x"], n["y"])
        colour = ZONE_TINT[n.get("zone", 0)]
        if n["effect"] == tree.E_SOCKET:
            colour = (255, 140, 30)
            radius *= 1.4
        elif n.get("sigil") and n["type"] == tree.KEYSTONE:
            colour = GOLD
        elif n.get("sigil"):
            colour = GOLD if n["sigil"] in complete else (180, 175, 255)
        elif n.get("bridge"):
            colour = (120, 220, 255)
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=colour)

    label = font(max(12, int(size / 70)))
    small = font(max(10, int(size / 110)))
    for s in sigils:
        if only_sigil and s["id"] != only_sigil:
            continue
        x, y = to_px(s["x"], s["y"])
        away = 760 * scale
        norm = math.hypot(s["x"], s["y"]) or 1
        lx, ly = to_px(s["x"] + s["x"] / norm * 760, s["y"] + s["y"] / norm * 760)
        draw.text((lx, ly), s["titan"], fill=(230, 215, 170), font=label, anchor="mm")
        if only_sigil:
            draw.text((size / 2, size - 40), "%s - %d stars, %d points" % (
                s["titan"], len(s["nodes"]), sum(by_id[i]["cost"] for i in s["nodes"])), fill=GOLD, font=small,
                anchor="mm")
        del away

    if not only_sigil:
        title = font(max(16, int(size / 30)))
        x, y = to_px(0, 2700)
        draw.text((x, y), "Le Panthéon", fill=(240, 220, 170), font=title, anchor="mm")
        points = sum(n["cost"] for n in nodes if n.get("zone") == 3)
        count = sum(1 for n in nodes if n.get("zone") == 3)
        draw.text((20, size - 30), "%d nodes, %d links; Panthéon %d nodes, %d points" % (
            len(nodes), len(links), count, points), fill=(200, 200, 200), font=small)

    image.save(output)
    print(output)


if __name__ == "__main__":
    main()
