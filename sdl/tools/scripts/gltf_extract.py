#!/usr/bin/env python3
"""Extrait d'un .glb un sous-ensemble autonome et compact (M4 : personnages KayKit, CC0).

Le moteur cuit ensuite le résultat avec assetc (règles CMake de tools/CMakeLists.txt). Cet outil
ne fait QUE trier des données glTF : il garde des nœuds, des animations (renommées), et réécrit un
tampon binaire qui ne contient que ce qui est référencé. Aucune géométrie n'est recalculée.

    gltf_extract.py ENTRÉE.glb SORTIE.glb [--drop-node NOM]... [--anim SOURCE=NOM]... [--no-anims]
    gltf_extract.py ENTRÉE.glb SORTIE.glb --root-node NOM

  --drop-node NOM   retire le nœud NOM et ses descendants (armes, boucliers...) ;
  --anim SRC=NOM    garde l'animation SRC sous le nom NOM (dans l'ordre de la ligne de commande) ;
                    sans --anim : aucune animation n'est gardée ;
  --root-node NOM   ne garde que le nœud NOM, à la racine de la scène, avec SA transformation locale
                    (celle relative à son parent dans le fichier source) : pour une arme rangée sous
                    un os « handslot », les sommets cuits sont alors exprimés dans le repère de cet os.

Bibliothèque standard seulement ; lecture seule de l'entrée (fichier de données, jamais exécuté).
"""
import argparse
import json
import struct
import sys

GLB_MAGIC = 0x46546C67
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942


def read_glb(path):
    data = open(path, "rb").read()
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC or version != 2 or length > len(data):
        sys.exit(f"{path} : pas un GLB 2.0")
    off = 12
    doc = None
    binary = b""
    while off < length:
        clen, ctype = struct.unpack_from("<II", data, off)
        chunk = data[off + 8:off + 8 + clen]
        if ctype == CHUNK_JSON:
            doc = json.loads(chunk.decode("utf-8"))
        elif ctype == CHUNK_BIN:
            binary = chunk
        off += 8 + clen
    if doc is None:
        sys.exit(f"{path} : pas de JSON")
    return doc, binary


