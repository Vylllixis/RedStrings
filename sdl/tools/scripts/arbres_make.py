#!/usr/bin/env python3
"""Arbres de la carte du monde (2026-11-21) : chêne, acacia (modèles originaux, CC0 du projet) et
sapin enneigé (géométrie du sapin KayKit tree_single_B, CC0, recoloré : neige sur le dessus des
branches). Style low-poly à faces plates, couleurs d'une petite palette commune.

    python3 tools/scripts/arbres_make.py

Écrit dans assets/props/nature/ : oak, acacia, pine_snow (.gltf + .bin) et trees_palette.png.
Déterministe : mêmes fichiers à chaque exécution (graine fixe). Dépend de Pillow."""
import base64, json, math, random, struct, os
from PIL import Image

OUT = 'assets/props/nature'
PALETTE = [  # sRGB
    (92, 160, 58), (70, 135, 45), (50, 105, 38), (110, 75, 50),      # chêne : feuilles x3, écorce
    (138, 146, 56), (108, 120, 44), (125, 95, 65), (238, 244, 250),  # acacia x2, écorce ; neige
    (205, 218, 232), (30, 100, 70), (22, 80, 58), (95, 62, 45),      # neige ombrée ; sapin x2, écorce
    (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0)]
GRID, CELL = 4, 16  # palette 64 x 64 : 16 cases de 16 pixels


