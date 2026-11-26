"""Procedural details for the whole town: street furniture, vegetation, details on the buildings, details on the ground.

usage: python tools/details.py        (from the project folder; needs numpy and Pillow)

Reads the scene (terrain and its materials, the buildings, the roads from models/city/traffic.txt, the lamps, the
parked cars and the trees) and places, by rules:
  - on the roofs (flat roofs, material Roofing / krisha): air conditioning units, vents, antennas, lift machine rooms,
    water tanks on the tall blocks
  - around every building: on grass hedges, bushes and flower beds; on pavement benches, bins, planters, bike racks;
    canopies over the entrances; ivy on some walls
  - on the blocks: enclosed balconies in front of the windows of some columns; rows of metal garages in some yards
  - along the streets: bus stops, kiosks, hydrants, bins and benches on the sidewalks, manhole covers on the roads
  - on the ground everywhere in town: cracks, oil stains, fallen leaves under the trees, small bushes in the grass
  - Pripyat is abandoned: only wild vegetation, ivy and cracks there
Everything is made of boxes, cylinders and textured quads, with the town's existing textures plus a few textures
generated here (foliage, ivy, cracks, leaves, manholes...). Output: models/details/objects.obj (cast shadows) and
decals.obj (flat on the ground, no shadows), each with one shape per material and TILE x TILE tile, so the game can
skip the ones far from the camera.
"""
import math
import os
import random
from collections import defaultdict

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "models", "details")
TILE = 150.0
rng = random.Random(2026)

# ----------------------------------------------------------------------------------------------------------------
# reading the scene
# ----------------------------------------------------------------------------------------------------------------

def load_obj(path, offset=(0.0, 0.0, 0.0)):
    """-> vertices (n x 3, world), triangles: list of (object name, material, i0, i1, i2)."""
    verts = []
    tris = []
    obj, mtl = "?", "?"
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("v "):
                p = line.split()
                verts.append((float(p[1]), float(p[2]), float(p[3])))
            elif line.startswith("f "):
                idx = []
                for t in line.split()[1:]:
                    i = int(t.split("/")[0])
                    idx.append(i - 1 if i > 0 else len(verts) + i)
                for k in range(1, len(idx) - 1):
                    tris.append((obj, mtl, idx[0], idx[k], idx[k + 1]))
            elif line.startswith("o ") or line.startswith("g "):
                obj = line[2:].strip()
            elif line.startswith("usemtl"):
                mtl = line.split()[1]
    return np.array(verts, dtype=np.float64) + np.array(offset), tris


ROAD = {"GAsphalt", "GRoad", "RoadPaint"}
PAVE = {"PConcrete", "PSquare", "PHex", "PPatterned", "PCobbleColor", "PCobbleGrey", "ConcreteSM", "ConcreteSMDark",
        "Concrete"}
GRASS = {"GGrass", "GGrassRocky", "GForest", "Flowers"}
DIRT = {"GDirt", "GMudLeaves"}


def placed(V, place):
    """the vertices of a model placed like the program does (models/layout.txt: x y z yaw, glm::rotate around +Y)"""
    x, y, z, yaw = place
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return np.stack([V[:, 0] * c + V[:, 2] * s + x, V[:, 1] + y, -V[:, 0] * s + V[:, 2] * c + z], axis=1)


def read_layout():
    places = {}
    for line in open(os.path.join(ROOT, "models", "layout.txt")):
        p = line.split()
        if p and not p[0].startswith("#") and p[0] not in ("lamp", "floodlight") and len(p) >= 5:
            places[p[0]] = tuple(float(a) for a in p[1:5])
    return places


def category(mtl):
    if mtl in ROAD: return "road"
    if mtl in PAVE: return "pave"
    if mtl in GRASS: return "grass"
    if mtl in DIRT: return "dirt"
    return "other"


class Ground:
    """The walkable surfaces (terrain, sidewalks, plazas): height and material at any point."""
    CELL = 8.0

    def __init__(self):
        self.a, self.b, self.c, self.cat = [], [], [], []
        self.grid = defaultdict(list)

    def add(self, V, tris, only_up=True, max_height_over_terrain=None):
        for (_, mtl, i0, i1, i2) in tris:
            a, b, c = V[i0], V[i1], V[i2]
            n = np.cross(b - a, c - a)
            ln = np.linalg.norm(n)
            if ln < 1e-9:
                continue
            if only_up and abs(n[1]) / ln < 0.7:
                continue
            if max_height_over_terrain is not None:
                y = (a[1] + b[1] + c[1]) / 3.0
                t = self.height_terrain((a[0] + b[0] + c[0]) / 3.0, (a[2] + b[2] + c[2]) / 3.0)
                if t is None or y > t + max_height_over_terrain:
                    continue
            k = len(self.a)
            self.a.append(a); self.b.append(b); self.c.append(c); self.cat.append(category(mtl))
            x0, x1 = min(a[0], b[0], c[0]), max(a[0], b[0], c[0])
            z0, z1 = min(a[2], b[2], c[2]), max(a[2], b[2], c[2])
            for gx in range(int(math.floor(x0 / self.CELL)), int(math.floor(x1 / self.CELL)) + 1):
                for gz in range(int(math.floor(z0 / self.CELL)), int(math.floor(z1 / self.CELL)) + 1):
                    self.grid[(gx, gz)].append(k)

    def query(self, x, z, below=None):
        """-> (height, category) of the highest surface at (x, z) (not higher than `below`), or (None, None)."""
        best = (None, None)
        for k in self.grid.get((int(math.floor(x / self.CELL)), int(math.floor(z / self.CELL))), ()):
            a, b, c = self.a[k], self.b[k], self.c[k]
            d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2])
            if abs(d) < 1e-12:
                continue
            l1 = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / d
            l2 = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / d
            l3 = 1.0 - l1 - l2
            if l1 < -1e-6 or l2 < -1e-6 or l3 < -1e-6:
                continue
            y = l1 * a[1] + l2 * b[1] + l3 * c[1]
            if below is not None and y > below:
                continue
            if best[0] is None or y > best[0]:
                best = (y, self.cat[k])
        return best

    terrain = None

    def height_terrain(self, x, z):
        return Ground.terrain.query(x, z)[0] if Ground.terrain else None


class Building:
    def __init__(self, name, abandoned=False, landmark=False):
        self.name = name
        self.lo = np.array([1e9, 1e9, 1e9])
        self.hi = -self.lo
        self.roof = []           # roof triangles (world)
        self.walls = []          # ground-floor walls: (x0, z0, x1, z1, material) seen from above
        self.signs = False       # has shop signs (no canopies there)
        self.glass = []          # window panes above the ground floor (balconies go in front of some of them)
        self.abandoned = abandoned
        self.landmark = landmark

    def extend(self, pts):
        self.lo = np.minimum(self.lo, pts.min(axis=0))
        self.hi = np.maximum(self.hi, pts.max(axis=0))


def buildings_from(V, tris, prefix_of, abandoned=False, landmark=False, min_height=2.0):
    found = {}
    for (obj, mtl, i0, i1, i2) in tris:
        name = prefix_of(obj)
        b = found.setdefault(name, Building(name, abandoned, landmark))
        pts = V[[i0, i1, i2]]
        b.extend(pts)
        n = np.cross(pts[1] - pts[0], pts[2] - pts[0])
        ln = np.linalg.norm(n)
        if ln < 1e-9:
            continue
        if (mtl == "Roofing" or "krisha" in mtl) and abs(n[1]) / ln > 0.9:
            b.roof.append(pts)
        if mtl == "Signs":
            b.signs = True
        if mtl == "Glass" and abs(n[1]) / ln < 0.2 and pts[:, 1].min() > 3.0:
            b.glass.append(pts)
        if abs(n[1]) / ln < 0.2 and pts[:, 1].min() < 1.5 and pts[:, 1].max() > 2.0:
            # a wall at street level: its trace on the ground (the two farthest points of the triangle, seen from above)
            xz = pts[:, [0, 2]]
            pairs = [(0, 1), (1, 2), (0, 2)]
            i, j = max(pairs, key=lambda p: np.sum((xz[p[0]] - xz[p[1]]) ** 2))
            b.walls.append((xz[i][0], xz[i][1], xz[j][0], xz[j][1], mtl))
    return [b for b in found.values() if b.hi[1] - b.lo[1] >= min_height]