def write_glb(path, doc, binary):
    js = json.dumps(doc, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    binary += b"\0" * ((4 - len(binary) % 4) % 4)
    total = 12 + 8 + len(js) + (8 + len(binary) if binary else 0)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", GLB_MAGIC, 2, total))
        f.write(struct.pack("<II", len(js), CHUNK_JSON))
        f.write(js)
        if binary:
            f.write(struct.pack("<II", len(binary), CHUNK_BIN))
            f.write(binary)


def parents_of(doc):
    parent = {}
    for i, n in enumerate(doc.get("nodes", [])):
        for c in n.get("children", []):
            parent[c] = i
    return parent


def node_index(doc, name):
    for i, n in enumerate(doc.get("nodes", [])):
        if n.get("name") == name:
            return i
    sys.exit(f"nœud introuvable : {name}")


def descendants(doc, root):
    out = {root}
    stack = [root]
    while stack:
        for c in doc["nodes"][stack.pop()].get("children", []):
            if c not in out:
                out.add(c)
                stack.append(c)
    return out


class Rebuilder:
    """Recopie dans un nouveau document les objets référencés, en renumérotant."""

    def __init__(self, src, binary):
        self.src = src
        self.bin = binary
        self.out = {"asset": {"version": "2.0", "generator": "gltf_extract.py (RedStrings)"}}
        self.maps = {}
        self.blob = bytearray()

    def _map(self, kind, index, build):
        m = self.maps.setdefault(kind, {})
        if index not in m:
            lst = self.out.setdefault(kind, [])
            m[index] = len(lst)
            lst.append(None)
            lst[m[index]] = build(self.src[kind][index])
        return m[index]

    def buffer_view(self, i):
        def build(bv):
            start = bv.get("byteOffset", 0)
            chunk = self.bin[start:start + bv["byteLength"]]
            while len(self.blob) % 4:
                self.blob.append(0)
            nbv = {k: v for k, v in bv.items() if k not in ("buffer", "byteOffset")}
            nbv["buffer"] = 0
            nbv["byteOffset"] = len(self.blob)
            self.blob += chunk
            return nbv
        return self._map("bufferViews", i, build)

    def accessor(self, i):
        def build(a):
            na = dict(a)
            if "bufferView" in a:
                na["bufferView"] = self.buffer_view(a["bufferView"])
            if "sparse" in a:
                sys.exit("accesseur creux non géré")
            return na
        return self._map("accessors", i, build)

    def image(self, i):
        def build(im):
            ni = dict(im)
            if "bufferView" in im:
                ni["bufferView"] = self.buffer_view(im["bufferView"])
            return ni
        return self._map("images", i, build)

    def sampler(self, i):
        return self._map("samplers", i, dict)

    def texture(self, i):
        def build(t):
            nt = dict(t)
            if "source" in t:
                nt["source"] = self.image(t["source"])
            if "sampler" in t:
                nt["sampler"] = self.sampler(t["sampler"])
            return nt
        return self._map("textures", i, build)

    def material(self, i):
        def build(m):
            nm = json.loads(json.dumps(m))

            def fix(obj):
                if isinstance(obj, dict):
                    for k, v in list(obj.items()):
                        if k == "index" and isinstance(v, int):  # textureInfo.index
                            obj[k] = self.texture(v)
                        else:
                            fix(v)
            fix(nm)
            return nm
        return self._map("materials", i, build)

    def mesh(self, i):
        def build(m):
            nm = {k: v for k, v in m.items() if k != "primitives"}
            nm["primitives"] = []
            for p in m["primitives"]:
                np_ = {k: v for k, v in p.items() if k not in ("attributes", "indices", "material", "targets")}
                np_["attributes"] = {k: self.accessor(v) for k, v in p["attributes"].items()}
                if "indices" in p:
                    np_["indices"] = self.accessor(p["indices"])
                if "material" in p:
                    np_["material"] = self.material(p["material"])
                if "targets" in p:
                    np_["targets"] = [{k: self.accessor(v) for k, v in t.items()} for t in p["targets"]]
                nm["primitives"].append(np_)
            return nm
        return self._map("meshes", i, build)


def extract(src, binary, keep_nodes, roots, anims, root_override=None):
    """keep_nodes : indices source gardés ; roots : racines de la scène (indices source)."""
    rb = Rebuilder(src, binary)
    order = sorted(keep_nodes)
    node_map = {old: new for new, old in enumerate(order)}
    nodes = []
    for old in order:
        n = src["nodes"][old]
        nn = {k: v for k, v in n.items() if k not in ("children", "mesh", "skin", "camera")}
        kids = [node_map[c] for c in n.get("children", []) if c in node_map]
        if kids:
            nn["children"] = kids
        if "mesh" in n:
            nn["mesh"] = rb.mesh(n["mesh"])
        nodes.append(nn)
    rb.out["nodes"] = nodes
    skins = []
    for old in order:
        n = src["nodes"][old]
        if "skin" in n:
            s = src["skins"][n["skin"]]
            joints = s["joints"]
            if not all(j in node_map for j in joints):
                sys.exit("un nœud skinné perd ses articulations : retire-le aussi")
            ns = {"joints": [node_map[j] for j in joints]}
            if "inverseBindMatrices" in s:
                ns["inverseBindMatrices"] = rb.accessor(s["inverseBindMatrices"])
            if "skeleton" in s and s["skeleton"] in node_map:
                ns["skeleton"] = node_map[s["skeleton"]]
            if "name" in s:
                ns["name"] = s["name"]
            key = json.dumps(ns, sort_keys=True)
            idx = next((i for i, (k, _) in enumerate(skins) if k == key), None)
            if idx is None:
                skins.append((key, ns))
                idx = len(skins) - 1
            nodes[node_map[old]]["skin"] = idx
    if skins:
        rb.out["skins"] = [s for _, s in skins]
    if root_override is not None:
        nodes[node_map[root_override]].pop("children", None)
    rb.out["scenes"] = [{"nodes": [node_map[r] for r in roots]}]
    rb.out["scene"] = 0
    out_anims = []
    by_name = {a.get("name"): a for a in src.get("animations", [])}
    for source_name, new_name in anims:
        a = by_name.get(source_name)
        if a is None:
            sys.exit(f"animation introuvable : {source_name}")
        samplers = []
        smap = {}
        channels = []
        for ch in a["channels"]:
            t = ch["target"]
            if t.get("node") not in node_map:
                continue
            si = ch["sampler"]
            if si not in smap:
                s = a["samplers"][si]
                smap[si] = len(samplers)
                samplers.append({"input": rb.accessor(s["input"]), "output": rb.accessor(s["output"]),
                                 "interpolation": s.get("interpolation", "LINEAR")})
            channels.append({"sampler": smap[si], "target": {"node": node_map[t["node"]], "path": t["path"]}})
        out_anims.append({"name": new_name, "channels": channels, "samplers": samplers})
    if out_anims:
        rb.out["animations"] = out_anims
    if rb.blob:
        rb.out["buffers"] = [{"byteLength": len(rb.blob)}]
    return rb.out, bytes(rb.blob)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--drop-node", action="append", default=[])
    ap.add_argument("--anim", action="append", default=[], help="SOURCE=NOM")
    ap.add_argument("--root-node")
    a = ap.parse_args()
    src, binary = read_glb(a.input)
    if src.get("extensionsRequired"):
        sys.exit(f"extensions requises non gérées : {src['extensionsRequired']}")
    anims = []
    for spec in a.anim:
        if "=" not in spec:
            sys.exit(f"--anim attend SOURCE=NOM : {spec}")
        s, n = spec.split("=", 1)
        anims.append((s, n))
    if a.root_node:
        r = node_index(src, a.root_node)
        keep = {r}
        roots = [r]
        doc, blob = extract(src, binary, keep, roots, [], root_override=r)
    else:
        drop = set()
        for name in a.drop_node:
            drop |= descendants(src, node_index(src, name))
        keep = set(range(len(src["nodes"]))) - drop
        roots = [r for r in src["scenes"][src.get("scene", 0)]["nodes"] if r in keep]
        doc, blob = extract(src, binary, keep, roots, anims)
    write_glb(a.output, doc, blob)
    print(f"{a.output} : {len(doc['nodes'])} nœuds, {len(doc.get('meshes', []))} maillages, "
          f"{len(doc.get('animations', []))} animations, {len(blob)} octets de données")


if __name__ == "__main__":
    main()
