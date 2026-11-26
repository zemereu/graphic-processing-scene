"""The lit windows in groups that come on and go off at different hours of the night.

usage: python tools/window_groups.py        (from the project folder; needs numpy)

The lit windows are copies of the window faces with a lit texture (models/*/..._lights.obj). Each window (the faces
that touch) gets one of GROUPS groups at random; the group is written in its texture coordinate u: u - floor(u) is
kept (the textures repeat, the picture doesn't change) and GROUP_STRIDE * group is added. basic.frag reads the group
back (floor(u / GROUP_STRIDE)) and gives each group its own hours (lights on in the evening, off later in the night,
some on again early in the morning).
Running it again gives the same groups.
"""
import os
from collections import defaultdict

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FILES = ["models/city/city_lights.obj", "models/genex/genex_lights.obj", "models/romanita/romanita_lights.obj",
         "models/telefoane/telefoane_lights.obj", "models/satu_mare/satu_mare_lights.obj"]
GROUPS = 16
GROUP_STRIDE = 128.0   # basic.frag: the same


def regroup(path):
    lines = open(path, encoding="utf-8", errors="ignore").read().split("\n")
    V = [[float(a) for a in l.split()[1:4]] for l in lines if l.startswith("v ")]
    VT = [[float(a) for a in l.split()[1:3]] for l in lines if l.startswith("vt ")]
    V = np.array(V)
    faces = []   # (line index, [(v, vt, vn) ...])
    for k, l in enumerate(lines):
        if l.startswith("f "):
            corners = []
            for t in l.split()[1:]:
                a = (t.split("/") + ["", ""])[:3]
                corners.append((int(a[0]) - 1, int(a[1]) - 1 if a[1] else -1, a[2]))
            faces.append((k, corners))
    # one window = the faces that share a corner (by position)
    parent = {}
    def find(a):
        while parent.setdefault(a, a) != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    key = lambda v: tuple(np.round(V[v], 3))
    for (k, corners) in faces:
        for c in corners[1:]:
            parent[find(key(c[0]))] = find(key(corners[0][0]))
    windows = defaultdict(list)
    for fi, (k, corners) in enumerate(faces):
        windows[find(key(corners[0][0]))].append(fi)
    new_vt = []
    out_faces = {}
    for fis in windows.values():
        vs = sorted({c[0] for fi in fis for c in faces[fi][1]})
        center = V[vs].mean(axis=0)
        h = (np.sin(center[0] * 12.9898 + center[1] * 4.1414 + center[2] * 78.233) * 43758.5453) % 1.0
        group = int(h * GROUPS) % GROUPS
        us = [VT[c[1]][0] for fi in fis for c in faces[fi][1] if c[1] >= 0]
        shift = np.floor(min(us)) if us else 0.0
        if max(us) - shift >= GROUP_STRIDE / 2:
            print("  a window with u over", GROUP_STRIDE / 2, "in", path)
        remap = {}
        for fi in fis:
            k, corners = faces[fi]
            parts = []
            for (v, t, n) in corners:
                if t >= 0:
                    if t not in remap:
                        u, w = VT[t]
                        new_vt.append((u - shift + GROUP_STRIDE * group, w))
                        remap[t] = len(new_vt)
                    parts.append("%d/%d/%s" % (v + 1, remap[t], n) if n else "%d/%d" % (v + 1, remap[t]))
                else:
                    parts.append("%d//%s" % (v + 1, n) if n else "%d" % (v + 1))
            out_faces[k] = "f " + " ".join(parts)
    # every v / vt / vn first (the faces refer to the new vt), then the objects and their faces
    head = [l for l in lines if l.startswith(("mtllib", "#"))]
    vs = [l for l in lines if l.startswith("v ")]
    vns = [l for l in lines if l.startswith("vn ")]
    body = []
    for k, l in enumerate(lines):
        if not l or l.startswith(("v ", "vt ", "vn ", "mtllib", "#")):
            continue
        body.append(out_faces.get(k, l))
    out = head + vs + ["vt %.5f %.5f" % t for t in new_vt] + vns + body
    open(path, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("%s: %d windows in %d groups" % (os.path.relpath(path, ROOT), len(windows), GROUPS))


if __name__ == "__main__":
    for f in FILES:
        regroup(os.path.join(ROOT, f))
