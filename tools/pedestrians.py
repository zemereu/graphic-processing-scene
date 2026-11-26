"""The pedestrians: their body parts (moved by the program as they walk) and the ways they walk along.

usage: python tools/pedestrians.py      (from the project folder; needs numpy, Pillow; tools/paths.py next to it)

  - models/pedestrians/body.obj (torso, neck, head, hair), leg.obj, arm.obj: a person 1.75 m tall facing +x (like the
    vehicles), y up; a leg / an arm hangs from its pivot (the hip, the shoulder) at the origin, so the program swings
    it by turning it around z; the clothes ("...Paint" materials) get their color per person (uniform paintColor)
  - models/pedestrians/walks.txt: the ways: the middle of the sidewalks along the traffic lanes (the right-hand side
    of each lane) and the footpaths of tools/paths.py (models/city/entrances.txt), each as a polyline on the ground;
    a person walks one of them to its end and back
"""
import math
import os

import numpy as np
from PIL import Image

from paths import M, PAVE, ROAD, NX, NZ, Raster, cell, read_obj

OUT = M("pedestrians")
TOWN = (-450.0, 650.0, -400.0, 880.0)   # x0 x1 z0 z1: the walks only in the town


class Obj:
    def __init__(self):
        self.V, self.N, self.F = [], [], []   # F: (material, [(v, n)])

    def tri(self, mat, a, b, c):
        a, b, c = (np.array(p, float) for p in (a, b, c))
        n = np.cross(b - a, c - a)
        ln = np.linalg.norm(n)
        if ln < 1e-12:
            return
        base = len(self.V)
        self.V += [a, b, c]
        self.N.append(n / ln)
        k = len(self.N)
        self.F.append((mat, [(base, k), (base + 1, k), (base + 2, k)]))

    def quad(self, mat, a, b, c, d):
        self.tri(mat, a, b, c)
        self.tri(mat, a, c, d)

    def box(self, mat, x0, x1, y0, y1, z0, z1, taper=1.0):
        """a box; its top face scaled by taper in x and z (around the middle)"""
        cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
        tx0, tx1 = cx + (x0 - cx) * taper, cx + (x1 - cx) * taper
        tz0, tz1 = cz + (z0 - cz) * taper, cz + (z1 - cz) * taper
        b = [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]
        t = [(tx0, y1, tz0), (tx1, y1, tz0), (tx1, y1, tz1), (tx0, y1, tz1)]
        self.quad(mat, b[0], b[1], b[2], b[3])        # bottom (seen from below)
        self.quad(mat, t[3], t[2], t[1], t[0])        # top
        for k in range(4):
            k2 = (k + 1) % 4
            self.quad(mat, b[k], t[k], t[k2], b[k2])

    def sphere(self, mat, c, r, n=10, m=7, ry=1.0, ymin=-1.0):
        """a sphere (r, ry: the vertical radius), only the part above ymin (of the unit sphere)"""
        def p(i, j):
            th = math.pi * j / m
            ph = 2 * math.pi * i / n
            return (c[0] + r * math.sin(th) * math.cos(ph), c[1] + r * ry * math.cos(th), c[2] + r * math.sin(th) * math.sin(ph))
        for j in range(m):
            if math.cos(math.pi * (j + 1) / m) < ymin and math.cos(math.pi * j / m) < ymin:
                continue
            for i in range(n):
                a, b, cc, d = p(i, j), p(i + 1, j), p(i + 1, j + 1), p(i, j + 1)
                self.quad(mat, a, d, cc, b)

    def fix_winding(self, center):
        """every face facing away from `center` (x, y, z)"""
        for k, (mat, corners) in enumerate(self.F):
            P = [self.V[v] for v, n in corners]
            nrm = self.N[corners[0][1] - 1]
            if np.dot(nrm, np.mean(P, axis=0) - np.array(center)) < 0:
                self.N[corners[0][1] - 1] = -nrm
                self.F[k] = (mat, corners[::-1])

    def write(self, name, mats):
        with open(os.path.join(OUT, name + ".obj"), "w") as f:
            f.write("# tools/pedestrians.py\nmtllib %s.mtl\n" % name)
            for v in self.V:
                f.write("v %.4f %.4f %.4f\n" % tuple(v))
            f.write("vt 0.5 0.5\n")
            for n in self.N:
                f.write("vn %.4f %.4f %.4f\n" % tuple(n))
            for mat in sorted({m for m, c in self.F}):
                f.write("o %s_%s\nusemtl %s\n" % (name, mat, mat))
                for (m, corners) in self.F:
                    if m == mat:
                        f.write("f " + " ".join("%d/1/%d" % (v + 1, n) for v, n in corners) + "\n")
        with open(os.path.join(OUT, name + ".mtl"), "w") as f:
            for mat in sorted({m for m, c in self.F}):
                f.write("newmtl %s\nKa 1 1 1\nKd 1 1 1\nKs 0.05 0.05 0.05\nNs 8\nmap_Kd %s\n\n" % (mat, mats[mat]))


