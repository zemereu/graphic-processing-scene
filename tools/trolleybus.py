"""The trolleybus line: its route through the town, its stops, the overhead wires and their poles, the poles on the
roof of the trolleybus.

usage: python tools/trolleybus.py      (from the project folder; needs numpy, Pillow; tools/paths.py next to it)
       then python tools/details.py (it puts the bus stops of the line at its stops and keeps off the poles)

  - the route: a loop of traffic lanes (models/city/traffic.txt) through WAYPOINTS (the shortest way from one to the
    next, Dijkstra over the lanes), driven clockwise (right-hand traffic)
  - the stops: about every STOP_SPACING m, on a straight lane, where there is a sidewalk on the right
  - the wires: two (the trolleybus takes the current from one and gives it back to the other), WIRE_HEIGHT above the
    lane, 0.6 m apart, along every lane of the route (the curves through the junctions too); poles every
    POLE_SPACING m beside the road, with an arm over the lane that holds the wires
Output: models/trolleybus/route.txt (Scene.cpp: the route, the stops; details.py: the stops and the poles),
models/trolleybus/wires.obj (+ .mtl), models/trolleybus/trolley_poles.obj (on the roof, in the bus's own space).
"""
import heapq
import math
import os

import numpy as np
from PIL import Image

from paths import M, PAVE, ROAD, NX, NZ, Raster, cell, read_obj

OUT = M("trolleybus")
# the loop (x, z, heading x, heading z): the main road (east), the east avenue (south), the southern road (west), the
# western avenue (north)
WAYPOINTS = [(90.0, 195.0, 1, 0), (300.0, 280.0, 0, 1), (300.0, 480.0, 0, 1), (90.0, 560.0, -1, 0), (-120.0, 380.0, 0, -1)]
STOP_SPACING = 320.0
POLE_SPACING = 30.0
WIRE_HEIGHT = 5.6     # above the lane (the lanes are 0.25 m above the road)
WIRE_GAP = 0.6
POLE_HEIGHT = 7.5
BUS_ROOF = 3.12       # laz699.obj
TROLLEY_LENGTH = 6.0
TROLLEY_BASE_X = 0.5  # where the poles start on the roof (the bus faces +x, its middle at 0)


def read_lanes():
    lanes, nxt = {}, {}
    for line in open(M("city", "traffic.txt")):
        p = line.split()
        if not p:
            continue
        if p[0] == "lane":
            i, n = int(p[1]), int(p[4])
            pts = np.array([float(a) for a in p[5:5 + 3 * n]]).reshape(-1, 3)
            d = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(pts, axis=0), axis=1))])
            lanes[i] = dict(junction=int(p[2]), pts=pts, dist=d, length=float(d[-1]))
        elif p[0] == "next":
            nxt[int(p[1])] = [int(a) for a in p[3:]]
    return lanes, nxt


def nearest_lane(lanes, x, z, hx, hz):
    best = None
    for i, L in lanes.items():
        if L["junction"] >= 0 or L["length"] < 30:
            continue
        P = L["pts"]
        for k in range(len(P) - 1):
            a, b = P[k][[0, 2]], P[k + 1][[0, 2]]
            d = b - a
            ln = np.linalg.norm(d)
            if ln < 1e-6 or np.dot(d / ln, (hx, hz)) < 0.9:
                continue
            t = np.clip(np.dot((x, z) - a, d) / ln ** 2, 0, 1)
            dist = np.linalg.norm(a + d * t - (x, z))
            if best is None or dist < best[0]:
                best = (dist, i)
    return best[1]


def shortest(lanes, nxt, a, b):
    """the lanes after a up to b (b included)"""
    dist, prev = {a: 0.0}, {}
    heap = [(0.0, a)]
    while heap:
        d, u = heapq.heappop(heap)
        if u == b and u != a:
            break
        if d > dist.get(u, 1e18):
            continue
        for v in nxt.get(u, []):
            nd = d + lanes[v]["length"]
            if nd < dist.get(v, 1e18):
                dist[v] = nd
                prev[v] = u
                heapq.heappush(heap, (nd, v))
    path = [b]
    while path[-1] != a:
        path.append(prev[path[-1]])
    return path[::-1][1:]


def at(L, s):
    """point and direction (x, z) of lane L at distance s"""
    k = int(np.clip(np.searchsorted(L["dist"], s) - 1, 0, len(L["pts"]) - 2))
    a, b = L["pts"][k], L["pts"][k + 1]
    t = np.clip((s - L["dist"][k]) / max(L["dist"][k + 1] - L["dist"][k], 1e-6), 0, 1)
    d = (b - a)[[0, 2]]
    return a + (b - a) * t, d / max(np.linalg.norm(d), 1e-9)