def wall_hit(b, x, z, dx, dz, max_dist=8.0):
    """the nearest wall of b along the ray (x, z) + t (dx, dz) -> (t, material) or None."""
    best = None
    for (x0, z0, x1, z1, mtl) in b.walls:
        ex, ez = x1 - x0, z1 - z0
        den = dx * ez - dz * ex
        if abs(den) < 1e-9:
            continue
        t = ((x0 - x) * ez - (z0 - z) * ex) / den
        u = ((x0 - x) * dz - (z0 - z) * dx) / den
        if 0 <= u <= 1 and 0 < t < max_dist and (best is None or t < best[0]):
            best = (t, mtl)
    return best


# ----------------------------------------------------------------------------------------------------------------
# geometry
# ----------------------------------------------------------------------------------------------------------------

class Mesh:
    """Triangles per material, cut into tiles; two kinds: objects (cast shadows) and decals (don't)."""

    def __init__(self):
        self.parts = defaultdict(lambda: {"v": [], "vt": [], "vn": []})   # (kind, tile, material) -> lists

    def tri(self, kind, mtl, p, uv, n, tile_of):
        key = (kind, tile_of, mtl)
        part = self.parts[key]
        for i in range(3):
            part["v"].append(p[i]); part["vt"].append(uv[i]); part["vn"].append(n)

    def quad(self, kind, mtl, p0, p1, p2, p3, uv=((0, 0), (1, 0), (1, 1), (0, 1)), double=False, tile_at=None):
        """p0..p3 counter-clockwise seen from the front."""
        p0, p1, p2, p3 = map(np.asarray, (p0, p1, p2, p3))
        n = np.cross(p1 - p0, p2 - p0)
        n = n / (np.linalg.norm(n) + 1e-12)
        c = (p0 + p1 + p2 + p3) / 4.0 if tile_at is None else np.asarray(tile_at)
        t = (int(math.floor(c[0] / TILE)), int(math.floor(c[2] / TILE)))
        self.tri(kind, mtl, (p0, p1, p2), (uv[0], uv[1], uv[2]), n, t)
        self.tri(kind, mtl, (p0, p2, p3), (uv[0], uv[2], uv[3]), n, t)
        if double:
            self.tri(kind, mtl, (p0, p2, p1), (uv[0], uv[2], uv[1]), -n, t)
            self.tri(kind, mtl, (p0, p3, p2), (uv[0], uv[3], uv[2]), -n, t)


M = Mesh()


def rot(yaw):
    c, s = math.cos(yaw), math.sin(yaw)
    return lambda x, z: (c * x - s * z, s * x + c * z)


def facing(fx, fz):
    """the yaw that turns an object's front (its local +z) towards (fx, fz); its local x is then (fz, -fx)"""
    return math.atan2(-fx, fz)


def box(mtl, center, size, yaw=0.0, uv_scale=1.0, tile_at=None, top_mtl=None):
    """center = middle of the bottom face; size = (along x, height, along z) before the yaw."""
    cx, cy, cz = center
    sx, sy, sz = size[0] / 2.0, size[1], size[2] / 2.0
    r = rot(yaw)
    def P(x, y, z):
        dx, dz = r(x, z)
        return (cx + dx, cy + y, cz + dz)
    u = lambda a: a / uv_scale
    anchor = tile_at if tile_at is not None else (cx, cy, cz)
    # sides (counter-clockwise from outside)
    M.quad("obj", mtl, P(-sx, 0, sz), P(sx, 0, sz), P(sx, sy, sz), P(-sx, sy, sz),
           ((0, 0), (u(2 * sx), 0), (u(2 * sx), u(sy)), (0, u(sy))), tile_at=anchor)
    M.quad("obj", mtl, P(sx, 0, -sz), P(-sx, 0, -sz), P(-sx, sy, -sz), P(sx, sy, -sz),
           ((0, 0), (u(2 * sx), 0), (u(2 * sx), u(sy)), (0, u(sy))), tile_at=anchor)
    M.quad("obj", mtl, P(sx, 0, sz), P(sx, 0, -sz), P(sx, sy, -sz), P(sx, sy, sz),
           ((0, 0), (u(2 * sz), 0), (u(2 * sz), u(sy)), (0, u(sy))), tile_at=anchor)
    M.quad("obj", mtl, P(-sx, 0, -sz), P(-sx, 0, sz), P(-sx, sy, sz), P(-sx, sy, -sz),
           ((0, 0), (u(2 * sz), 0), (u(2 * sz), u(sy)), (0, u(sy))), tile_at=anchor)
    M.quad("obj", top_mtl or mtl, P(-sx, sy, sz), P(sx, sy, sz), P(sx, sy, -sz), P(-sx, sy, -sz),
           ((0, 0), (u(2 * sx), 0), (u(2 * sx), u(2 * sz)), (0, u(2 * sz))), tile_at=anchor)


def cylinder(mtl, center, radius, height, segments=10, uv_scale=1.0, cap=True, tile_at=None):
    cx, cy, cz = center
    anchor = tile_at if tile_at is not None else center
    circumference = 2 * math.pi * radius
    for i in range(segments):
        a0, a1 = 2 * math.pi * i / segments, 2 * math.pi * (i + 1) / segments
        x0, z0 = cx + radius * math.cos(a0), cz + radius * math.sin(a0)
        x1, z1 = cx + radius * math.cos(a1), cz + radius * math.sin(a1)
        u0, u1 = circumference * i / segments / uv_scale, circumference * (i + 1) / segments / uv_scale
        M.quad("obj", mtl, (x1, cy, z1), (x0, cy, z0), (x0, cy + height, z0), (x1, cy + height, z1),
               ((u1, 0), (u0, 0), (u0, height / uv_scale), (u1, height / uv_scale)), tile_at=anchor)
        if cap:
            top = cy + height
            M.tri("obj", mtl, ((cx, top, cz), (x1, top, z1), (x0, top, z0)),
                  ((0.5, 0.5), (0.5 + 0.5 * math.cos(a1), 0.5 + 0.5 * math.sin(a1)),
                   (0.5 + 0.5 * math.cos(a0), 0.5 + 0.5 * math.sin(a0))), np.array([0.0, 1.0, 0.0]),
                  (int(math.floor(anchor[0] / TILE)), int(math.floor(anchor[2] / TILE))))


def decal(mtl, x, y, z, size, yaw, lift):
    r = rot(yaw)
    h = size / 2.0
    pts = []
    for (a, b) in ((-h, h), (h, h), (h, -h), (-h, -h)):
        dx, dz = r(a, b)
        pts.append((x + dx, y + lift, z + dz))
    M.quad("decal", mtl, *pts)


def crossed(mtl, x, y, z, width, height, yaw):
    """Two crossed vertical quads (a bush seen from any side), double-sided."""
    for k in range(2):
        r = rot(yaw + k * math.pi / 2)
        dx, dz = r(width / 2.0, 0.0)
        M.quad("obj", mtl, (x - dx, y, z - dz), (x + dx, y, z + dz), (x + dx, y + height, z + dz),
               (x - dx, y + height, z - dz), double=True, tile_at=(x, y, z))


# ----------------------------------------------------------------------------------------------------------------
# the objects
# ----------------------------------------------------------------------------------------------------------------

_models = {}


def model(name):
    """a model of models/details/<name>/<name>.obj (normalized by tools/buildings.py): its triangles per material,
    and its materials added to the ones written in details.mtl"""
    if name in _models:
        return _models[name]
    folder = os.path.join(OUT, name)
    V, VT, tris, mtl = [], [], [], None
    for line in open(os.path.join(folder, name + ".obj")):
        if line.startswith("v "):
            V.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("vt "):
            VT.append([float(x) for x in line.split()[1:3]])
        elif line.startswith("usemtl"):
            mtl = line.split()[1]
        elif line.startswith("f "):
            c = [t.split("/") for t in line.split()[1:4]]
            tris.append((mtl, [V[int(a[0]) - 1] for a in c], [VT[int(a[1]) - 1] for a in c]))
    tex = None
    for line in open(os.path.join(folder, name + ".mtl")):
        if line.startswith("newmtl"):
            current = line.split()[1]
        elif line.startswith("map_Kd"):
            MATERIALS[current] = name + "/" + line.split()[1]
    _models[name] = tris
    return tris


