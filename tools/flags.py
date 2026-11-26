"""The flags that wave in the wind: their cloth, their poles and where they stand.

usage: python tools/flags.py        (from the project folder; needs numpy and Pillow)

  - models/flags/flag_<name>.obj: a cloth of FLAG_CELLS quads (the vertex shader bends it in the wind, basic.vert /
    shadow.vert); local space: x along the flag from the pole (0 .. length), y down from its top edge (0 .. -height),
    the front towards +z (the back: the same vertices, the normals turned around); u = x / length (0 at the pole)
  - the textures of the Soviet flags (USSR, Moldavian SSR: red, a green band 3:2:3, the gold hammer and sickle and
    the star in the canton), drawn here; Romania and the EU: the textures of Satu Mare
  - models/flags/flagpole.obj: a white pole with a golden ball
  - models/flags/flags.txt: where the flags fly (Scene.cpp reads it)
  - the flat flags of Satu Mare taken out of satu_mare.obj (they wave now, from the same poles)
"""
import math
import os
import shutil

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "models", "flags")
CELLS = (24, 12)            # quads along, down
GOLD = (255, 204, 0)
RED = (204, 0, 0)
GREEN = (0, 128, 0)

# name: (texture, length, height)
FLAGS = {
    "ussr": ("flag_ussr.png", 3.0, 1.5),
    "mssr": ("flag_mssr.png", 3.0, 1.5),
    "ro": ("flag_ro.jpg", 2.4, 1.6),
    "eu": ("flag_eu.jpg", 2.4, 1.6),
}
# where they fly: flag, the model they stand on (its placement in models/layout.txt) or "world", top of the flag
# (x, y, z), pole (1: a pole is drawn here too)
PLACES = [
    ("eu", "satu_mare", (-72.0, 8.5, 39.0), 0),   # the poles of Satu Mare are part of its model
    ("ro", "satu_mare", (-68.0, 8.5, 39.0), 0),
    ("ro", "satu_mare", (-64.0, 8.5, 39.0), 0),
    ("ussr", "world", (188.0, 9.6, 157.0), 1),     # in front of the House of Culture
    ("mssr", "world", (200.0, 9.6, 157.0), 1),
    ("ussr", "world", (212.0, 9.6, 157.0), 1),
]
POLE_HEIGHT = 10.0


def star(cx, cy, r_out, r_in, rot=-math.pi / 2):
    pts = []
    for k in range(10):
        r = r_out if k % 2 == 0 else r_in
        a = rot + k * math.pi / 5
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def hammer_sickle(d, cx, cy, s):
    """the hammer and the sickle, crossed, about s x s pixels around (cx, cy)"""
    # the sickle: a thick arc open to the upper right, with a short handle down to the left
    r = 0.36 * s
    box = (cx - r, cy - r - 0.04 * s, cx + r, cy + r - 0.04 * s)
    d.arc(box, start=-35, end=215, fill=GOLD, width=int(0.1 * s))
    # a point at the blade's tip (upper right) and the handle (lower left)
    a = math.radians(-35)
    tip = (cx + r * math.cos(a), cy - 0.04 * s + r * math.sin(a))
    d.polygon([tip, (tip[0] + 0.02 * s, tip[1] - 0.12 * s), (tip[0] - 0.08 * s, tip[1] + 0.02 * s)], fill=GOLD)
    hx, hy = cx - 0.22 * s, cy + 0.28 * s
    handle = [(hx, hy), (hx - 0.2 * s, hy + 0.2 * s)]
    d.line(handle, fill=GOLD, width=int(0.11 * s))
    # the hammer: a handle from lower right to the middle, a head across its top
    def rot(p, a, o):
        x, y = p[0] - o[0], p[1] - o[1]
        return (o[0] + x * math.cos(a) - y * math.sin(a), o[1] + x * math.sin(a) + y * math.cos(a))
    o = (cx, cy)
    ang = math.radians(-45)
    shaft = [(-0.05 * s, -0.25 * s), (0.05 * s, -0.25 * s), (0.05 * s, 0.5 * s), (-0.05 * s, 0.5 * s)]
    head = [(-0.24 * s, -0.4 * s), (0.18 * s, -0.4 * s), (0.18 * s, -0.24 * s), (-0.24 * s, -0.24 * s)]
    for shape in (shaft, head):
        d.polygon([rot((cx + x, cy + y), ang, o) for (x, y) in shape], fill=GOLD)


