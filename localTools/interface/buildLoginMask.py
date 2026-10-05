"""Cuts the login art into its depth layers: the near one (the terrace, the two hooded statues with their braziers,
the banners' poles, the near trees) and the two banners' cloth - what stands in front
of the sky, the far city and the mist.

    python buildLoginMask.py

Writes clientPatcher/assets/login/LoginLayers-mask.png, the size of LoginBackdropWide-source.png: red the near
layer, blue the banners' cloth. buildGlueArt.py makes each layer from it (and paints the far layer in where they
stood), so the login screen can move the far layer with the pointer, wave the banners, and draw the clouds, the
mist and the birds behind the statues. Run it again only if the art changes.

The cliffs on either side, with their bridges and towers, are far: they run on into the far city's cliffs and
bridges, and any cut through them would tear open as soon as the two parts moved apart. Only where the near trees
stand in front of them does GrabCut draw the line, along the trees' own edge.

GrabCut (OpenCV) from rough guides drawn on a 1600-pixel-wide preview of the art: what is surely sky or far city,
what is probably and what is surely foreground. The pieces too small to matter are dropped and the holes closed.
"""
import os

import cv2
import numpy as np

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SOURCE = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginBackdropWide-source.png')
MASK = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginLayers-mask.png')
PREVIEW_WIDTH = 1600

# Guides, in preview pixels
SURE_BACKGROUND = [
    [(0, 0), (1600, 0), (1600, 28), (0, 28)],                                   # the top of the sky
    [(600, 60), (1000, 60), (1000, 300), (600, 300)],                           # the far city between the statues
    [(150, 40), (380, 40), (380, 150), (150, 150)],                             # the sky over the left cliffs
    [(1250, 40), (1550, 40), (1550, 150), (1250, 150)],                         # and over the right ones
]
PROBABLE_FOREGROUND = [
    [(0, 330), (1600, 330), (1600, 449), (0, 449)],                             # the terrace and its balustrades
    [(395, 40), (550, 40), (560, 290), (555, 365), (410, 365), (390, 290)],     # the left statue on its pedestal
    [(1035, 40), (1195, 40), (1200, 290), (1195, 365), (1040, 365), (1030, 290)],  # the right one
    [(325, 150), (400, 150), (400, 345), (325, 345)],                           # the left banner
    [(1170, 150), (1250, 150), (1250, 350), (1170, 350)],                       # the right banner
    [(0, 200), (120, 200), (130, 449), (0, 449)],                               # the trees bottom left
    [(1470, 290), (1600, 290), (1600, 449), (1470, 449)],                       # and bottom right
]
SURE_FOREGROUND = [
    [(150, 380), (1450, 380), (1450, 449), (150, 449)],                         # the terrace's floor
    [(440, 90), (510, 90), (520, 330), (430, 330)],                             # the statues' bodies
    [(1080, 90), (1150, 90), (1160, 330), (1070, 330)],
]
# Surely far, over the guides above: the cliffs on either side with their bridges and towers, down to where the near
# trees stand in front of them, and a far tree on the cliff below the left statue
FAR_CLIFFS = [
    [(0, 40), (310, 40), (310, 290), (140, 290), (140, 195), (0, 195)],
    [(1270, 40), (1600, 40), (1600, 285), (1270, 285)],
    [(1515, 250), (1575, 250), (1575, 300), (1515, 300)],
    [(545, 280), (650, 280), (650, 331), (545, 331)],
]
LEAST_PIECE = 1500      # pixels: smaller pieces are dropped
# The waterfalls, in the art's own pixels (3642x1024, as the login screen knows them): each goes in the layer of the
# rocks around it, so it never slides off its cliff when the layers move apart. The login screen draws their flow
# over them (AccountLogin.lua FALLS, the same boxes and the layer this script prints for each).
ART_SIZE = (3642, 1024)
WATERFALLS = [
    (125, 416, 177, 568), (233, 160, 279, 326), (340, 146, 376, 303), (439, 480, 513, 687), (1437, 576, 1481, 678),
    (1501, 614, 1525, 690), (1657, 422, 1702, 520), (1762, 454, 1845, 574),
    (2174, 477, 2228, 574), (2852, 428, 2912, 584), (2995, 442, 3029, 539), (3251, 556, 3339, 704),
    (3348, 260, 3414, 436),
]
# Where the banners' cloth (what waves) hangs from its crossbar: the cloth itself is told apart by its colour inside
CLOTH = [
    [(339, 192), (393, 192), (393, 332), (381, 342), (345, 346), (339, 302)],
    [(1188, 190), (1244, 190), (1244, 346), (1226, 351), (1195, 331), (1188, 301)],
]


