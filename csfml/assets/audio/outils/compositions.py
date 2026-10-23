"""Partitions des quatre musiques (mélodies et grilles originales)."""

import musiques as m
from musiques import (SR_MUSIQUE, Boucle, basse, caisse_claire, charleston, cordes, cuivres,
                      cymbale, doum, kick, lead, nappe, ney, oud, pincee, tek, timbale)

__all__ = ["SR_MUSIQUE", "MUSIQUES"]


def _melodie(b, bus, notes, debut_mesure, instrument, volume, pan=0.0):
    """notes : (nom ou None, durée en temps) ; jouées à la suite à partir d'une mesure."""
    t = b.t(debut_mesure)
    for nom, d in notes:
        if nom:
            b.ajouter(instrument(nom, d * b.temps), t, volume, pan, bus)
        t += d * b.temps


def standard_menu():
    b = Boucle(96)
    grille = [("A3", "C4", "E4", "B4"), ("F3", "A3", "C4", "E4"), ("C4", "E4", "G4", "D5"),
              ("G3", "B3", "D4", "A4"), ("A3", "C4", "E4", "B4"), ("F3", "A3", "C4", "E4"),
              ("C4", "E4", "G4", "D5"), ("E3", "G#3", "B3", "D4")]
    racines = ["A1", "F1", "C2", "G1", "A1", "F1", "C2", "E1"]
    arp, voix = b.bus(), b.bus()
    for k, accord in enumerate(grille):
        mes = 2 * k
        b.ajouter(nappe(accord, 2 * 4 * b.temps, 1300), b.t(mes), 0.55, -0.25)
        b.ajouter(nappe(accord, 2 * 4 * b.temps, 1600), b.t(mes), 0.4, 0.3)
        for croche in range(16):
            r = racines[k] if croche % 4 != 2 else racines[k][0] + racines[k][1:-1] \
                + str(int(racines[k][-1]) + 1)
            b.ajouter(basse(r, b.temps * 0.45), b.t(mes, croche * 0.5), 0.42)
        motif = list(accord) + [accord[1][:-1] + str(int(accord[1][-1]) + 1)]
        for dbl in range(32):
            nom = motif[[0, 1, 2, 3, 4, 3, 2, 1][dbl % 8]]
            nom = nom[:-1] + str(int(nom[-1]) + 1)
            b.ajouter(pincee(nom, 0.2, tau=0.09), b.t(mes, dbl * 0.25), 0.2, 0.0, arp)
    for mes in range(16):
        for tps in range(4):
            b.ajouter(kick(0.9 if tps % 2 == 0 else 0.0), b.t(mes, tps), 0.55)
            if tps % 2 == 1:
                b.ajouter(caisse_claire(200), b.t(mes, tps), 0.18)
            b.ajouter(charleston(), b.t(mes, tps + 0.5), 0.08, 0.35)
    melodie = [("E5", 2), ("C5", 1), ("D5", 1), ("E5", 4),
               ("A5", 2), ("G5", 1), ("F5", 1), ("E5", 2), ("C5", 2),
               ("G5", 2), ("E5", 1), ("D5", 1), ("C5", 2), ("D5", 1), ("E5", 1),
               ("D5", 2), ("C5", 1), ("B4", 1), ("G#4", 4)]
    _melodie(b, voix, melodie, 8, lambda x, d: lead(x, d, "scie", 2000), 0.32, 0.1)
    b.echo(arp, b.temps * 0.75, 0.45, 4, 0.4)
    b.echo(voix, b.temps * 0.75, 0.35, 3, 0.3)
    b.salle(voix, 0.4)
    b.fusionner(arp)
    b.fusionner(voix)
    return b.master()


def standard_combat():
    b = Boucle(132)
    grille = [("D4", "F4", "A4"), ("A#3", "D4", "F4"), ("C4", "E4", "G4"), ("A3", "C#4", "E4")] * 2
    racines = ["D2", "A#1", "C2", "A1"] * 2
    voix = b.bus()
    for k, accord in enumerate(grille):
        mes = 2 * k
        for dbl in range(32):
            b.ajouter(basse(racines[k], b.temps * 0.22, 900), b.t(mes, dbl * 0.25), 0.4)
        for tps in range(8):
            for x in accord:
                b.ajouter(pincee(x[:-1] + str(int(x[-1]) + 1), 0.14, 0.5, 0.05, 2600),
                          b.t(mes, tps + 0.5), 0.09, 0.2)
        b.ajouter(nappe(accord, 2 * 4 * b.temps, 1100), b.t(mes), 0.35, -0.2)
    for mes in range(16):
        for tps in range(4):
            b.ajouter(kick(), b.t(mes, tps), 0.65)
            if tps % 2 == 1:
                b.ajouter(caisse_claire(180), b.t(mes, tps), 0.34)
            b.ajouter(charleston(0.04), b.t(mes, tps + 0.5), 0.12, 0.3)
            b.ajouter(charleston(0.02), b.t(mes, tps + 0.75), 0.05, -0.3)
    b.ajouter(cymbale(), 0.0, 0.18)
    b.ajouter(cymbale(), b.t(8), 0.15)
    riff = [("D5", .5), ("D5", .5), ("F5", .5), ("A5", .5), ("G5", .5), ("F5", .5), ("E5", .5), ("F5", .5),
            ("A5", 2), (None, 2),
            ("F5", .5), ("F5", .5), ("D5", .5), ("F5", .5), ("G5", .5), ("A5", .5), ("A#5", .5), ("A5", .5),
            ("F5", 2), (None, 2),
            ("E5", .5), ("E5", .5), ("G5", .5), ("C6", .5), ("A#5", .5), ("A5", .5), ("G5", .5), ("E5", .5),
            ("G5", 2), (None, 2),
            ("C#5", .5), ("E5", .5), ("A5", .5), ("E5", .5), ("C#6", 1), ("A5", 1), ("E5", 4)]
    _melodie(b, voix, riff, 8, lambda x, d: lead(x, d, "carre", 3000), 0.22, 0.0)
    b.echo(voix, b.temps * 0.75, 0.3, 3, 0.3)
    b.fusionner(voix)
    return b.master()