def soviet_flag(path, green_band):
    W, H = 2048, 1024
    img = Image.new("RGB", (W, H), RED)
    d = ImageDraw.Draw(img)
    if green_band:   # red 3 : green 2 : red 3
        d.rectangle((0, H * 3 // 8, W, H * 5 // 8), fill=GREEN)
    # the canton: the hammer and sickle, the star above
    s = H * 0.26
    cx, cy = H * 0.26, H * 0.3
    hammer_sickle(d, cx, cy, s)
    sx, sy, sr = cx, cy - 0.62 * s, 0.17 * s
    d.polygon(star(sx, sy, sr, sr * 0.38), fill=GOLD)
    d.polygon(star(sx, sy, sr * 0.72, sr * 0.72 * 0.38), fill=RED)   # a red star with a gold border
    img = img.resize((1024, 512), Image.LANCZOS)
    img.save(path)


def write_cloth(name, texture, L, H):
    nu, nv = CELLS
    V, VT = [], []
    for j in range(nv + 1):
        for i in range(nu + 1):
            V.append((L * i / nu, -H * j / nv, 0.0))
            VT.append((i / nu, 1.0 - j / nv))
    idx = lambda i, j: j * (nu + 1) + i + 1
    with open(os.path.join(OUT, "flag_%s.obj" % name), "w") as f:
        f.write("# tools/flags.py: the cloth of a flag (bent in the wind by the vertex shader)\n")
        f.write("mtllib flag_%s.mtl\n" % name)
        for v in V:
            f.write("v %.4f %.4f %.4f\n" % v)
        for t in VT:
            f.write("vt %.4f %.4f\n" % t)
        f.write("vn 0 0 1\nvn 0 0 -1\n")
        f.write("o Flag_%s\nusemtl Flag%s\n" % (name, name.upper()))
        for j in range(nv):
            for i in range(nu):
                a, b, c, d = idx(i, j), idx(i, j + 1), idx(i + 1, j + 1), idx(i + 1, j)
                f.write("f %d/%d/1 %d/%d/1 %d/%d/1\n" % (a, a, b, b, c, c))
                f.write("f %d/%d/1 %d/%d/1 %d/%d/1\n" % (a, a, c, c, d, d))
        f.write("o Flag_%s_back\nusemtl Flag%s\n" % (name, name.upper()))
        for j in range(nv):
            for i in range(nu):
                a, b, c, d = idx(i, j), idx(i, j + 1), idx(i + 1, j + 1), idx(i + 1, j)
                f.write("f %d/%d/2 %d/%d/2 %d/%d/2\n" % (a, a, c, c, b, b))
                f.write("f %d/%d/2 %d/%d/2 %d/%d/2\n" % (a, a, d, d, c, c))
    with open(os.path.join(OUT, "flag_%s.mtl" % name), "w") as f:
        f.write("newmtl Flag%s\nKa 1 1 1\nKd 1 1 1\nKs 0.1 0.1 0.1\nNs 8\nmap_Kd %s\n" % (name.upper(), texture))


def write_pole():
    """a white pole (8 sides) with a golden ball on top"""
    V, F, mats = [], [], []
    def ring(y, r, n=8):
        start = len(V)
        for k in range(n):
            a = 2 * math.pi * k / n
            V.append((r * math.cos(a), y, r * math.sin(a)))
        return start
    n = 8
    b, t = ring(0.0, 0.07), ring(POLE_HEIGHT, 0.045)
    for k in range(n):
        k2 = (k + 1) % n
        F.append(("Pole", (b + k, t + k, t + k2, b + k2)))
    # the ball: an octahedron-ish sphere
    c = (0.0, POLE_HEIGHT + 0.12, 0.0)
    rings = []
    for lat in range(1, 6):
        phi = math.pi * lat / 6
        rings.append(ring(c[1] + 0.12 * math.cos(phi), 0.12 * math.sin(phi)))
    top = len(V); V.append((0, c[1] + 0.12, 0))
    bot = len(V); V.append((0, c[1] - 0.12, 0))
    for k in range(n):
        k2 = (k + 1) % n
        F.append(("Ball", (top, rings[0] + k2, rings[0] + k)))
        F.append(("Ball", (bot, rings[-1] + k, rings[-1] + k2)))
        for r in range(len(rings) - 1):
            F.append(("Ball", (rings[r] + k, rings[r] + k2, rings[r + 1] + k2, rings[r + 1] + k)))
    with open(os.path.join(OUT, "flagpole.obj"), "w") as f:
        f.write("# tools/flags.py: a flag pole\nmtllib flagpole.mtl\n")
        for v in V:
            f.write("v %.4f %.4f %.4f\n" % v)
        f.write("vt 0 0\n")
        for mat in ("Pole", "Ball"):
            f.write("o Flagpole_%s\nusemtl Flagpole%s\n" % (mat, mat))
            for (m, face) in F:
                if m == mat:
                    # the outward normals: the loader computes none, so flat ones from the face
                    P = [np.array(V[i]) for i in face]
                    nrm = np.cross(P[1] - P[0], P[2] - P[0])
                    f.write("vn %.4f %.4f %.4f\n" % tuple(nrm / (np.linalg.norm(nrm) + 1e-12)))
                    f.write("f " + " ".join("%d/1/-1" % (i + 1) for i in face) + "\n")
    with open(os.path.join(OUT, "flagpole.mtl"), "w") as f:
        f.write("newmtl FlagpolePole\nKa 1 1 1\nKd 1 1 1\nKs 0.4 0.4 0.4\nNs 64\nmap_Kd pole_white.png\n\n")
        f.write("newmtl FlagpoleBall\nKa 1 1 1\nKd 1 1 1\nKs 0.9 0.9 0.9\nNs 128\nmap_Kd pole_gold.png\n")
    Image.new("RGB", (4, 4), (235, 235, 230)).save(os.path.join(OUT, "pole_white.png"))
    Image.new("RGB", (4, 4), (212, 165, 40)).save(os.path.join(OUT, "pole_gold.png"))


def remove_flat_flags():
    path = os.path.join(ROOT, "models", "satu_mare", "satu_mare.obj")
    lines = open(path, encoding="utf-8", errors="ignore").read().split("\n")
    out, skip = [], False
    for l in lines:
        if l.startswith("o "):
            skip = l[2:].strip().startswith(("Flags_FlagEU", "Flags_FlagRO"))
            if skip:
                continue
        if skip and (l.startswith("f ") or l.startswith("usemtl") or l.startswith("s ")):
            continue
        out.append(l)
    if len(out) != len(lines):
        open(path, "w", encoding="utf-8").write("\n".join(out))
        print("the flat flags taken out of satu_mare.obj")


def main():
    os.makedirs(OUT, exist_ok=True)
    soviet_flag(os.path.join(OUT, "flag_ussr.png"), False)
    soviet_flag(os.path.join(OUT, "flag_mssr.png"), True)
    for t in ("flag_ro.jpg", "flag_eu.jpg"):
        shutil.copy(os.path.join(ROOT, "models", "satu_mare", t), os.path.join(OUT, t))
    for name, (tex, L, H) in FLAGS.items():
        write_cloth(name, tex, L, H)
    write_pole()
    with open(os.path.join(OUT, "flags.txt"), "w") as f:
        f.write("# tools/flags.py: flag <name> <on: a model of layout.txt, or world> <x y z: the top of the flag at "
                "the pole> <pole: 1 = draw a pole there>\n")
        for (name, on, (x, y, z), pole) in PLACES:
            f.write("flag %s %s %.2f %.2f %.2f %d\n" % (name, on, x, y, z, pole))
    remove_flat_flags()
    print("flags written to", OUT)


if __name__ == "__main__":
    main()
