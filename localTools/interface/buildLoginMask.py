"""Cuts the login art's foreground out: the terrace, the two hooded statues with their braziers and banners, the near
trees and the side cliffs - what stands in front of the sky, the far city and the mist.

    python buildLoginMask.py

Writes clientPatcher/assets/login/LoginForeground-mask.png (white: foreground), the size of
LoginBackdropWide-source.png. buildGlueArt.py lays it as the alpha of the foreground layer, so the login screen can
draw the clouds, the mist and the birds behind the statues. Run it again only if the art changes.

GrabCut (OpenCV) from rough guides drawn on a 1600-pixel-wide preview of the art: what is surely sky or far city,
what is probably and what is surely foreground. The pieces too small to matter are dropped and the holes closed.
"""
import os

import cv2
import numpy as np

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SOURCE = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginBackdropWide-source.png')
MASK = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginForeground-mask.png')
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
LEAST_PIECE = 1500      # pixels: smaller pieces are dropped


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
    background_model, foreground_model = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    cv2.grabCut(image, mask, None, background_model, foreground_model, 6, cv2.GC_INIT_WITH_MASK)

    foreground = np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 255, 0).astype(np.uint8)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(foreground)
    kept = np.zeros_like(foreground)
    for index in range(1, count):
        if stats[index, cv2.CC_STAT_AREA] > LEAST_PIECE:
            kept[labels == index] = 255
    kept = cv2.morphologyEx(kept, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))
    cv2.imwrite(MASK, kept)
    print(f'Wrote {MASK} ({width}x{height}, {int((kept > 0).mean() * 100)}% foreground)')


if __name__ == '__main__':
    main()