def write_models():
    os.makedirs(OUT, exist_ok=True)
    for name, color in (("white.png", (255, 255, 255)), ("skin.png", (224, 180, 150)), ("hair.png", (60, 42, 30)),
                        ("shoes.png", (30, 28, 26))):
        Image.new("RGB", (4, 4), color).save(os.path.join(OUT, name))
    mats = {"CoatPaint": "white.png", "TrousersPaint": "white.png", "Skin": "skin.png", "Hair": "hair.png",
            "Shoes": "shoes.png", "SleevePaint": "white.png"}
    # the body: the torso (wider at the shoulders), the neck, the head, the hair
    body = Obj()
    body.box("CoatPaint", -0.12, 0.12, 0.86, 1.46, -0.19, 0.19, taper=1.12)
    body.box("CoatPaint", -0.1, 0.1, 1.42, 1.48, -0.23, 0.23)            # the shoulders
    body.fix_winding((0, 1.15, 0))
    neck = Obj()
    neck.box("Skin", -0.05, 0.05, 1.46, 1.55, -0.05, 0.05)
    neck.sphere("Skin", (0.01, 1.64, 0.0), 0.105, ry=1.15)
    neck.fix_winding((0.01, 1.6, 0))
    hair = Obj()
    hair.sphere("Hair", (-0.005, 1.66, 0.0), 0.112, ry=1.1, ymin=0.05)
    hair.fix_winding((0, 1.66, 0))
    for part in (neck, hair):
        off = len(body.V)
        noff = len(body.N)
        body.V += part.V
        body.N += part.N
        body.F += [(m, [(v + off, n + noff) for v, n in c]) for m, c in part.F]
    body.write("body", mats)
    # a leg from the hip (origin) down: the trouser leg, the shoe
    leg = Obj()
    leg.box("TrousersPaint", -0.075, 0.075, -0.82, 0.0, -0.075, 0.075, taper=1.2)
    leg.fix_winding((0, -0.4, 0))
    shoe = Obj()
    shoe.box("Shoes", -0.08, 0.16, -0.9, -0.82, -0.06, 0.06)
    shoe.fix_winding((0.04, -0.86, 0))
    off, noff = len(leg.V), len(leg.N)
    leg.V += shoe.V; leg.N += shoe.N
    leg.F += [(m, [(v + off, n + noff) for v, n in c]) for m, c in shoe.F]
    leg.write("leg", mats)
    # an arm from the shoulder (origin) down: the sleeve, the hand
    arm = Obj()
    arm.box("SleevePaint", -0.05, 0.05, -0.56, 0.0, -0.05, 0.05, taper=1.25)
    arm.fix_winding((0, -0.28, 0))
    hand = Obj()
    hand.box("Skin", -0.04, 0.04, -0.66, -0.56, -0.035, 0.035)
    hand.fix_winding((0, -0.61, 0))
    off, noff = len(arm.V), len(arm.N)
    arm.V += hand.V; arm.N += hand.N
    arm.F += [(m, [(v + off, n + noff) for v, n in c]) for m, c in hand.F]
    arm.write("arm", mats)