def place_model(name, x, y, z, n, scale=(1.0, 1.0, 1.0)):
    """the model's +Z along n (horizontal), its X along up x n, its origin at (x, y, z)"""
    n = np.array([n[0], 0.0, n[1]], dtype=float)
    n /= np.linalg.norm(n)
    d = np.cross([0.0, 1.0, 0.0], n)
    o = np.array([x, y, z])
    tile = (int(math.floor(x / TILE)), int(math.floor(z / TILE)))
    for (mtl, pts, uv) in model(name):
        w = [o + d * p[0] * scale[0] + np.array([0.0, p[1] * scale[1], 0.0]) + n * p[2] * scale[2] for p in pts]
        nn = np.cross(w[1] - w[0], w[2] - w[0])
        ln = np.linalg.norm(nn)
        if ln < 1e-12:
            continue
        M.tri("obj", mtl, w, uv, nn / ln, tile)


def bench(x, y, z, yaw):
    """the seat towards the local +z (see facing())"""
    r = rot(yaw)
    def at(a, b):
        dx, dz = r(a, b)
        return (x + dx, y, z + dz)
    for side in (-0.75, 0.75):            # concrete legs
        box("BenchConcrete", at(side, 0.0), (0.12, 0.42, 0.5), yaw)
    seat = at(0.0, 0.05)
    box("BenchWood", (seat[0], y + 0.42, seat[2]), (1.8, 0.06, 0.42), yaw, uv_scale=1.5)
    back = at(0.0, -0.2)
    box("BenchWood", (back[0], y + 0.55, back[2]), (1.8, 0.35, 0.05), yaw, uv_scale=1.5)


def trash_bin(x, y, z):
    cylinder("SteelGreen", (x, y, z), 0.28, 0.85, segments=10, uv_scale=1.0)
    cylinder("SteelDark", (x, y + 0.85, z), 0.3, 0.05, segments=10)


def planter(x, y, z, yaw):
    box("BenchConcrete", (x, y, z), (1.2, 0.5, 1.2), yaw, top_mtl="Soil")
    crossed("Bush", x, y + 0.45, z, 1.3, 1.0, yaw + 0.3)


def bike_rack(x, y, z, yaw):
    r = rot(yaw)
    for i in range(4):
        dx, dz = r(-0.9 + i * 0.6, 0.0)
        box("Steel", (x + dx, y, z + dz), (0.05, 0.8, 0.6), yaw)


def hedge(x, y, z, yaw, length):
    box("Hedge", (x, y, z), (length, 1.0, 0.7), yaw, uv_scale=1.5)


def bush(x, y, z, size):
    crossed("Bush", x, y - 0.05, z, size * 1.2, size, rng.uniform(0, math.pi))


def flower_bed(x, y, z, yaw):
    box("BenchConcrete", (x, y, z), (2.4, 0.45, 1.2), yaw, top_mtl="Flowers")
    r = rot(yaw)
    for k in range(3):   # a few low clumps of flowers on top
        dx, dz = r(-0.75 + 0.75 * k, rng.uniform(-0.2, 0.2))
        crossed("FlowerClump", x + dx, y + 0.42, z + dz, 0.7, 0.45, rng.uniform(0, math.pi))


def canopy(x, y, z, yaw):
    """Concrete slab over an entrance on two steel posts; (x, z) at the wall, yaw = facing(out of the wall)."""
    r = rot(yaw)
    dx, dz = r(0.0, 0.8)
    box("BenchConcrete", (x + dx, y + 2.8, z + dz), (3.2, 0.18, 1.6), yaw, uv_scale=2.0)
    for side in (-1.4, 1.4):
        px, pz = r(side, 1.45)
        cylinder("Steel", (x + px, y, z + pz), 0.05, 2.8, segments=6, cap=False)


def ivy(wall_a, wall_b, y, height, normal):
    """Ivy on a wall between two points, slightly in front of it."""
    off = np.asarray(normal) * 0.08
    a = np.asarray(wall_a) + off
    b = np.asarray(wall_b) + off
    width = np.linalg.norm(b - a)
    # one patch of ivy per quad (the texture has its own irregular outline: thick at the bottom, thinning upwards)
    M.quad("obj", "Ivy", (a[0], y, a[2]), (b[0], y, b[2]), (b[0], y + height, b[2]), (a[0], y + height, a[2]),
           ((0, 0), (1, 0), (1, 1), (0, 1)))


def hydrant(x, y, z):
    cylinder("PaintRed", (x, y, z), 0.13, 0.65, segments=8)
    cylinder("PaintRed", (x, y + 0.65, z), 0.09, 0.12, segments=8)
    box("PaintRed", (x, y + 0.4, z), (0.42, 0.1, 0.1), 0.0)


def bus_stop(x, y, z, towards_road):
    """(x, z) = middle of the shelter; its open side towards the road ("Old Soviet Bus Stop", denlog)"""
    place_model("busstop", x, y, z, towards_road)


def metal_garages(x, y, z, n, count):
    """a row of metal garages ("Soviet Metal Garage 3D", GGBotNet), side by side, doors towards n"""
    d = (n[1], -n[0])
    for k in range(count):
        t = (k - (count - 1) / 2.0) * 3.5
        place_model("metal_garage", x + d[0] * t, y, z + d[1] * t, n)


def balconies(b):
    """enclosed balconies ("Ussr balcony", DOMNIN) in front of the windows of some columns of a block"""
    # the panes: two triangles that share an edge are one window
    parent = {}
    def find(k):
        while parent.setdefault(k, k) != k:
            k = parent[k]
        return k
    keys = []
    for i, t in enumerate(b.glass):
        vk = [tuple(np.round(p, 2)) for p in t]
        keys.append(vk[0])
        for v in vk[1:]:
            parent[find(v)] = find(vk[0])
    windows = defaultdict(list)
    for i, t in enumerate(b.glass):
        windows[find(keys[i])].append(t)
    center = (b.lo + b.hi) / 2.0
    columns = defaultdict(list)
    for tris in windows.values():
        pts = np.concatenate(tris)
        big = max(tris, key=lambda t: np.linalg.norm(np.cross(t[1] - t[0], t[2] - t[0])))   # a pane, not its edge
        n = np.cross(big[1] - big[0], big[2] - big[0])
        n = np.array([n[0], 0.0, n[2]])
        if np.linalg.norm(n) < 1e-9:
            continue
        n /= np.linalg.norm(n)
        c = pts.mean(axis=0)
        if np.dot(c - center, n) < 0:
            n = -n
        d = np.cross([0.0, 1.0, 0.0], n)
        u = pts @ d
        width, y0, y1 = u.max() - u.min(), pts[:, 1].min(), pts[:, 1].max()
        if not (0.7 < width < 3.5 and 0.8 < y1 - y0 < 2.5):
            continue
        key = (round(math.degrees(math.atan2(n[0], n[2])) / 10), round(float(c @ n) * 2), round(float(c @ d) * 2))
        columns[key].append((c, n, width, y0, y1))
    count = 0
    for key, wins in sorted(columns.items()):
        if rng.random() > 0.3:
            continue
        for (c, n, width, y0, y1) in wins:
            bottom = y0 - 0.85
            place_model("balcony", c[0], bottom, c[2], (n[0], n[2]), (width + 0.6, min(2.6, y1 + 0.25 - bottom), 1.15))
            count += 1
    return count


def kiosk(x, y, z, yaw):
    color = rng.choice(["PaintBlue", "PaintGreen", "PaintYellow"])
    box(color, (x, y, z), (2.6, 2.5, 2.2), yaw, top_mtl="SteelDark")
    r = rot(yaw)
    # window and sign on the front (+z local)
    def at(a, b, h):
        dx, dz = r(a, b)
        return (x + dx, y + h, z + dz)
    M.quad("obj", "BusGlass", at(-1.0, 1.12, 0.9), at(1.0, 1.12, 0.9), at(1.0, 1.12, 1.9), at(-1.0, 1.12, 1.9))
    M.quad("obj", "KioskSign", at(-1.3, 1.13, 2.0), at(1.3, 1.13, 2.0), at(1.3, 1.13, 2.45), at(-1.3, 1.13, 2.45))