def yugioh_menu():
    b = Boucle(84)
    egypte = b.bus()
    motif_a = [("E4", .5), ("F4", .5), ("G#4", .5), ("A4", .5), ("G#4", .5), ("F4", .5), ("E4", 1),
               ("D4", .5), ("E4", .5), ("F4", .5), ("E4", .5), ("C4", .5), ("B3", .5), ("B3", 1)]
    motif_b = [("A4", .5), ("B4", .5), ("C5", .5), ("B4", .5), ("A4", .5), ("G#4", .5), ("A4", 1),
               ("G#4", .5), ("F4", .5), ("E4", .5), ("F4", .5), ("G#4", .5), ("F4", .5), ("E4", 1)]
    for mes, motif in ((0, motif_a), (2, motif_a), (4, motif_b), (6, motif_a),
                       (8, motif_a), (10, motif_b), (12, motif_a), (14, motif_b)):
        _melodie(b, egypte, motif, mes, lambda x, d: oud(x, 0.9), 0.5, -0.15)
    for mes in range(16):
        b.ajouter(nappe(("E2", "B2", "E3"), 4 * b.temps, 500 if mes % 2 else 650), b.t(mes), 0.35)
        for croche, frappe in enumerate("DT-TD-T-"):
            if frappe == "D":
                b.ajouter(doum(), b.t(mes, croche * 0.5), 0.55, -0.1)
            elif frappe == "T":
                b.ajouter(tek(), b.t(mes, croche * 0.5), 0.22, 0.25)
    flute = [("B4", 3), ("C5", 1), ("B4", 2), ("A4", 2), ("G#4", 4), (None, 4),
             ("E5", 3), ("D#5", 1), ("E5", 2), ("F5", 2), ("E5", 4), ("B4", 4),
             ("C5", 2), ("B4", 2), ("A4", 2), ("G#4", 2), ("A4", 2), ("F4", 2), ("E4", 8)]
    _melodie(b, egypte, flute, 4, ney, 0.3, 0.2)
    b.echo(egypte, b.temps * 1.5, 0.3, 3, 0.25)
    b.salle(egypte, 0.5)
    b.fusionner(egypte)
    return b.master(0.8)


def yugioh_combat():
    b = Boucle(140)
    grille = [("E4", "G4", "B4"), ("C4", "E4", "G4"), ("D4", "F#4", "A4"), ("B3", "D#4", "F#4")] * 2
    racines = ["E2", "C2", "D2", "B1"] * 2
    voix = b.bus()
    for k, accord in enumerate(grille):
        mes = 2 * k
        for croche in range(16):
            b.ajouter(basse(racines[k], b.temps * 0.4, 800), b.t(mes, croche * 0.5), 0.4)
            b.ajouter(cordes(accord[croche % 3], b.temps * 0.4), b.t(mes, croche * 0.5), 0.12,
                      -0.3 if croche % 2 else 0.3)
        for x in accord:
            b.ajouter(cuivres(x, b.temps * 1.5), b.t(mes), 0.12, 0.0)
        b.ajouter(timbale(racines[k][:-1] + "2"), b.t(mes), 0.5)
    for mes in range(16):
        for tps in range(4):
            b.ajouter(kick(0.9), b.t(mes, tps), 0.5 if tps % 2 == 0 else 0.3)
            if tps % 2 == 1:
                b.ajouter(caisse_claire(170), b.t(mes, tps), 0.3)
            b.ajouter(charleston(0.03), b.t(mes, tps + 0.5), 0.08, 0.3)
        if mes % 4 == 3:
            for i, ton in enumerate((260, 220, 180, 150)):
                b.ajouter(caisse_claire(ton), b.t(mes, 3 + i * 0.25), 0.25, -0.4 + 0.25 * i)
    b.ajouter(cymbale(), 0.0, 0.2)
    theme = [("B4", 1), ("E5", 1), ("F#5", 1), ("G5", 1), ("F#5", 1), ("E5", 1), ("D5", 1), ("E5", 1),
             ("G5", 2), ("E5", 1), ("C5", 1), ("D5", 1), ("E5", 1), ("G5", 2),
             ("F#5", 1), ("A5", 1), ("D6", 1), ("A5", 1), ("G5", 1), ("F#5", 1), ("E5", 1), ("D5", 1),
             ("D#5", 1), ("F#5", 1), ("B5", 2), ("A5", 1), ("G5", 1), ("F#5", 1), ("D#5", 1)]
    _melodie(b, voix, theme, 8, cuivres, 0.4, 0.0)
    b.salle(voix, 0.4)
    b.fusionner(voix)
    return b.master()


MUSIQUES = {
    "standard": {"musique_menu": standard_menu, "musique_combat": standard_combat},
    "yugioh": {"musique_menu": yugioh_menu, "musique_combat": yugioh_combat},
}

_ = m
