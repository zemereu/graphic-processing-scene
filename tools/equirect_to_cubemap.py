"""Cuts an equirectangular (360 x 180 degrees) sky panorama into the 6 faces of an OpenGL cube map.

usage: python equirect_to_cubemap.py panorama.jpg output_folder [face_size] [sun_u]

  face_size  pixels per face (default: a quarter of the panorama width, at most 2048)
  sun_u      horizontal position of the sun in the panorama (0..1); the faces are rotated so that the sun
             ends up towards +Z, the azimuth of lightDir in Lighting.cpp. Default 0.5 (no rotation).

Writes right/left/top/bottom/back/front.jpg (+X, -X, +Y, -Y, +Z, -Z), the order Skybox.cpp loads them in.
Needs numpy and Pillow (pip install numpy pillow).
"""
import os
import sys

import numpy as np
from PIL import Image

# OpenGL cube map conventions: for a face pixel (sc, tc) in [-1, 1] (s to the right, t downwards in the image),
# the world direction it shows
FACES = {
    "right":  lambda sc, tc: (np.ones_like(sc), -tc, -sc),   # +X
    "left":   lambda sc, tc: (-np.ones_like(sc), -tc, sc),   # -X
    "top":    lambda sc, tc: (sc, np.ones_like(sc), tc),     # +Y
    "bottom": lambda sc, tc: (sc, -np.ones_like(sc), -tc),   # -Y
    "back":   lambda sc, tc: (sc, -tc, np.ones_like(sc)),    # +Z
    "front":  lambda sc, tc: (-sc, -tc, -np.ones_like(sc)),  # -Z
}


def sample_bilinear(img, u, v):
    """img: H x W x 3 floats; u, v in [0, 1] (u wraps around horizontally)."""
    h, w, _ = img.shape
    x = u * w - 0.5
    y = np.clip(v * h - 0.5, 0, h - 1)
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    x0 %= w
    x1 = (x0 + 1) % w
    y1 = np.minimum(y0 + 1, h - 1)
    top = img[y0, x0] * (1 - fx) + img[y0, x1] * fx
    bottom = img[y1, x0] * (1 - fx) + img[y1, x1] * fx
    return top * (1 - fy) + bottom * fy


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    src, out_dir = sys.argv[1], sys.argv[2]
    pano = np.asarray(Image.open(src).convert("RGB")).astype(np.float32)
    size = int(sys.argv[3]) if len(sys.argv) > 3 else min(pano.shape[1] // 4, 2048)
    sun_u = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5
    os.makedirs(out_dir, exist_ok=True)

    t = (np.arange(size) + 0.5) / size * 2.0 - 1.0
    sc, tc = np.meshgrid(t, t)   # sc varies along a row, tc down the columns
    for name, direction in FACES.items():
        x, y, z = direction(sc, tc)
        length = np.sqrt(x * x + y * y + z * z)
        x, y, z = x / length, y / length, z / length
        # azimuth measured from +Z; turning right (towards +X when looking down -Z) moves right in the panorama
        azimuth = np.arctan2(x, z)
        u = (sun_u - azimuth / (2.0 * np.pi)) % 1.0
        v = 0.5 - np.arcsin(np.clip(y, -1.0, 1.0)) / np.pi
        face = sample_bilinear(pano, u, v)
        Image.fromarray(np.clip(face + 0.5, 0, 255).astype(np.uint8)).save(
            os.path.join(out_dir, name + ".jpg"), quality=95)
        print("wrote", os.path.join(out_dir, name + ".jpg"))


if __name__ == "__main__":
    main()