def ac_unit(x, y, z, yaw):
    box("Steel", (x, y, z), (1.3, 0.9, 0.9), yaw, top_mtl="SteelDark")


def vent(x, y, z):
    cylinder("SteelDark", (x, y, z), 0.25, 0.8, segments=8)
    cylinder("Steel", (x, y + 0.8, z), 0.38, 0.06, segments=8)


def antenna(x, y, z):
    h = rng.uniform(2.5, 5.0)
    cylinder("Steel", (x, y, z), 0.035, h, segments=5, cap=False)
    for k in range(rng.randint(2, 4)):
        box("Steel", (x, y + h * (0.55 + 0.13 * k), z), (rng.uniform(0.8, 1.6), 0.03, 0.03), rng.uniform(0, math.pi))


def machine_room(x, y, z, yaw):
    box("BenchConcrete", (x, y, z), (3.2, 2.4, 3.0), yaw, uv_scale=2.0, top_mtl="Roofing")


def water_tank(x, y, z):
    for (a, b) in ((-0.9, -0.9), (0.9, -0.9), (-0.9, 0.9), (0.9, 0.9)):
        box("Steel", (x + a, y, z + b), (0.12, 1.2, 0.12), 0.0)
    cylinder("SteelGreen", (x, y + 1.2, z), 1.4, 2.2, segments=14, uv_scale=2.0)


# ----------------------------------------------------------------------------------------------------------------
# placement
# ----------------------------------------------------------------------------------------------------------------

class Occupied:
    """Circles already taken (props, trees, new objects), to keep things from overlapping."""
    CELL = 10.0

    def __init__(self):
        self.grid = defaultdict(list)

    def add(self, x, z, r):
        for gx in range(int(math.floor((x - r) / self.CELL)), int(math.floor((x + r) / self.CELL)) + 1):
            for gz in range(int(math.floor((z - r) / self.CELL)), int(math.floor((z + r) / self.CELL)) + 1):
                self.grid[(gx, gz)].append((x, z, r))

    def free(self, x, z, r):
        for gx in range(int(math.floor((x - r) / self.CELL)), int(math.floor((x + r) / self.CELL)) + 1):
            for gz in range(int(math.floor((z - r) / self.CELL)), int(math.floor((z + r) / self.CELL)) + 1):
                for (ox, oz, orad) in self.grid.get((gx, gz), ()):
                    if (ox - x) ** 2 + (oz - z) ** 2 < (orad + r) ** 2:
                        return False
        return True


def inside_any(buildings_grid, x, z, margin):
    for b in buildings_grid.get((int(math.floor(x / 50.0)), int(math.floor(z / 50.0))), ()):
        if b.lo[0] - margin < x < b.hi[0] + margin and b.lo[2] - margin < z < b.hi[2] + margin:
            return True
    return False


