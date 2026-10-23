"""Génère les packs audio de RedStrings (bruitages et musiques), puis les encode en OGG.

Usage (depuis la racine du dépôt) :
    python3 csfml/assets/audio/outils/generer_sons.py [standard] [yugioh] [musiques]

Sans argument, tout est régénéré. Les WAV intermédiaires vont dans outils/build/
(ignoré par git) ; les OGG vont dans csfml/assets/audio/<thème>/.
Le résultat est déterministe : chaque son part d'une graine tirée de son nom.
"""

import math
import os
import subprocess
import sys
import zlib

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)

import synthe  # noqa: E402

BUILD = os.path.join(ICI, "build")
AUDIO = os.path.dirname(ICI)
CONVERTISSEUR = os.path.join(BUILD, "wav_vers_ogg")
# Sonie maximale d'un bruitage (RMS en dB) : à crête égale, un son dense paraît
# bien plus fort qu'un clic ; au-delà de ce niveau, il est baissé.
SONIE_MAX = -16.0


def compiler_convertisseur():
    os.makedirs(BUILD, exist_ok=True)
    subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    os.path.join(ICI, "wav_vers_ogg.c"), "-lcsfml-audio", "-lcsfml-system",
                    "-o", CONVERTISSEUR], check=True)


def mesures(sig, sr):
    crete = max((abs(v) for v in sig), default=1e-9)
    rms = math.sqrt(sum(v * v for v in sig) / max(1, len(sig)))
    return len(sig) / sr, 20 * math.log10(max(crete, 1e-9)), 20 * math.log10(max(rms, 1e-9))


def generer_bruitages(theme, sons):
    """Chaque son : synthèse, crête à -1 dB, fin sous -50 dB retirée, fondus anti-clic,
    sonie plafonnée à SONIE_MAX."""
    os.makedirs(os.path.join(BUILD, theme), exist_ok=True)
    synthe.SR = 44100
    paires = []
    for nom, fabrique in sons.items():
        synthe.graine(zlib.crc32((theme + nom).encode()))
        sig = synthe.rogner(synthe.normaliser(fabrique(), 0.89), seuil=0.003)
        sig = synthe.fondus(sig, 0.001, min(0.04, len(sig) / synthe.SR / 4))
        _, _, rms = mesures(sig, synthe.SR)
        if rms > SONIE_MAX:                 # trop dense (ondes carrées) : on le ramène
            sig = synthe.gain(sig, 10 ** ((SONIE_MAX - rms) / 20))
        wav = os.path.join(BUILD, theme, nom + ".wav")
        synthe.ecrire_wav(wav, sig)
        duree, crete, rms = mesures(sig, synthe.SR)
        print("  %-8s %-22s %5.2f s   crête %5.1f dB   RMS %6.1f dB" % (theme, nom, duree, crete, rms))
        paires += [wav, os.path.join(AUDIO, theme, nom + ".ogg")]
    return paires


def generer_musiques(themes):
    import compositions as musiques
    paires = []
    for theme in themes:
        for nom, fabrique in musiques.MUSIQUES[theme].items():
            synthe.SR = musiques.SR_MUSIQUE
            synthe.graine(zlib.crc32((theme + nom).encode()))
            gauche, droite = fabrique()
            wav = os.path.join(BUILD, theme, nom + ".wav")
            os.makedirs(os.path.dirname(wav), exist_ok=True)
            synthe.ecrire_wav(wav, gauche, droite, sr=musiques.SR_MUSIQUE)
            duree, crete, rms = mesures(gauche, musiques.SR_MUSIQUE)
            print("  %-8s %-22s %5.2f s   crête %5.1f dB   RMS %6.1f dB (stéréo, boucle)"
                  % (theme, nom, duree, crete, rms))
            paires += [wav, os.path.join(AUDIO, theme, nom + ".ogg")]
    return paires


def main():
    demandes = set(sys.argv[1:]) or {"standard", "yugioh", "musiques"}
    compiler_convertisseur()
    paires = []
    if "standard" in demandes:
        import sons_standard
        paires += generer_bruitages("standard", sons_standard.SONS)
    if "yugioh" in demandes:
        import sons_yugioh
        paires += generer_bruitages("yugioh", sons_yugioh.SONS)
    if "musiques" in demandes:
        paires += generer_musiques(("standard", "yugioh"))
    subprocess.run([CONVERTISSEUR] + paires, check=True)
    print("%d fichier(s) OGG écrits dans %s" % (len(paires) // 2, AUDIO))


if __name__ == "__main__":
    main()
