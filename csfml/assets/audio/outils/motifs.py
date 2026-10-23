"""Motifs sonores réutilisés par les deux thèmes (souffles, clics, cloches, coups…).

Chaque fonction renvoie un signal mono (liste de flottants), pas encore normalisé.
"""

import math

from synthe import (SR, adsr, bruit, cloche, corde, decroit, gain, mix, n, note, osc,
                    passe_bas, passe_haut, reverb, sature, svf)


def souffle(f0, f1, duree, q=1.2, forme=0.35):
    """Bruit filtré dont la bande glisse de f0 à f1 : passage d'air, carte qui glisse."""
    sig = svf(bruit(duree), lambda p: f0 * (f1 / f0) ** p, q=q, mode="bande")
    total = len(sig)
    return [v * math.sin(math.pi * min(1.0, (i / total) / forme) * 0.5)
            * (1.0 - i / total) ** 1.5 for i, v in enumerate(sig)]


def bip(freq, duree, forme="sinus", tau=None, rapport=0.5):
    sig = osc(forme, freq, duree, rapport=rapport)
    return decroit(sig, tau if tau else duree / 3.0)


def clic(centre, duree=0.02, q=1.5):
    """Petit clic sec : bruit très court dans une bande."""
    return decroit(svf(bruit(duree), centre, q=q, mode="bande"), duree / 4.0, 0.0005)


def chiquenaude(clair=3500.0, duree=0.05):
    """Carte pichenettée : clic aigu + souffle très bref."""
    return mix((clic(clair, 0.012, 2.0), 0.0, 1.0),
               (souffle(clair * 0.6, clair * 1.6, duree, q=1.0, forme=0.1), 0.0, 0.5))


def claque(grave=180.0, duree=0.12):
    """Carte posée à plat sur une table : bruit grave + claquement."""
    corps = decroit(svf(bruit(duree), 900.0, q=0.8, mode="bas"), 0.025)
    sourd = decroit(osc("sinus", grave, duree, freq_fin=grave * 0.6), 0.03)
    return mix((corps, 0.0, 1.0), (sourd, 0.0, 0.7), (clic(4200.0, 0.01), 0.0, 0.4))


def cloches(notes, ecart=0.06, tau=0.35, rapport=3.5, indice=1.6, duree=None):
    """Arpège de cloches FM : notes jouées tous les `ecart` secondes."""
    couches = []
    for i, nom in enumerate(notes):
        d = duree if duree else tau * 4.0
        couches.append((cloche(note(nom), d, rapport=rapport, indice=indice, tau=tau),
                        i * ecart, 1.0 / (1.0 + 0.15 * i)))
    return mix(*couches)


def grosse_caisse(f0=150.0, f1=45.0, tau=0.12, duree=0.5):
    corps = decroit(osc("sinus", f0, duree, freq_fin=f1), tau)
    attaque = decroit(svf(bruit(0.02), 3000.0, q=0.7, mode="bas"), 0.005, 0.0005)
    return mix((corps, 0.0, 1.0), (attaque, 0.0, 0.5))


def coup(grave=120.0, eclat=3000.0, tau=0.07, duree=0.28, rugosite=1.8):
    """Coup de poing de jeu de combat : thump grave + éclat de bruit, un peu saturé."""
    thump = decroit(osc("sinus", grave, duree, freq_fin=grave * 0.38), tau)
    choc = decroit(svf(bruit(duree), eclat, q=0.6, mode="bas"), tau * 0.4, 0.0005)
    return sature(mix((thump, 0.0, 1.0), (choc, 0.0, 0.8),
                      (clic(eclat * 1.8, 0.01), 0.0, 0.5)), rugosite)


def metal(fondamentale=520.0, duree=0.5, tau=0.18):
    """Choc métallique (bouclier, épée parée) : partiels inharmoniques."""
    partiels = (1.0, 2.64, 4.25, 6.01, 8.3)
    couches = [(decroit(osc("sinus", fondamentale * p, duree), tau / (1 + 0.4 * k)), 0.0,
                1.0 / (1 + k)) for k, p in enumerate(partiels)]
    couches.append((clic(5000.0, 0.012), 0.0, 0.6))
    return mix(*couches)