def main():
    os.makedirs(OUT, exist_ok=True)
    print("reading the scene...")
    tV, tT = load_obj(os.path.join(ROOT, "models/terrain/terrain.obj"))
    terrain = Ground()
    terrain.add(tV, tT, only_up=False)
    Ground.terrain = terrain
    ground = Ground()
    ground.add(tV, tT, only_up=False)
    pV, pT = load_obj(os.path.join(ROOT, "models/city/city_props.obj"))
    ground.add(pV, pT, only_up=True, max_height_over_terrain=1.0)

    buildings = []
    town = lambda name: name.rsplit("_", 1)[0]
    for part in ("city_north", "city_south"):
        V, T = load_obj(os.path.join(ROOT, "models/city", part + ".obj"))
        ground.add(V, T, only_up=True, max_height_over_terrain=0.6)   # the plinths / paths around the blocks
        buildings += buildings_from(V, T, town)
    places = read_layout()
    landmarks = [("satu_mare/satu_mare.obj", places.get("satu_mare", (0, 0, 60, 0))),
                 ("genex/genex.obj", places.get("genex", (-420, 0, 196, 0))),
                 ("telefoane/telefoane.obj", places.get("telefoane", (-170, 0, 140, 0))),
                 ("romanita/romanita.obj", places.get("romanita", (200, 0, 280, 0))),
                 ("panorama/panorama.obj", places.get("panorama", (-100, 70, -520, 0)))]
    for path, place in landmarks:
        V, T = load_obj(os.path.join(ROOT, "models", path))
        V = placed(V, place)
        ground.add(V, T, only_up=True, max_height_over_terrain=0.6)
        whole = Building(os.path.basename(path), landmark=True)
        whole.extend(V)
        buildings.append(whole)
    V, T = load_obj(os.path.join(ROOT, "models/pripyat/pripyat_blocks.obj"))
    V = placed(V, places.get("pripyat", (-780, 0, 250, 0)))
    buildings += buildings_from(V, T, lambda name: "_".join(name.split("_")[:2]), abandoned=True)
    print(len(buildings), "buildings")

    grid = defaultdict(list)
    for b in buildings:
        for gx in range(int(math.floor((b.lo[0] - 5) / 50.0)), int(math.floor((b.hi[0] + 5) / 50.0)) + 1):
            for gz in range(int(math.floor((b.lo[2] - 5) / 50.0)), int(math.floor((b.hi[2] + 5) / 50.0)) + 1):
                grid[(gx, gz)].append(b)

    occupied = Occupied()
    trees = []
    for line in open(os.path.join(ROOT, "models/trees/trees_near.txt")):
        p = line.split()
        if len(p) >= 4 and not line.startswith("#"):
            trees.append((float(p[1]), float(p[2]), float(p[3])))
            occupied.add(float(p[1]), float(p[3]), 1.5)
    for line in open(os.path.join(ROOT, "models/city/props.txt")):
        p = line.split()
        if p and p[0] == "lamp":
            occupied.add(float(p[1]), float(p[3]), 0.6)
        elif p and p[0] == "car":
            occupied.add(float(p[2]), float(p[4]), 2.6)
    for (x, z) in [(-60, 130), (-20, 130), (20, 130), (60, 130), (-60, 170), (-20, 170), (20, 170), (60, 170),
                   (-195, 175), (-145, 175), (-455, 186), (-385, 186), (89.1, 186), (110.9, 206), (84, 35.06),
                   (116, 39.08)]:
        occupied.add(x, z, 0.6)

    # the entrances and the footpaths (tools/paths.py): nothing on them, nothing in front of a door
    doors, footpaths = [], []
    entrances = os.path.join(ROOT, "models/city/entrances.txt")
    if os.path.exists(entrances):
        for line in open(entrances):
            p = line.split()
            if p and p[0] == "door":
                doors.append((float(p[1]), float(p[2]), int(p[3]), int(p[4]), float(p[5]), p[6], p[7]))
            elif p and p[0] == "path":
                w = float(p[1])
                kind = "entrance"
                if not p[2].lstrip("-").replace(".", "").isdigit():
                    kind = p[2]
                    p = p[:2] + p[3:]
                c = [float(a) for a in p[2:]]
                footpaths.append((w, [(c[k], c[k + 1]) for k in range(0, len(c), 2)], kind))
    for (x, z, nx, nz, depth, kind, _) in doors:
        for t in (-2.0, -0.5, 1.0, 2.5, 4.0):   # (a canopy or a porch may stand out of the wall: from behind it)
            occupied.add(x + nx * t, z + nz * t, 1.8)
    for (w, pts, kind) in footpaths:
        for (ax, az), (bx, bz) in zip(pts[:-1], pts[1:]):
            L = math.hypot(bx - ax, bz - az)
            for k in range(int(L) + 1):
                t = k / max(int(L), 1)
                occupied.add(ax + (bx - ax) * t, az + (bz - az) * t, w / 2 + 0.3)
    # the props of the town (bus stops, benches, kiosks, fences, playgrounds...)
    for (_, mtl, i0, i1, i2) in pT:
        P = pV[[i0, i1, i2]]
        t = terrain.query(P[:, 0].mean(), P[:, 2].mean())[0]
        if t is None or P[:, 1].max() < t + 0.3 or P[:, 1].min() > t + 3.0:
            continue
        ext = max(P[:, 0].max() - P[:, 0].min(), P[:, 2].max() - P[:, 2].min())
        if ext < 6.0:
            occupied.add(P[:, 0].mean(), P[:, 2].mean(), ext / 2 + 0.3)
        else:   # a long fence / railing: circles along it
            a, b = P[np.argmin(P[:, 0] + P[:, 2])], P[np.argmax(P[:, 0] + P[:, 2])]
            n = int(ext / 2.0) + 1
            for k in range(n + 1):
                q = a + (b - a) * k / n
                occupied.add(q[0], q[2], 0.8)

    def ground_at(x, z):
        t = terrain.query(x, z)[0]
        if t is None:
            return None, None
        y, cat = ground.query(x, z, below=t + 1.0)
        if y is None or y < -1.9:   # under water
            return None, None
        return y, cat

    def place_ok(x, z, r, margin=0.4):
        return not inside_any(grid, x, z, margin) and occupied.free(x, z, r)

    counts = defaultdict(int)
    # --- benches along the park's paths: facing the path, on both sides by turns, a bin beside some -----------------
    for (w, pts, kind) in footpaths:
        if kind != "park":
            continue
        for (ax, az), (bx, bz) in zip(pts[:-1], pts[1:]):
            L = math.hypot(bx - ax, bz - az)
            dx, dz = (bx - ax) / L, (bz - az) / L
            for k, t in enumerate(np.arange(10.0, L - 6.0, 16.0)):
                side = 1 if k % 2 == 0 else -1
                nx, nz = -dz * side, dx * side           # from the path to the bench
                x, z = ax + dx * t + nx * (w / 2 + 0.8), az + dz * t + nz * (w / 2 + 0.8)
                y, cat = ground_at(x, z)
                if y is None or cat not in ("grass", "dirt") or any((tx - x) ** 2 + (tz - z) ** 2 < 2.0 ** 2
                                                                   for (tx, _, tz) in trees):
                    continue
                yaw = facing(-nx, -nz)
                bench(x, y, z, yaw); occupied.add(x, z, 1.1); counts["bench"] += 1
                if k % 2 == 0:
                    bx_, bz_ = x + math.cos(yaw) * 1.4, z + math.sin(yaw) * 1.4
                    trash_bin(bx_, y, bz_); occupied.add(bx_, bz_, 0.35); counts["bin"] += 1

    # --- roofs ---------------------------------------------------------------------------------------------------
    for b in buildings:
        if b.landmark or not b.roof:
            continue
        roof = np.concatenate(b.roof)
        lo, hi = roof.min(axis=0), roof.max(axis=0)
        if hi[0] - lo[0] < 6 or hi[2] - lo[2] < 6:
            continue
        roof_tris = b.roof
        def roof_height(x, z):
            """height of the roof at (x, z), or None if (x, z) is not on the roof (L-shaped / stepped roofs)."""
            for t in roof_tris:
                a, c, e = t
                d = (c[2] - e[2]) * (a[0] - e[0]) + (e[0] - c[0]) * (a[2] - e[2])
                if abs(d) < 1e-9:
                    continue
                l1 = ((c[2] - e[2]) * (x - e[0]) + (e[0] - c[0]) * (z - e[2])) / d
                l2 = ((e[2] - a[2]) * (x - e[0]) + (a[0] - e[0]) * (z - e[2])) / d
                if l1 >= 0 and l2 >= 0 and l1 + l2 <= 1:
                    return l1 * a[1] + l2 * c[1] + (1 - l1 - l2) * e[1]
            return None
        area = sum(0.5 * abs((t[1][0] - t[0][0]) * (t[2][2] - t[0][2]) - (t[2][0] - t[0][0]) * (t[1][2] - t[0][2]))
                   for t in roof_tris)
        taken = []
        def roof_spot(r):
            """a free spot whose whole footprint (center and corners, + a margin) is on one flat part of the roof"""
            for _ in range(40):
                x, z = rng.uniform(lo[0] + r, hi[0] - r), rng.uniform(lo[2] + r, hi[2] - r)
                y = roof_height(x, z)
                if y is None:
                    continue
                m = r + 1.2
                corners = [roof_height(x + sx * m, z + sz * m) for sx in (-1, 1) for sz in (-1, 1)]
                if any(cy is None or abs(cy - y) > 0.3 for cy in corners):
                    continue
                if all((x - a) ** 2 + (z - c) ** 2 > (r + d) ** 2 for (a, c, d) in taken):
                    taken.append((x, z, r))
                    return x, y, z
            return None
        yaw = 0.0 if hi[0] - lo[0] > hi[2] - lo[2] else math.pi / 2
        if b.hi[1] - b.lo[1] > 15 and not b.abandoned:
            for _ in range(1 + int(area > 800)):
                s = roof_spot(2.5)
                if s: machine_room(s[0], s[1], s[2], yaw); counts["machine room"] += 1
        if b.hi[1] - b.lo[1] > 25 and rng.random() < 0.6:
            s = roof_spot(2.0)
            if s: water_tank(s[0], s[1], s[2]); counts["water tank"] += 1
        if not b.abandoned:
            for _ in range(int(min(max(area / 140.0, 1), 10))):
                s = roof_spot(1.0)
                if s: ac_unit(s[0], s[1], s[2], yaw); counts["air conditioning"] += 1
        for _ in range(int(min(max(area / 300.0, 1), 5))):
            s = roof_spot(0.5)
            if s: vent(s[0], s[1], s[2]); counts["vent"] += 1
        for _ in range(rng.randint(1, 3)):
            s = roof_spot(0.8)
            if s: antenna(s[0], s[1], s[2]); counts["antenna"] += 1

    # --- around the buildings ------------------------------------------------------------------------------------
    for b in buildings:
        x0, x1, z0, z1 = b.lo[0], b.hi[0], b.lo[2], b.hi[2]
        if x1 - x0 > 400 or z1 - z0 > 400:
            continue
        sides = [((x0, z1), (x1, z1), (0.0, 1.0)), ((x1, z0), (x0, z0), (0.0, -1.0)),
                 ((x1, z1), (x1, z0), (1.0, 0.0)), ((x0, z0), (x0, z1), (-1.0, 0.0))]
        pave_side, pave_count = None, -1
        for (a, c, n) in sides:
            length = math.hypot(c[0] - a[0], c[1] - a[1])
            steps = max(int(length / 3.0), 1)
            d = (rng.uniform(1.6, 2.4))
            hedge_run = rng.random() < 0.5
            paves = 0
            grass_spots = []
            for k in range(steps):
                t = (k + 0.5) / steps
                x = a[0] + (c[0] - a[0]) * t + n[0] * d
                z = a[1] + (c[1] - a[1]) * t + n[1] * d
                y, cat = ground_at(x, z)
                if y is None or inside_any(grid, x, z, 0.3):
                    continue
                wall_yaw = facing(n[0], n[1])   # front away from the wall, local x along it
                if cat == "grass" or cat == "dirt":
                    grass_spots.append((x, y, z))
                    if b.abandoned:
                        if rng.random() < 0.45 and occupied.free(x, z, 0.8):
                            bush(x, y, z, rng.uniform(0.9, 2.2)); occupied.add(x, z, 0.8); counts["bush"] += 1
                    elif hedge_run and occupied.free(x, z, 1.4):
                        hedge(x, y, z, wall_yaw, 2.9); occupied.add(x, z, 1.4); counts["hedge"] += 1
                    elif rng.random() < 0.18 and occupied.free(x, z, 0.8):
                        bush(x, y, z, rng.uniform(0.8, 1.5)); occupied.add(x, z, 0.8); counts["bush"] += 1
                    elif rng.random() < 0.06 and occupied.free(x, z, 1.3):
                        flower_bed(x, y, z, wall_yaw); occupied.add(x, z, 1.3); counts["flower bed"] += 1
                elif cat == "pave":
                    paves += 1
                    if b.abandoned:
                        continue
                    roll = rng.random()
                    if roll < 0.10 and occupied.free(x, z, 1.1):
                        bench(x, y, z, wall_yaw); occupied.add(x, z, 1.1); counts["bench"] += 1
                        bx, bz = x + math.cos(wall_yaw) * 1.4, z + math.sin(wall_yaw) * 1.4   # beside it
                        if occupied.free(bx, bz, 0.35):
                            trash_bin(bx, y, bz); occupied.add(bx, bz, 0.35); counts["bin"] += 1
                    elif roll < 0.16 and occupied.free(x, z, 0.9):
                        planter(x, y, z, wall_yaw); occupied.add(x, z, 0.9); counts["planter"] += 1
                    elif roll < 0.19 and occupied.free(x, z, 1.1):
                        bike_rack(x, y, z, wall_yaw); occupied.add(x, z, 1.1); counts["bike rack"] += 1
            if paves > pave_count:
                pave_side, pave_count = (a, c, n), paves
            # ivy on walls facing grass: on the real wall (found by casting a ray inwards from the outline: the
            # outline may be the front of the balconies)
            if grass_spots and (b.abandoned or rng.random() < 0.3) and not b.landmark:
                for _ in range(rng.randint(1, 3 if b.abandoned else 2)):
                    t = rng.uniform(0.1, 0.8)
                    w = rng.uniform(2.0, 5.0) / max(length, 1.0)
                    ends = []
                    for tt in (t, min(t + w, 1.0)):
                        ox, oz = a[0] + (c[0] - a[0]) * tt + n[0] * 0.5, a[1] + (c[1] - a[1]) * tt + n[1] * 0.5
                        hit = wall_hit(b, ox, oz, -n[0], -n[1])
                        ends.append(None if hit is None else (ox - n[0] * hit[0], 0.0, oz - n[1] * hit[0]))
                    if None in ends or abs(np.dot(np.subtract(ends[0], ends[1]), (n[0], 0, n[1]))) > 0.05:
                        continue   # no wall there, or not one flat wall
                    gy = min(s[1] for s in grass_spots)
                    ivy(ends[0], ends[1], gy - 0.1, rng.uniform(3.0, 9.0 if b.abandoned else 6.0), (n[0], 0.0, n[1]))
                    counts["ivy"] += 1
        # balconies on the blocks (the windows of the upper floors), a row of metal garages in the yard of some
        residential = b.name.startswith(("Auto", "Block", "Tower", "ShopBlock")) and not b.landmark
        if residential and b.glass:
            counts["balcony"] += balconies(b)
        if residential and not b.abandoned and rng.random() < 0.35:
            for (a, c, n) in sorted(sides, key=lambda s_: rng.random()):
                if pave_side and (a, c, n) == pave_side:
                    continue
                gx, gz = (a[0] + c[0]) / 2 + n[0] * 11.0, (a[1] + c[1]) / 2 + n[1] * 11.0
                count = rng.randint(3, 6)
                d = (n[1], -n[0])
                spots = [(gx + d[0] * (k - (count - 1) / 2.0) * 3.5, gz + d[1] * (k - (count - 1) / 2.0) * 3.5)
                         for k in range(count)]
                grounds = [ground_at(px, pz) for px, pz in spots]
                if all(g[0] is not None and g[1] in ("grass", "dirt") for g in grounds) and \
                        all(place_ok(px, pz, 3.2) for px, pz in spots) and \
                        max(g[0] for g in grounds) - min(g[0] for g in grounds) < 0.6:
                    metal_garages(gx, min(g[0] for g in grounds), gz, (-n[0], -n[1]), count)
                    for px, pz in spots:
                        occupied.add(px, pz, 3.2)
                    counts["metal garage"] += count
                    break
    # --- canopies over the entrances of the procedural blocks (tools/paths.py found them; the downloaded
    # buildings have their own) ------------------------------------------------------------------------------------
    for (x, z, nx, nz, depth, kind, name) in doors:
        if kind not in ("landing", "stub", "side"):
            continue
        y = ground_at(x + nx * 1.0, z + nz * 1.0)[0]
        if y is None:
            continue
        canopy(x, y, z, facing(nx, nz)); counts["canopy"] += 1

    # --- along the streets ---------------------------------------------------------------------------------------
    lanes = []
    for line in open(os.path.join(ROOT, "models/city/traffic.txt")):
        p = line.split()
        if p and p[0] == "lane":
            n = int(p[4])
            pts = [(float(p[5 + 3 * i]), float(p[7 + 3 * i])) for i in range(n)]
            lanes.append(pts)
    # the same kind of thing not closer than its spacing (on either side of the street; the bus stops of the
    # opposite direction excepted)
    SPACING = {"bus stop": 260.0, "kiosk": 320.0, "hydrant": 80.0, "bin": 55.0, "bench": 90.0, "manhole": 40.0}
    placed_at = defaultdict(list)
    def far_enough(kind, x, z, f=None):
        for (px, pz, pf) in placed_at[kind]:
            if (px - x) ** 2 + (pz - z) ** 2 < SPACING[kind] ** 2:
                if kind == "bus stop" and f is not None and pf[0] * f[0] + pf[1] * f[1] < -0.5 and \
                        (px - x) ** 2 + (pz - z) ** 2 > 12.0 ** 2:
                    continue   # the stop of the other direction, across the street
                return False
        return True
    # the trolleybus line (tools/trolleybus.py): a shelter at each of its stops, nothing on its poles
    route = os.path.join(ROOT, "models/trolleybus/route.txt")
    if os.path.exists(route):
        for line in open(route):
            p = line.split()
            if p and p[0] == "pole":
                occupied.add(float(p[1]), float(p[2]), 0.6)
        for line in open(route):
            p = line.split()
            if p and p[0] == "stop":
                x, z, fx, fz = (float(a) for a in p[3:7])
                y = ground_at(x, z)[0]
                if y is not None and not inside_any(grid, x, z, 0.4):
                    bus_stop(x, y, z, (fx, fz)); occupied.add(x, z, 3.2)
                    placed_at["bus stop"].append((x - fx * 2.4, z - fz * 2.4, (fx, fz))); counts["bus stop"] += 1
    for pts in lanes:
        for i in range(len(pts) - 1):
            (ax, az), (bx, bz) = pts[i], pts[i + 1]
            seg = math.hypot(bx - ax, bz - az)
            if seg < 1e-3:
                continue
            dx, dz = (bx - ax) / seg, (bz - az) / seg
            nx, nz = dz, -dx    # right side of the direction of travel
            s = rng.uniform(0.0, 6.0)
            while s < seg:
                x, z = ax + dx * s, az + dz * s
                s += 6.0
                if abs(x) > 900 or abs(z) > 900:
                    continue
                y0, cat0 = ground_at(x, z)
                if y0 is None:
                    continue
                # manhole covers on the road
                mx, mz = x - nx * 1.2, z - nz * 1.2
                if cat0 == "road" and far_enough("manhole", mx, mz):
                    placed_at["manhole"].append((mx, mz, None))
                    decal("Manhole", mx, y0, mz, 0.8, rng.uniform(0, math.pi), 0.025)
                    counts["manhole"] += 1
                # the sidewalk: first pavement beyond the road on the right
                found = None
                left_road = False
                for k in range(6, 34):
                    ox, oz = x + nx * k * 0.5, z + nz * k * 0.5
                    y, cat = ground_at(ox, oz)
                    if cat == "road":
                        left_road = True
                    elif cat == "pave" and (left_road or k > 8):
                        found = (ox, y, oz, k * 0.5)
                        break
                if not found:
                    continue
                sx, sy, sz, dist = found
                wide = ground_at(sx + nx * 2.5, sz + nz * 2.5)[1] in ("pave", "grass")
                to_road = (-nx, -nz)
                road_yaw = facing(*to_road)   # the front towards the road
                if wide and far_enough("bus stop", sx, sz, to_road):
                    cx, cz = sx + nx * 1.6, sz + nz * 1.6
                    if place_ok(cx, cz, 3.2, 0.5):
                        bus_stop(cx, sy, cz, to_road); occupied.add(cx, cz, 3.2)
                        placed_at["bus stop"].append((sx, sz, to_road)); counts["bus stop"] += 1
                        continue
                if wide and far_enough("kiosk", sx, sz):
                    cx, cz = sx + nx * 2.0, sz + nz * 2.0
                    if place_ok(cx, cz, 2.0, 0.5):
                        kiosk(cx, sy, cz, road_yaw); occupied.add(cx, cz, 2.0)
                        placed_at["kiosk"].append((sx, sz, None)); counts["kiosk"] += 1
                        continue
                if far_enough("hydrant", sx, sz):
                    hx, hz = sx + nx * 0.4, sz + nz * 0.4
                    if place_ok(hx, hz, 0.3):
                        hydrant(hx, sy, hz); occupied.add(hx, hz, 0.3)
                        placed_at["hydrant"].append((sx, sz, None)); counts["hydrant"] += 1
                        continue
                if wide and far_enough("bench", sx, sz):
                    bx, bz = sx + nx * 1.5, sz + nz * 1.5
                    if place_ok(bx, bz, 1.1):
                        bench(bx, sy, bz, road_yaw); occupied.add(bx, bz, 1.1)
                        placed_at["bench"].append((sx, sz, None)); counts["bench"] += 1
                        # a bin beside it (along the bench: its local x)
                        tx, tz = bx + math.cos(road_yaw) * 1.4, bz + math.sin(road_yaw) * 1.4
                        if place_ok(tx, tz, 0.35):
                            trash_bin(tx, sy, tz); occupied.add(tx, tz, 0.35)
                            placed_at["bin"].append((tx, tz, None)); counts["bin"] += 1
                        continue
                if far_enough("bin", sx, sz):
                    tx, tz = sx + nx * 0.5, sz + nz * 0.5
                    if place_ok(tx, tz, 0.35):
                        trash_bin(tx, sy, tz); occupied.add(tx, tz, 0.35)
                        placed_at["bin"].append((sx, sz, None)); counts["bin"] += 1

    # --- on the ground -------------------------------------------------------------------------------------------
    lo = np.min([b.lo for b in buildings if not b.landmark], axis=0) - 60
    hi = np.max([b.hi for b in buildings if not b.landmark], axis=0) + 60
    lo, hi = np.maximum(lo, -900), np.minimum(hi, 900)
    tree_grid = Occupied()
    for (x, y, z) in trees:
        tree_grid.add(x, z, 7.0)
    for _ in range(int((hi[0] - lo[0]) * (hi[2] - lo[2]) / 180.0)):
        x, z = rng.uniform(lo[0], hi[0]), rng.uniform(lo[2], hi[2])
        y, cat = ground_at(x, z)
        if y is None or inside_any(grid, x, z, 0.2):
            continue
        near_tree = not tree_grid.free(x, z, 0.0)
        lift = rng.uniform(0.02, 0.035)
        roll = rng.random()
        if near_tree and cat in ("grass", "pave", "dirt", "road") and roll < 0.7:
            decal("Leaves", x, y, z, rng.uniform(1.5, 3.0), rng.uniform(0, 6.3), lift); counts["fallen leaves"] += 1
        elif cat == "road":
            if roll < 0.35:
                decal("Cracks", x, y, z, rng.uniform(1.5, 3.5), rng.uniform(0, 6.3), lift); counts["crack"] += 1
            elif roll < 0.5:
                decal("Oil", x, y, z, rng.uniform(0.8, 1.6), rng.uniform(0, 6.3), lift); counts["oil stain"] += 1
        elif cat == "pave":
            if roll < 0.35:
                decal("Cracks", x, y, z, rng.uniform(1.0, 2.5), rng.uniform(0, 6.3), lift); counts["crack"] += 1
        elif cat in ("grass", "dirt"):
            if roll < 0.04 and occupied.free(x, z, 1.0):
                for _ in range(rng.randint(2, 4)):
                    bx, bz = x + rng.uniform(-1.5, 1.5), z + rng.uniform(-1.5, 1.5)
                    by, bcat = ground_at(bx, bz)
                    if by is not None and bcat in ("grass", "dirt") and occupied.free(bx, bz, 0.5):
                        bush(bx, by, bz, rng.uniform(0.6, 1.3)); counts["bush"] += 1
                occupied.add(x, z, 2.0)
    # Pripyat: overgrown
    for b in buildings:
        if not b.abandoned:
            continue
        for _ in range(int((b.hi[0] - b.lo[0] + b.hi[2] - b.lo[2]) / 4)):
            x, z = rng.uniform(b.lo[0] - 15, b.hi[0] + 15), rng.uniform(b.lo[2] - 15, b.hi[2] + 15)
            y, cat = ground_at(x, z)
            if y is None or inside_any(grid, x, z, 0.5):
                continue
            if cat in ("grass", "dirt") and occupied.free(x, z, 0.7):
                bush(x, y, z, rng.uniform(0.8, 2.5)); occupied.add(x, z, 0.7); counts["bush"] += 1
            elif cat in ("pave", "road"):
                decal("Cracks", x, y, z, rng.uniform(2.0, 4.0), rng.uniform(0, 6.3), rng.uniform(0.02, 0.035))
                counts["crack"] += 1

    for k, v in sorted(counts.items()):
        print("  %-18s %d" % (k, v))
    write_textures()
    write_output()