class ObjWriter:
    def __init__(self):
        self.V, self.F = [], []   # F: (material, [vertex indices], normal)

    def box(self, mat, a, b, w, h):
        """a beam from a to b (3D), w wide, h high (its cross-section)"""
        a, b = np.array(a, float), np.array(b, float)
        d = b - a
        L = np.linalg.norm(d)
        if L < 1e-6:
            return
        d /= L
        up = np.array([0.0, 1.0, 0.0])
        side = np.cross(d, up)
        if np.linalg.norm(side) < 1e-6:
            side = np.array([1.0, 0.0, 0.0])
        side /= np.linalg.norm(side)
        up = np.cross(side, d)
        corners = []
        for p in (a, b):
            for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                corners.append(p + side * sx * w / 2 + up * sy * h / 2)
        base = len(self.V)
        self.V += corners
        quads = [((0, 1, 5, 4), -up), ((1, 2, 6, 5), side), ((2, 3, 7, 6), up), ((3, 0, 4, 7), -side),
                 ((3, 2, 1, 0), -d), ((4, 5, 6, 7), d)]
        for q, n in quads:
            face = [base + i for i in q]
            P = [self.V[i] for i in face]
            if np.dot(np.cross(P[1] - P[0], P[2] - P[0]), n) < 0:   # counter-clockwise seen from outside
                face = face[::-1]
            self.F.append((mat, face, n))

    def write(self, path, mtllib, header):
        mats = sorted({m for m, f, n in self.F})
        with open(path, "w") as f:
            f.write("# %s\nmtllib %s\n" % (header, mtllib))
            for v in self.V:
                f.write("v %.3f %.3f %.3f\n" % tuple(v))
            f.write("vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n")
            for (m, face, n) in self.F:
                f.write("vn %.4f %.4f %.4f\n" % tuple(n))
            normal_of = {k: k + 1 for k in range(len(self.F))}
            for mat in mats:
                f.write("o %s\nusemtl %s\n" % (mat, mat))
                for k, (m, face, n) in enumerate(self.F):
                    if m == mat:
                        f.write("f " + " ".join("%d/%d/%d" % (v + 1, (j % 4) + 1, normal_of[k])
                                                for j, v in enumerate(face)) + "\n")