def palette_png():
    im = Image.new('RGB', (GRID * CELL, GRID * CELL))
    for i, c in enumerate(PALETTE):
        x, y = (i % GRID) * CELL, (i // GRID) * CELL
        for yy in range(y, y + CELL):
            for xx in range(x, x + CELL):
                im.putpixel((xx, yy), c)
    im.save(f'{OUT}/trees_palette.png', optimize=True)


def uv(color):
    return ((color % GRID + 0.5) / GRID, (color // GRID + 0.5) / GRID)


class Mesh:
    def __init__(self):
        self.pos, self.nrm, self.uv = [], [], []

    def tri(self, a, b, c, color):
        ux, uy = (b[0]-a[0], b[1]-a[1], b[2]-a[2]), (c[0]-a[0], c[1]-a[1], c[2]-a[2])
        n = (ux[1]*uy[2]-ux[2]*uy[1], ux[2]*uy[0]-ux[0]*uy[2], ux[0]*uy[1]-ux[1]*uy[0])
        l = math.sqrt(sum(v*v for v in n)) or 1.0
        n = tuple(v / l for v in n)
        for p in (a, b, c):
            self.pos.append(p); self.nrm.append(n); self.uv.append(uv(color))
        return n


def icosphere(subdiv=1):
    t = (1 + 5 ** 0.5) / 2
    v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
         (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    v = [tuple(c / math.sqrt(sum(x*x for x in p)) for c in p) for p in v]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
         (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
         (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for _ in range(subdiv):
        cache, nf = {}, []
        def mid(i, j):
            k = (min(i, j), max(i, j))
            if k not in cache:
                p = tuple((v[i][c] + v[j][c]) / 2 for c in range(3))
                l = math.sqrt(sum(x*x for x in p)); v.append(tuple(x / l for x in p)); cache[k] = len(v) - 1
            return cache[k]
        for a, b, c in f:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        f = nf
    return v, f


def blob(m, rng, center, radius, squash, colors, wobble=0.12):
    v, f = icosphere(1)
    jit = [1.0 + wobble * (rng.random() - 0.5) * 2 for _ in v]  # bosselé, toujours la même forme
    pts = [(center[0] + p[0]*radius*j, center[1] + p[1]*radius*squash*j, center[2] + p[2]*radius*j)
           for p, j in zip(v, jit)]
    for a, b, c in f:
        top = (v[a][1] + v[b][1] + v[c][1]) / 3
        pick = colors[0] if top > 0.35 else (colors[1] if top > -0.2 else colors[2])
        if rng.random() < 0.18:
            pick = colors[rng.randrange(len(colors))]
        m.tri(pts[a], pts[b], pts[c], pick)


def prism(m, a, b, r0, r1, color, sides=6):
    for i in range(sides):
        t0, t1 = 2*math.pi*i/sides, 2*math.pi*(i+1)/sides
        p = lambda base, r, t: (base[0] + r*math.cos(t), base[1], base[2] + r*math.sin(t))
        a0, a1, b0, b1 = p(a, r0, t0), p(a, r0, t1), p(b, r1, t0), p(b, r1, t1)
        m.tri(a0, b0, a1, color); m.tri(a1, b0, b1, color)


def oak():
    m, rng = Mesh(), random.Random(11)
    prism(m, (0, -0.1, 0), (0, 0.5, 0), 0.075, 0.05, 3)
    prism(m, (0.0, 0.38, 0), (0.12, 0.58, 0.04), 0.035, 0.025, 3)
    prism(m, (0.0, 0.42, 0), (-0.1, 0.6, -0.05), 0.035, 0.025, 3)
    leaves = (0, 1, 2)
    for c, r in (((0, 0.72, 0), 0.33), ((0.2, 0.64, 0.07), 0.23), ((-0.18, 0.66, -0.08), 0.24),
                 ((0.03, 0.9, -0.04), 0.22), ((-0.05, 0.65, 0.2), 0.2)):
        blob(m, rng, c, r, 0.85, leaves)
    return m


def acacia():
    m, rng = Mesh(), random.Random(23)
    prism(m, (0, -0.1, 0), (0.03, 0.38, 0), 0.05, 0.035, 6)
    prism(m, (0.03, 0.36, 0), (0.2, 0.62, 0.03), 0.03, 0.02, 6)
    prism(m, (0.03, 0.36, 0), (-0.16, 0.6, -0.04), 0.03, 0.02, 6)
    prism(m, (0.03, 0.4, 0), (0.0, 0.63, 0.15), 0.025, 0.018, 6)
    for c, r in (((0.18, 0.68, 0.02), 0.3), ((-0.15, 0.66, -0.03), 0.27), ((0.0, 0.7, 0.15), 0.24)):
        blob(m, rng, c, r, 0.28, (4, 4, 5), wobble=0.08)
    return m


def gltf_accessor(g, src):  # (positions, uvs, indices) d'un glTF KayKit
    b = open(os.path.join(OUT, g['buffers'][0]['uri']), 'rb').read()
    def acc(i):
        a = g['accessors'][i]; bv = g['bufferViews'][a['bufferView']]
        off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        n = {'VEC3': 3, 'VEC2': 2, 'SCALAR': 1}[a['type']]
        fmt = {5126: 'f', 5123: 'H', 5125: 'I'}[a['componentType']]
        stride = bv.get('byteStride', struct.calcsize(fmt) * n)
        return [struct.unpack_from('<' + fmt * n, b, off + k * stride) for k in range(a['count'])]
    prim = g['meshes'][0]['primitives'][0]
    return acc(prim['attributes']['POSITION']), acc(prim['attributes']['TEXCOORD_0']), \
        [i[0] for i in acc(prim['indices'])]


def split(tri, depth):
    """Découpe un triangle en 4^depth triangles (milieux des côtés), même sens de parcours."""
    if depth == 0:
        return [tri]
    a, b, c = tri
    m = lambda p, q: tuple((p[i] + q[i]) / 2 for i in range(3))
    ab, bc, ca = m(a, b), m(b, c), m(c, a)
    out = []
    for t in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)):
        out += split(t, depth - 1)
    return out


def pine_snow():
    g = json.load(open(f'{OUT}/tree_single_B.gltf'))
    tex = Image.open(f'{OUT}/hexagons_medieval.png').convert('RGB')
    P, UV, I = gltf_accessor(g, 'tree_single_B')
    m, rng = Mesh(), random.Random(37)
    for k in range(0, len(I), 3):
        tri = (P[I[k]], P[I[k + 1]], P[I[k + 2]])
        u, v = UV[I[k]]
        r, gg, bb = tex.getpixel((int(u * tex.width) % tex.width, int(v * tex.height) % tex.height))
        if not gg > r + 15:  # écorce
            m.tri(*tri, 11)
            continue
        y0 = min(p[1] for p in tri)
        y1 = max(p[1] for p in tri)
        for sub in split(tri, 1):
            n = m.tri(*sub, 9)
            t = ((sum(p[1] for p in sub) / 3) - y0) / ((y1 - y0) or 1.0)
            # neige sur le haut de chaque étage (faces tournées vers le ciel), plaques plus bas
            snow = n[1] > 0.2 and (t > 0.45 or (t > 0.15 and rng.random() < 0.25))
            if n[1] <= 0.2:
                color = 10
            elif snow:
                color = 7 if n[1] > 0.45 else 8
            else:
                color = 9 if rng.random() < 0.7 else 10
            for q in range(3):
                m.uv[-1 - q] = uv(color)
    return m


def write(name, m):
    n = len(m.pos)
    data = b''.join(struct.pack('<3f', *p) for p in m.pos)
    data += b''.join(struct.pack('<3f', *p) for p in m.nrm)
    data += b''.join(struct.pack('<2f', *p) for p in m.uv)
    idx = b''.join(struct.pack('<I', i) for i in range(n))
    off_n, off_uv, off_i = 12 * n, 24 * n, 32 * n
    blob_ = data + idx
    mins = [min(p[c] for p in m.pos) for c in range(3)]
    maxs = [max(p[c] for p in m.pos) for c in range(3)]
    g = {
        'asset': {'version': '2.0', 'generator': 'arbres_make.py (RedStrings)'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': name}],
        'meshes': [{'name': name, 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1,
                                                                  'TEXCOORD_0': 2},
                                                   'indices': 3, 'material': 0}]}],
        'materials': [{'name': 'trees_palette', 'pbrMetallicRoughness': {
            'baseColorTexture': {'index': 0}, 'metallicFactor': 0.0, 'roughnessFactor': 0.85}}],
        'textures': [{'source': 0, 'sampler': 0}],
        'samplers': [{'magFilter': 9728, 'minFilter': 9728}],
        'images': [{'uri': 'trees_palette.png'}],
        'buffers': [{'uri': f'{name}.bin', 'byteLength': len(blob_)}],
        'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': off_uv, 'byteStride': 12, 'target': 34962},
                        {'buffer': 0, 'byteOffset': off_uv, 'byteLength': off_i - off_uv, 'byteStride': 8, 'target': 34962},
                        {'buffer': 0, 'byteOffset': off_i, 'byteLength': 4 * n, 'target': 34963}],
        'accessors': [{'bufferView': 0, 'byteOffset': 0, 'componentType': 5126, 'count': n, 'type': 'VEC3', 'min': mins, 'max': maxs},
                      {'bufferView': 0, 'byteOffset': off_n, 'componentType': 5126, 'count': n, 'type': 'VEC3'},
                      {'bufferView': 1, 'componentType': 5126, 'count': n, 'type': 'VEC2'},
                      {'bufferView': 2, 'componentType': 5125, 'count': n, 'type': 'SCALAR'}],
    }
    open(f'{OUT}/{name}.bin', 'wb').write(blob_)
    open(f'{OUT}/{name}.gltf', 'w').write(json.dumps(g, indent=1) + '\n')
    print(f'{name} : {n // 3} triangles, hauteur {maxs[1] - mins[1]:.2f}')