def write_walks():
    tV, tF = read_obj(M("terrain", "terrain.obj"))
    road, pave = Raster(), Raster()
    height = Raster("F", -100.0)
    rows = []
    for (o, m, idx) in tF:
        for k in range(1, len(idx) - 1):
            P = tV[[idx[0], idx[k], idx[k + 1]]]
            if P[:, 0].max() < TOWN[0] or P[:, 0].min() > TOWN[1] or P[:, 2].max() < TOWN[2] or P[:, 2].min() > TOWN[3]:
                continue
            if m in ROAD or o.startswith("Parking_GAsphalt"):
                road.tri(P, 1)
            elif m in PAVE:
                pave.tri(P, 1)
            rows.append((P[:, 1].max(), P))
    rows.sort(key=lambda r: r[0])
    for y, P in rows:
        height.tri(P, float(P[:, 1].mean()))
    road, pave, height = road.array().astype(bool), pave.array().astype(bool), height.array()
    def get(grid, x, z):
        i, j = cell(x, z)
        return grid[i, j] if 0 <= i < NZ and 0 <= j < NX else 0
    walks = []
    # the sidewalks: along every lane, on its right, the middle of the first pavement past the road
    lanes = []
    for line in open(M("city", "traffic.txt")):
        p = line.split()
        if p and p[0] == "lane" and int(p[2]) < 0:
            n = int(p[4])
            lanes.append(np.array([float(a) for a in p[5:5 + 3 * n]]).reshape(-1, 3))
    for P in lanes:
        cur = []
        d_all = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))])
        for s in np.arange(0.0, d_all[-1], 3.0):
            k = int(np.clip(np.searchsorted(d_all, s) - 1, 0, len(P) - 2))
            t = (s - d_all[k]) / max(d_all[k + 1] - d_all[k], 1e-6)
            q = P[k] + (P[k + 1] - P[k]) * t
            d = (P[k + 1] - P[k])[[0, 2]]
            d /= max(np.linalg.norm(d), 1e-9)
            right = np.array([-d[1], d[0]])
            x, z = q[0], q[2]
            if not (TOWN[0] < x < TOWN[1] and TOWN[2] < z < TOWN[3]):
                found = None
            else:
                found = None
                t0 = None
                left_road = False
                for k2 in range(2, 40):
                    a = np.array([x, z]) + right * k2 * 0.5
                    if get(road, *a):
                        left_road = True
                    elif get(pave, *a) and left_road:
                        if t0 is None:
                            t0 = k2 * 0.5
                    elif t0 is not None:
                        found = (t0 + k2 * 0.5) / 2
                        break
                    elif left_road and k2 * 0.5 > 6.0:
                        break
            if found is None or found > 18.0:
                if len(cur) >= 8:
                    walks.append(cur)
                cur = []
                continue
            a = np.array([x, z]) + right * found
            cur.append((a[0], get(height, *a) + 0.02, a[1]))
        if len(cur) >= 8:
            walks.append(cur)
    n_side = len(walks)
    # the footpaths
    for line in open(M("city", "entrances.txt")):
        p = line.split()
        if p and p[0] == "path":
            c = [float(a) for a in p[3:]]
            pts = [(c[k], c[k + 1]) for k in range(0, len(c), 2)]
            dense = []
            for (ax, az), (bx, bz) in zip(pts[:-1], pts[1:]):
                L = math.hypot(bx - ax, bz - az)
                for k in range(int(L / 2.0) + 1):
                    t = k / max(int(L / 2.0), 1)
                    x, z = ax + (bx - ax) * t, az + (bz - az) * t
                    if not dense or math.hypot(x - dense[-1][0], z - dense[-1][2]) > 0.5:
                        dense.append((x, get(height, x, z) + 0.02, z))
            if len(dense) >= 4 and sum(math.hypot(b[0] - a[0], b[2] - a[2]) for a, b in zip(dense[:-1], dense[1:])) > 12:
                walks.append(dense)
    with open(os.path.join(OUT, "walks.txt"), "w") as f:
        f.write("# tools/pedestrians.py: walk <n> <x y z> ...: the middle of a sidewalk or a footpath\n")
        for w in walks:
            f.write("walk %d " % len(w) + " ".join("%.2f %.2f %.2f" % p for p in w) + "\n")
    print("%d walks (%d sidewalks, %d footpaths)" % (len(walks), n_side, len(walks) - n_side))


if __name__ == "__main__":
    write_models()
    write_walks()