def main():
    os.makedirs(OUT, exist_ok=True)
    lanes, nxt = read_lanes()
    # the route
    ways = [nearest_lane(lanes, *w) for w in WAYPOINTS]
    route = []
    for k in range(len(ways)):
        route += shortest(lanes, nxt, ways[k], ways[(k + 1) % len(ways)])
    total = sum(lanes[i]["length"] for i in route)
    print("route: %d lanes, %.0f m" % (len(route), total))

    # the ground: roads and pavement
    tV, tF = read_obj(M("terrain", "terrain.obj"))
    road, pave = Raster(), Raster()
    for (o, m, idx) in tF:
        for k in range(1, len(idx) - 1):
            P = tV[[idx[0], idx[k], idx[k + 1]]]
            if m in ROAD:
                road.tri(P, 1)
            elif m in PAVE:
                pave.tri(P, 1)
    road, pave = road.array().astype(bool), pave.array().astype(bool)
    def is_(grid, x, z):
        i, j = cell(x, z)
        return 0 <= i < NZ and 0 <= j < NX and grid[i, j]

    def curb(p, d):
        """the first point off the road on the right of (p, d), its distance"""
        right = np.array([-d[1], d[0]])   # right of the direction of travel: forward x up (as carMatrix)
        for t in np.arange(1.0, 15.0, 0.25):
            q = p[[0, 2]] + right * t
            if not is_(road, q[0], q[1]):
                return q, right, t
        return None, right, None

    # the stops and the poles
    stops, poles = [], []
    run = STOP_SPACING * 0.5
    pole_run = 0.0
    for ri, i in enumerate(route):
        L = lanes[i]
        s = 0.0
        while s < L["length"]:
            step = min(2.0, L["length"] - s)
            s += step
            run += step
            pole_run += step
            if L["junction"] >= 0 or s < 15.0 or s > L["length"] - 15.0:
                continue
            p, d = at(L, s)
            if run >= STOP_SPACING:
                q, right, t = curb(p, d)
                if q is not None and is_(pave, q[0] + right[0] * 1.5, q[1] + right[1] * 1.5):
                    stops.append((ri, s, q[0] + right[0] * 2.4, q[1] + right[1] * 2.4, -right[0], -right[1]))
                    run = 0.0
            if pole_run >= POLE_SPACING:
                q, right, t = curb(p, d)
                if q is not None:
                    q = q + right * 0.6
                    poles.append((q[0], p[1] - 0.25, q[1], p[0], p[2], p[1] + WIRE_HEIGHT))
                    pole_run = 0.0
    print("%d stops, %d poles" % (len(stops), len(poles)))

    # the wires and the poles
    w = ObjWriter()
    for i in route:
        P = lanes[i]["pts"]
        for k in range(len(P) - 1):
            a, b = P[k], P[k + 1]
            d = (b - a)[[0, 2]]
            ln = np.linalg.norm(d)
            if ln < 1e-6:
                continue
            side = np.array([-d[1], 0.0, d[0]]) / ln
            for sgn in (-1, 1):
                off = side * sgn * WIRE_GAP / 2 + np.array([0.0, WIRE_HEIGHT, 0.0])
                w.box("Wire", a + off, b + off, 0.035, 0.035)
    for (x, y, z, lx, lz, wy) in poles:
        w.box("PoleSteel", (x, y, z), (x, y + POLE_HEIGHT, z), 0.22, 0.22)
        # the arm: from the pole out over the lane, a little higher than the wires; a hanger down to them
        w.box("PoleSteel", (x, wy + 0.35, z), (lx, wy + 0.35, lz), 0.06, 0.06)
        w.box("PoleSteel", (x, wy + 1.3, z), ((x + lx) / 2, wy + 0.38, (z + lz) / 2), 0.04, 0.04)
        w.box("PoleSteel", (lx, wy + 0.35, lz), (lx, wy + 0.02, lz), 0.03, 0.03)
    w.write(os.path.join(OUT, "wires.obj"), "wires.mtl", "tools/trolleybus.py: the overhead wires of the trolleybus line")
    with open(os.path.join(OUT, "wires.mtl"), "w") as f:
        f.write("newmtl Wire\nKa 1 1 1\nKd 1 1 1\nKs 0.3 0.3 0.3\nNs 32\nmap_Kd wire.png\n\n")
        f.write("newmtl PoleSteel\nKa 1 1 1\nKd 1 1 1\nKs 0.3 0.3 0.3\nNs 32\nmap_Kd pole_steel.png\n")
    Image.new("RGB", (4, 4), (28, 28, 30)).save(os.path.join(OUT, "wire.png"))
    Image.new("RGB", (4, 4), (120, 125, 120)).save(os.path.join(OUT, "pole_steel.png"))

    # the poles on the roof: from the roof (a little in front of the middle) up and back to the two wires
    t = ObjWriter()
    dh = WIRE_HEIGHT - BUS_ROOF - 0.1
    back = math.sqrt(max(TROLLEY_LENGTH ** 2 - dh ** 2, 1.0))
    for zs in (-WIRE_GAP / 2, WIRE_GAP / 2):
        base = (TROLLEY_BASE_X, BUS_ROOF + 0.1, zs * 0.8)
        tip = (TROLLEY_BASE_X - back, WIRE_HEIGHT, zs)
        t.box("TrolleyPole", base, tip, 0.06, 0.06)
        t.box("TrolleyPole", (TROLLEY_BASE_X - 0.4, BUS_ROOF + 0.02, zs * 0.8),
              (TROLLEY_BASE_X + 0.4, BUS_ROOF + 0.14, zs * 0.8), 0.25, 0.12)   # the base on the roof
    t.write(os.path.join(OUT, "trolley_poles.obj"), "wires.mtl", "tools/trolleybus.py: the poles on the trolleybus roof")
    with open(os.path.join(OUT, "wires.mtl"), "a") as f:
        f.write("\nnewmtl TrolleyPole\nKa 1 1 1\nKd 1 1 1\nKs 0.3 0.3 0.3\nNs 32\nmap_Kd wire.png\n")

    with open(os.path.join(OUT, "route.txt"), "w") as f:
        f.write("# tools/trolleybus.py: route <lanes of the loop>; stop <index in the route> <distance along that lane> "
                "<x z of the shelter> <towards the road x z>; pole <x z>\n")
        f.write("route " + " ".join(str(i) for i in route) + "\n")
        for (ri, s, x, z, fx, fz) in stops:
            f.write("stop %d %.1f %.2f %.2f %.3f %.3f\n" % (ri, s, x, z, fx, fz))
        for (x, y, z, lx, lz, wy) in poles:
            f.write("pole %.2f %.2f\n" % (x, z))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