# ----------------------------------------------------------------------------------------------------------------
# textures (generated) and output
# ----------------------------------------------------------------------------------------------------------------

def leaf_texture(size, density, colors, round_shape=True, seed=1):
    r = random.Random(seed)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = cy = size / 2
    for _ in range(density):
        if round_shape:
            a, rr = r.uniform(0, 2 * math.pi), (r.random() ** 0.6) * size * 0.45
            x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.9 + size * 0.03
        else:
            x, y = r.uniform(0, size), r.uniform(0, size)
        w, h = r.uniform(size * 0.025, size * 0.06), r.uniform(size * 0.015, size * 0.035)
        c = r.choice(colors)
        shade = r.uniform(0.6, 1.15)
        col = tuple(int(min(255, v * shade)) for v in c) + (255,)
        ang = r.uniform(0, 180)
        leaf = Image.new("RGBA", (int(w * 2) + 2, int(w * 2) + 2), (0, 0, 0, 0))
        ImageDraw.Draw(leaf).ellipse((w - w, w - h, w + w, w + h), fill=col)
        leaf = leaf.rotate(ang, resample=Image.BICUBIC)
        img.alpha_composite(leaf, (int(x - w), int(y - w)))
    return img


def write_textures():
    greens = [(52, 86, 30), (66, 102, 38), (40, 70, 26), (80, 110, 45), (58, 92, 34)]
    bush = leaf_texture(256, 1400, greens, True, 1)
    bush.save(os.path.join(OUT, "bush.png"))
    clump = leaf_texture(256, 900, greens, True, 8)
    d = ImageDraw.Draw(clump)
    r = random.Random(9)
    for _ in range(260):   # blossoms over the leaves
        a, rr = r.uniform(0, 2 * math.pi), (r.random() ** 0.7) * 100
        x, y = 128 + math.cos(a) * rr, 128 + math.sin(a) * rr * 0.9
        if clump.getpixel((int(min(max(x, 0), 255)), int(min(max(y, 0), 255))))[3] == 0:
            continue
        w = r.uniform(4, 8)
        c = r.choice([(220, 40, 50), (240, 200, 40), (235, 120, 160), (250, 250, 240), (160, 70, 200)])
        d.ellipse((x - w, y - w, x + w, y + w), fill=c + (255,))
    clump.save(os.path.join(OUT, "flower_clump.png"))
    hedge = leaf_texture(256, 3000, greens, False, 2)
    base = Image.new("RGBA", (256, 256), (28, 46, 18, 255))
    base.alpha_composite(hedge)
    base.convert("RGB").save(os.path.join(OUT, "hedge.jpg"), quality=92)
    # ivy: a patch climbing from the ground, its outline irregular (the leaves get rarer upwards and to the sides)
    ivy = Image.new("RGBA", (256, 512), (0, 0, 0, 0))
    r = random.Random(3)
    d = ImageDraw.Draw(ivy)
    stems = []
    for _ in range(9):   # stems climbing up from the bottom, wandering sideways
        x, y = r.uniform(40, 216), 512.0
        top = r.uniform(60, 300)
        while y > top:
            nx, ny = min(max(x + r.uniform(-14, 14), 8), 248), y - r.uniform(10, 22)
            d.line((x, y, nx, ny), fill=(70, 55, 35, 255), width=2)
            stems.append((nx, ny))
            x, y = nx, ny
    leaf_colors = [(45, 80, 28), (60, 95, 35), (38, 66, 24), (75, 105, 40), (52, 88, 30)]
    for _ in range(2600):   # leaves along the stems, crowded at the bottom
        sx, sy = r.choice(stems)
        if r.random() > 0.35 + 0.65 * (sy / 512.0):
            continue
        x, y = sx + r.gauss(0, 14), sy + r.gauss(0, 10)
        w = r.uniform(5, 10)
        d.ellipse((x - w, y - w * 0.8, x + w, y + w * 0.8), fill=r.choice(leaf_colors) + (255,))
    ivy.save(os.path.join(OUT, "ivy.png"))

    cracks = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    d = ImageDraw.Draw(cracks)
    r = random.Random(4)
    def crack(x, y, a, length, width):
        for _ in range(int(length)):
            nx, ny = x + math.cos(a) * 4, y + math.sin(a) * 4
            d.line((x, y, nx, ny), fill=(22, 22, 20, 255), width=max(1, int(width)))
            x, y, a = nx, ny, a + r.uniform(-0.5, 0.5)
            width *= 0.985
            if r.random() < 0.05 and width > 1.2:
                crack(x, y, a + r.choice((-1, 1)) * r.uniform(0.5, 1.2), length * 0.5, width * 0.7)
    for _ in range(3):
        crack(128 + r.uniform(-30, 30), 128 + r.uniform(-30, 30), r.uniform(0, 6.3), 35, 3.0)
    cracks.save(os.path.join(OUT, "cracks.png"))

    fallen = leaf_texture(256, 260, [(150, 90, 30), (170, 120, 40), (120, 60, 25), (180, 150, 60), (95, 70, 35)],
                          True, 5)
    fallen.save(os.path.join(OUT, "fallen_leaves.png"))

    manhole = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    d = ImageDraw.Draw(manhole)
    d.ellipse((4, 4, 124, 124), fill=(48, 46, 44, 255))
    d.ellipse((10, 10, 118, 118), fill=(62, 60, 57, 255))
    for k in range(14, 116, 9):
        d.line((k, 14, k, 114), fill=(42, 40, 38, 255), width=3)
        d.line((14, k, 114, k), fill=(42, 40, 38, 255), width=3)
    mask = Image.new("L", (128, 128), 0)
    ImageDraw.Draw(mask).ellipse((4, 4, 124, 124), fill=255)
    manhole.putalpha(mask)
    manhole.save(os.path.join(OUT, "manhole.png"))

    noise = np.random.RandomState(6).rand(32, 32)
    blot = Image.fromarray((noise * 255).astype(np.uint8)).resize((128, 128), Image.BICUBIC).filter(
        ImageFilter.GaussianBlur(6))
    a = np.asarray(blot).astype(np.float32) / 255.0
    yy, xx = np.mgrid[0:128, 0:128]
    radial = 1.0 - np.sqrt((xx - 64) ** 2 + (yy - 64) ** 2) / 64.0
    alpha = ((a * 0.6 + radial * 0.9) > 0.75).astype(np.uint8) * 255
    oil = np.zeros((128, 128, 4), np.uint8)
    oil[..., 0:3] = (18, 17, 16)
    oil[..., 3] = alpha
    Image.fromarray(oil, "RGBA").save(os.path.join(OUT, "oil.png"))

    soil = Image.fromarray((np.random.RandomState(7).rand(64, 64, 3) * 30 + np.array([45, 33, 22])).astype(np.uint8))
    soil.resize((128, 128), Image.NEAREST).save(os.path.join(OUT, "soil.jpg"), quality=90)

    sign = Image.new("RGB", (128, 128), (235, 190, 30))
    d = ImageDraw.Draw(sign)
    d.rectangle((6, 6, 121, 121), outline=(30, 30, 30), width=5)
    d.rounded_rectangle((24, 34, 104, 84), radius=8, fill=(30, 30, 30))   # a bus
    d.rectangle((32, 42, 96, 60), fill=(235, 190, 30))
    d.ellipse((34, 78, 50, 94), fill=(30, 30, 30))
    d.ellipse((78, 78, 94, 94), fill=(30, 30, 30))
    sign.save(os.path.join(OUT, "bus_sign.jpg"), quality=92)

    kiosk_sign = Image.new("RGB", (512, 96), (200, 40, 35))
    d = ImageDraw.Draw(kiosk_sign)
    from PIL import ImageFont
    try:
        f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 52)
    except OSError:
        f = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 52)
    l, t, r, b = d.textbbox((0, 0), "СОЮЗПЕЧАТЬ", font=f)
    d.text((256 - (l + r) / 2, 48 - (t + b) / 2), "СОЮЗПЕЧАТЬ", font=f, fill=(250, 245, 230))
    kiosk_sign.save(os.path.join(OUT, "kiosk_sign.jpg"), quality=92)