def cone(m, y0, y1, r, color_top, color_band, sides=7, band=0.35):
    """Étage de sapin simplifié : un cône (enneigé) au-dessus d'une bande (verte)."""
    yb = y0 + (y1 - y0) * band
    rb = r * (1.0 - band)
    for i in range(sides):
        t0, t1 = 2 * math.pi * i / sides, 2 * math.pi * (i + 1) / sides
        p = lambda rr, y, t: (rr * math.cos(t), y, rr * math.sin(t))
        m.tri(p(rb, yb, t0), (0, y1, 0), p(rb, yb, t1), color_top)
        m.tri(p(r, y0, t0), p(rb, yb, t0), p(r, y0, t1), color_band)
        m.tri(p(r, y0, t1), p(rb, yb, t0), p(rb, yb, t1), color_band)
        m.tri(p(r, y0, t1), (0, y0, 0), p(r, y0, t0), color_band)


def lod_oak():
    m, rng = Mesh(), random.Random(5)
    prism(m, (0, -0.1, 0), (0, 0.5, 0), 0.075, 0.05, 3, sides=4)
    v, f = icosphere(0)
    for a, b, c in f:
        pts = [(p[0] * 0.42, 0.74 + p[1] * 0.36, p[2] * 0.42) for p in (v[a], v[b], v[c])]
        m.tri(*pts, 0 if (v[a][1] + v[b][1] + v[c][1]) > 0 else 1)
    return m


def lod_pine():
    m = Mesh()
    prism(m, (0, -0.1, 0), (0, 0.3, 0), 0.07, 0.06, 11, sides=4)
    cone(m, 0.2, 0.75, 0.36, 7, 9)
    cone(m, 0.55, 1.11, 0.28, 7, 9)
    return m


def lod_acacia():
    m = Mesh()
    prism(m, (0, -0.1, 0), (0.03, 0.62, 0), 0.05, 0.03, 6, sides=4)
    v, f = icosphere(0)
    for a, b, c in f:
        pts = [(0.02 + p[0] * 0.42, 0.68 + p[1] * 0.1, p[2] * 0.42) for p in (v[a], v[b], v[c])]
        m.tri(*pts, 4 if (v[a][1] + v[b][1] + v[c][1]) > 0 else 5)
    return m


if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
    palette_png()
    write('oak', oak())
    write('acacia', acacia())
    write('pine_snow', pine_snow())
    write('oak_lod', lod_oak())      # versions simplifiées, au loin
    write('acacia_lod', lod_acacia())
    write('pine_snow_lod', lod_pine())
