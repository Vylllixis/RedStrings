#!/usr/bin/env python3
"""Télécharge un modèle Poly Haven (CC0, https://polyhaven.com) en glTF.

    arbres_fetch.py IDENTIFIANT RÉSOLUTION DOSSIER      (ex. : jacaranda_tree 1k build/arbres-sources)

Écrit DOSSIER/IDENTIFIANT/<identifiant>_<résolution>.gltf, son .bin et ses textures. Chaque fichier
est vérifié par l'empreinte md5 que publie l'API de Poly Haven ; un fichier déjà présent et de la
bonne taille n'est pas retéléchargé (mais il est revérifié). Aucune information de l'utilisateur
n'est envoyée : l'agent utilisateur est un nom générique."""
import hashlib
import json
import os
import sys
import urllib.request

AGENT = {"User-Agent": "asset-fetch-script/1.0"}
BLOCK = 1 << 20


def get(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=120)


def md5_of(path):
    digest = hashlib.md5()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(BLOCK), b""):
            digest.update(block)
    return digest.hexdigest()


def fetch(asset, resolution, out):
    files = json.load(get(f"https://api.polyhaven.com/files/{asset}"))
    entry = files["gltf"][resolution]["gltf"]
    todo = [(os.path.basename(entry["url"]), entry)]
    todo += list(entry.get("include", {}).items())
    total = 0
    for relative, item in todo:
        path = os.path.normpath(os.path.join(out, asset, relative))
        if not path.startswith(os.path.normpath(os.path.join(out, asset)) + os.sep):
            raise SystemExit(f"{relative} : chemin hors du dossier de sortie, refusé")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if not (os.path.exists(path) and os.path.getsize(path) == item["size"]):
            with get(item["url"]) as response, open(path + ".part", "wb") as stream:
                for block in iter(lambda: response.read(BLOCK), b""):
                    stream.write(block)
            os.replace(path + ".part", path)
        if md5_of(path) != item["md5"]:
            raise SystemExit(f"{path} : empreinte md5 inattendue, fichier à supprimer et à reprendre")
        total += item["size"]
    print(f"{asset} ({resolution}) : {len(todo)} fichier(s), {total / 1e6:.1f} Mo, empreintes vérifiées")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    fetch(sys.argv[1], sys.argv[2], sys.argv[3])