MATERIALS = {
    "BenchConcrete": "../city/concrete_sm.jpg", "BenchWood": "../city/wood_slats.jpg", "Steel": "../city/steel.jpg",
    "SteelDark": "../city/steel_dark.jpg", "SteelGreen": "../city/steel_green.jpg", "PaintRed": "../city/paint_red.jpg",
    "PaintBlue": "../city/paint_blue.jpg", "PaintGreen": "../city/paint_green.jpg",
    "PaintYellow": "../city/paint_yellow.jpg", "Flowers": "../city/flowers.jpg", "Roofing": "../city/roofing.jpg",
    "BusGlass": "../city/glass.jpg", "Bush": "bush.png", "FlowerClump": "flower_clump.png", "Hedge": "hedge.jpg", "Ivy": "ivy.png", "Soil": "soil.jpg",
    "Cracks": "cracks.png", "Leaves": "fallen_leaves.png", "Manhole": "manhole.png", "Oil": "oil.png",
    "BusSign": "bus_sign.jpg", "KioskSign": "kiosk_sign.jpg",
}


def write_output():
    with open(os.path.join(OUT, "details.mtl"), "w") as f:
        f.write("# materials of the procedural details (tools/details.py)\n")
        for name, tex in MATERIALS.items():
            f.write("newmtl %s\nNs 16\nKa 1 1 1\nKd 1 1 1\nKs 0.5 0.5 0.5\nillum 2\nmap_Kd %s\n" % (name, tex))
            f.write("map_Ks ../city/spec_low.jpg\n\n")
    # two models: the objects (cast shadows) and the decals (flat on the ground, no shadows); in each, one shape per
    # material and tile ("<material>_<i>_<j>"), so the game can cull them one by one
    for kind, name in (("obj", "objects.obj"), ("decal", "decals.obj")):
        with open(os.path.join(OUT, name), "w") as f:
            f.write("# procedural details (tools/details.py): one shape per material and %d m tile\n" % TILE)
            f.write("mtllib details.mtl\n")
            base = 1
            for (k, tile, mtl), part in sorted(M.parts.items(), key=lambda kv: (kv[0][1], kv[0][2])):
                if k != kind:
                    continue
                f.write("o %s_%d_%d\n" % (mtl, tile[0], tile[1]))
                for p in part["v"]:
                    f.write("v %.3f %.3f %.3f\n" % tuple(p))
                for t in part["vt"]:
                    f.write("vt %.3f %.3f\n" % tuple(t))
                for n in part["vn"]:
                    f.write("vn %.3f %.3f %.3f\n" % tuple(n))
                f.write("usemtl %s\n" % mtl)
                for i in range(0, len(part["v"]), 3):
                    a, b, c = base + i, base + i + 1, base + i + 2
                    f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))
                base += len(part["v"])
    total = sum(len(p["v"]) // 3 for p in M.parts.values())
    print(len(M.parts), "meshes,", total, "triangles")


if __name__ == "__main__":
    main()
