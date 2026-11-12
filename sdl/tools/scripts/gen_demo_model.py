#!/usr/bin/env python3
"""Génère le modèle de démonstration assets/models/demo.gltf + demo.bin (M1-10).

Matériaux PBR (M1-15) : diélectriques (métal 0), le tore assez mat, la sphère brillante ; sans
facteurs, glTF donnerait un métal très rugueux, sombre.
Un tore texturé (damier assets/textures/checker.png, URI externe) et une sphère orange (couleur
unie), reliés par une hiérarchie de nœuds : le nœud racine est incliné, la sphère est son enfant.
Usage : python3 tools/scripts/gen_demo_model.py   (depuis la racine du projet)
"""
import json
import math
import struct
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "assets" / "models"


def torus(major, minor, nu, nv, uv_repeat):
    pos, nor, uv, idx = [], [], [], []
    for i in range(nu + 1):
        a = 2 * math.pi * i / nu
        for j in range(nv + 1):
            b = 2 * math.pi * j / nv
            cx, cz = math.cos(a), math.sin(a)
            r = major + minor * math.cos(b)
            pos.append((r * cx, minor * math.sin(b), r * cz))
            nor.append((math.cos(b) * cx, math.sin(b), math.cos(b) * cz))
            uv.append((uv_repeat * i / nu, uv_repeat * 0.5 * j / nv))
    for i in range(nu):
        for j in range(nv):
            a = i * (nv + 1) + j
            b = a + nv + 1
            # sens anti-horaire vu de l'extérieur (vérifié par le test de rendu)
            idx += [a, a + 1, b, b, a + 1, b + 1]
    return pos, nor, uv, idx


def sphere(radius, nu, nv):
    pos, nor, uv, idx = [], [], [], []
    for j in range(nv + 1):
        phi = math.pi * j / nv
        for i in range(nu + 1):
            th = 2 * math.pi * i / nu
            n = (math.sin(phi) * math.cos(th), math.cos(phi), math.sin(phi) * math.sin(th))
            pos.append(tuple(radius * c for c in n))
            nor.append(n)
            uv.append((i / nu, j / nv))
    for j in range(nv):
        for i in range(nu):
            a = j * (nu + 1) + i
            b = a + nu + 1
            idx += [a, a + 1, b, b, a + 1, b + 1]
    return pos, nor, uv, idx


blob = bytearray()
views, accessors = [], []


def add(data, fmt, comp, count, typ, target, minmax=None):
    while len(blob) % 4:
        blob.append(0)
    offset = len(blob)
    blob.extend(data)
    views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target})
    acc = {"bufferView": len(views) - 1, "componentType": comp, "count": count, "type": typ}
    if minmax:
        acc["min"], acc["max"] = minmax
    accessors.append(acc)
    return len(accessors) - 1


def mesh_primitive(pos, nor, uv, idx, material):
    flat = [c for p in pos for c in p]
    mn = [min(p[k] for p in pos) for k in range(3)]
    mx = [max(p[k] for p in pos) for k in range(3)]
    a_pos = add(struct.pack(f"<{len(flat)}f", *flat), "f", 5126, len(pos), "VEC3", 34962, ([*mn], [*mx]))
    flat = [c for n in nor for c in n]
    a_nor = add(struct.pack(f"<{len(flat)}f", *flat), "f", 5126, len(nor), "VEC3", 34962)
    flat = [c for t in uv for c in t]
    a_uv = add(struct.pack(f"<{len(flat)}f", *flat), "f", 5126, len(uv), "VEC2", 34962)
    a_idx = add(struct.pack(f"<{len(idx)}H", *idx), "H", 5123, len(idx), "SCALAR", 34963)
    return {"attributes": {"POSITION": a_pos, "NORMAL": a_nor, "TEXCOORD_0": a_uv},
            "indices": a_idx, "material": material, "mode": 4}


t = torus(1.6, 0.55, 48, 24, 8)
s = sphere(0.9, 32, 16)
prim_t = mesh_primitive(*t, 0)
prim_s = mesh_primitive(*s, 1)

tilt = 0.35  # rotation du nœud racine autour de Z
q = [0.0, 0.0, math.sin(tilt / 2), math.cos(tilt / 2)]
gltf = {
    "asset": {"version": "2.0", "generator": "tools/scripts/gen_demo_model.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [
        {"name": "anneau", "mesh": 0, "rotation": q, "children": [1]},
        {"name": "bille", "mesh": 1, "translation": [0.0, 0.0, 0.0]},
    ],
    "meshes": [{"name": "tore", "primitives": [prim_t]}, {"name": "sphere", "primitives": [prim_s]}],
    "materials": [
        {"name": "damier", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                      "baseColorFactor": [1, 1, 1, 1],
                                                      "metallicFactor": 0.0, "roughnessFactor": 0.5}},
        {"name": "orange", "pbrMetallicRoughness": {"baseColorFactor": [0.95, 0.45, 0.1, 1.0],
                                                    "metallicFactor": 0.0, "roughnessFactor": 0.3}},
    ],
    "textures": [{"source": 0}],
    "images": [{"uri": "../textures/checker.png"}],
    "buffers": [{"uri": "demo.bin", "byteLength": len(blob)}],
    "bufferViews": views,
    "accessors": accessors,
}
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "demo.bin").write_bytes(bytes(blob))
(OUT / "demo.gltf").write_text(json.dumps(gltf, indent=1) + "\n")
print("écrit", OUT / "demo.gltf", f"({len(blob)} octets de tampon)")