def main():
    image = cv2.imread(SOURCE)
    height, width = image.shape[:2]
    scale = width / PREVIEW_WIDTH

    def polygon(points):
        return np.array([[int(x * scale), int(y * scale)] for x, y in points], np.int32)

    mask = np.full((height, width), cv2.GC_PR_BGD, np.uint8)
    for points in SURE_BACKGROUND:
        cv2.fillPoly(mask, [polygon(points)], cv2.GC_BGD)
    for points in PROBABLE_FOREGROUND:
        cv2.fillPoly(mask, [polygon(points)], cv2.GC_PR_FGD)
    for points in SURE_FOREGROUND:
        cv2.fillPoly(mask, [polygon(points)], cv2.GC_FGD)
    for points in FAR_CLIFFS:
        cv2.fillPoly(mask, [polygon(points)], cv2.GC_BGD)
    background_model, foreground_model = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    cv2.grabCut(image, mask, None, background_model, foreground_model, 6, cv2.GC_INIT_WITH_MASK)

    foreground = np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 255, 0).astype(np.uint8)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(foreground)
    kept = np.zeros_like(foreground)
    for index in range(1, count):
        if stats[index, cv2.CC_STAT_AREA] > LEAST_PIECE:
            kept[labels == index] = 255
    kept = cv2.morphologyEx(kept, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))

    def region(polygons):
        area = np.zeros_like(kept)
        for points in polygons:
            cv2.fillPoly(area, [polygon(points)], 255)
        return area

    # The cloth: its deep red, the holes closed and filled (the gold sun on it), the two largest pieces, and the gold
    # trim along their edge. Around it, inside the banners' area, the poles and the crossbars (dark or gold) stay in
    # the near layer; what shows between the tatters is far.
    banners = region(CLOTH)
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV).astype(np.int32)
    hue, saturation, value = hsv[..., 0], hsv[..., 1], hsv[..., 2]
    red = ((hue <= 6) | (hue >= 165)) & (saturation >= 110) & (value >= 35) & (value <= 230)
    gold = (hue >= 12) & (hue <= 32) & (saturation >= 90) & (value >= 110)
    cloth = np.where(red & (banners > 0), 255, 0).astype(np.uint8)
    cloth = cv2.morphologyEx(cloth, cv2.MORPH_OPEN, np.ones((2, 2), np.uint8))
    cloth = cv2.morphologyEx(cloth, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))
    count, labels, stats, _ = cv2.connectedComponentsWithStats(cloth)
    filled = np.zeros_like(cloth)
    for index in sorted(range(1, count), key=lambda index: -stats[index, cv2.CC_STAT_AREA])[:len(CLOTH)]:
        contours, _ = cv2.findContours(np.where(labels == index, 255, 0).astype(np.uint8), cv2.RETR_EXTERNAL,
                                       cv2.CHAIN_APPROX_NONE)
        cv2.drawContours(filled, contours, -1, 255, -1)
    trim = gold & (cv2.dilate(filled, np.ones((5, 5), np.uint8)) > 0) & (banners > 0)
    cloth = np.where((filled > 0) | trim, 255, 0).astype(np.uint8)
    poles = np.where((banners > 0) & (cloth == 0) & (kept > 0) & ((value < 85) | gold), 255, 0).astype(np.uint8)
    near = np.where(((kept > 0) & (banners == 0)) | (poles > 0), 255, 0).astype(np.uint8)

    # Each waterfall joins the layer most of the rocks around it are in (the far one: left as it is)
    to_source = (width / ART_SIZE[0], height / ART_SIZE[1])
    print('FALLS (true: in the near layer):')
    for box in WATERFALLS:
        x0, y0 = int(box[0] * to_source[0]), int(box[1] * to_source[1])
        x1, y1 = int(box[2] * to_source[0]) + 1, int(box[3] * to_source[1]) + 1
        ring = np.zeros_like(kept)
        cv2.rectangle(ring, (x0 - 12, y0 - 12), (x1 + 12, y1 + 12), 255, -1)
        ring[y0:y1, x0:x1] = 0
        in_near = int(((ring > 0) & (near > 0)).sum())
        is_near = in_near * 2 > int((ring > 0).sum())
        if is_near:
            near[y0:y1, x0:x1] = 255
        print(f'    {{ {box[0]}, {box[1]}, {box[2]}, {box[3]}, {"true" if is_near else "false"} }},')
    layers = cv2.merge([cloth, np.zeros_like(near), near])        # OpenCV writes BGR: blue the cloth, red near
    cv2.imwrite(MASK, layers)
    print(f'Wrote {MASK} ({width}x{height}: near {int((near > 0).mean() * 100)}%, '
          f'cloth {(cloth > 0).mean() * 100:.1f}%)')


if __name__ == '__main__':
    main()