def cuivre(freq, duree, attaque=0.03, brillance=2400.0, vibrato=0.006):
    """Timbre de cuivre synthétique : deux scies désaccordées, filtre qui s'ouvre."""
    a = osc("scie", freq, duree, vibrato=(5.5, vibrato))
    b = osc("scie", freq * 1.004, duree, vibrato=(5.1, vibrato))
    sig = [(x + y) * 0.5 for x, y in zip(a, b)]
    sig = passe_bas(sig, lambda p: 400.0 + brillance * min(1.0, p * 6.0))
    return adsr(sig, a=attaque, d=0.12, s=0.75, r=min(0.25, duree * 0.4))


def accord(noms, duree, timbre=cuivre, **options):
    return mix(*[(timbre(note(x), duree, **options), 0.0, 1.0 / len(noms)) for x in noms])


def chiptune(notes, pas=0.05, rapport=0.25, tenue=None):
    """Arpège 8 bits (carré au rapport cyclique 25 %), comme les consoles des années 80."""
    couches = []
    for i, nom in enumerate(notes):
        d = tenue if (tenue and i == len(notes) - 1) else pas * 1.05
        couches.append((adsr(osc("carre", note(nom), d, rapport=rapport), 0.002, 0.02, 0.8,
                             0.01), i * pas, 0.6))
    return mix(*couches)


def montee(duree=0.6, f0=300.0, f1=4000.0):
    """Montée de tension (riser) : souffle qui s'ouvre et s'amplifie."""
    sig = svf(bruit(duree), lambda p: f0 * (f1 / f0) ** p, q=2.0, mode="bande")
    total = len(sig)
    return [v * (i / total) ** 2 for i, v in enumerate(sig)]


def roulement(f0, f1, duree, nb, depart_coup=False):
    """Compteur de points de vie qui défile : tics carrés rapides, de f0 à f1."""
    couches = []
    for k in range(nb):
        p = k / max(1, nb - 1)
        t = duree * (p ** 0.8)
        f = f0 * (f1 / f0) ** p
        couches.append((bip(f, 0.012, "carre", tau=0.004, rapport=0.5), t, 0.5))
    if depart_coup:
        couches.append((coup(140.0, 2500.0, 0.05, 0.2, 1.4), 0.0, 0.8))
    return mix(*couches)


def choeur(noms, duree, voyelle=(700.0, 1150.0)):
    """Nappe de « chœur » : scies filtrées sur deux formants de voyelle (« a »)."""
    couches = []
    for nom in noms:
        f = note(nom)
        brut = [(x + y) * 0.5 for x, y in zip(osc("scie", f, duree, vibrato=(4.8, 0.004)),
                                              osc("scie", f * 1.006, duree, vibrato=(5.3, 0.004)))]
        formant = mix((svf(brut, voyelle[0], q=4.0), 0.0, 1.0), (svf(brut, voyelle[1], q=5.0), 0.0, 0.6))
        couches.append((adsr(formant, a=duree * 0.35, d=0.1, s=1.0, r=duree * 0.4), 0.0,
                        1.0 / len(noms)))
    return mix(*couches)


def scintillement(duree=0.6, densite=40):
    """Poussière d'étoiles : petites cloches aiguës aléatoires."""
    from synthe import _alea
    couches = []
    for _ in range(densite):
        t = _alea.uniform(0.0, duree * 0.8)
        f = _alea.uniform(2500.0, 6500.0)
        couches.append((decroit(osc("sinus", f, 0.12), 0.03, 0.001), t, 0.25))
    return mix(*couches)


def corde_note(nom, duree=0.8, amorti=0.995):
    return corde(note(nom), duree, amorti=amorti)


def renverse(sig):
    return list(reversed(sig))


__all__ = ["souffle", "bip", "clic", "chiquenaude", "claque", "cloches", "grosse_caisse",
           "coup", "metal", "cuivre", "accord", "chiptune", "montee", "roulement", "choeur",
           "scintillement", "corde_note", "renverse", "passe_haut", "reverb", "gain", "SR", "n"]
