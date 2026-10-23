"""Pack Yu-Gi-Oh! : cartes, disque de duel, compteur de points de vie, mysticisme égyptien.

Tout est synthétisé : aucun son ni aucune musique de la série ou des jeux
Konami n'est utilisé. Les clins d'œil passent par les GESTES (claquer une
carte sur le disque de duel, compteur de LP qui défile, carte piège qui se
révèle) et par la gamme mineure harmonique, associée aux objets du Millénium.

Trois événements très fréquents (saisie, effacement, valeur) ne sont
volontairement pas fournis : ils reprennent ceux du pack standard (repli).
"""

from motifs import (accord, bip, chiquenaude, choeur, claque, clic, cloches, corde_note,
                    coup, cuivre, grosse_caisse, metal, montee, renverse, roulement,
                    scintillement, souffle)
from synthe import (_alea, adsr, bruit, cloche, decroit, echo, mix, note, osc, passe_bas,
                    reverb, sature, svf)

MINEUR_HARMONIQUE = ("E4", "F4", "G#4", "A4", "B4", "C5", "D#5", "E5")


def frottement(f):
    return lambda: decroit(svf(bruit(0.03), f, q=2.5, mode="bande"), 0.008, 0.002)


def valider():
    # la carte claque sur le disque, une clochette mystique répond
    return mix((claque(170, 0.1), 0.0, 1.0),
               (cloche(note("E6"), 0.4, rapport=1.5, indice=1.5, tau=0.12), 0.03, 0.35))


def deplacement(f):
    return lambda: chiquenaude(f, 0.04)


def eventail():
    return mix(*[(chiquenaude(2800 + 500 * i, 0.04), 0.045 * i, 0.8) for i in range(3)])


def rassemble():
    return mix((chiquenaude(3200, 0.04), 0.0, 0.7), (chiquenaude(2400, 0.04), 0.05, 0.7),
               (claque(140, 0.08), 0.1, 0.6))


def tape_paquet():
    return mix((decroit(osc("sinus", 160, 0.07, freq_fin=110), 0.02), 0.0, 1.0),
               (decroit(svf(bruit(0.05), 700, q=0.8, mode="bas"), 0.012), 0.0, 0.8))


def refus():
    return passe_bas(cloche(110, 0.25, rapport=1.4, indice=2.0, tau=0.08), 900)


def erreur():
    # « Carte piège ! » : un accord dissonant qui claque, ton action est contrée
    stab = accord(("E4", "F4", "B4"), 0.4, brillance=3500, attaque=0.004)
    return mix((decroit(stab, 0.16), 0.0, 0.9), (coup(150, 2500, 0.04, 0.2, 1.4), 0.0, 0.6))


def succes():
    return cloches(("E6", "G#6", "B6"), ecart=0.05, tau=0.14, rapport=1.5, indice=1.0)


def info():
    # deux bips du disque de duel
    return mix((bip(1320, 0.05, "carre", 0.02, 0.5), 0.0, 0.5), (bip(1320, 0.05, "carre", 0.02, 0.5), 0.08, 0.5))


def retourne(sens=1):
    s = souffle(1200, 5200, 0.2, q=1.2, forme=0.5) if sens > 0 else souffle(5200, 1200, 0.2, q=1.2, forme=0.3)
    return mix((s, 0.0, 1.0), (clic(4500, 0.012), 0.19 if sens > 0 else 0.0, 0.6))


def app_ouverture():
    # le royaume des ombres s'ouvre : gong grave, oud en mineur harmonique, poussière dorée
    gong = cloche(55, 2.4, rapport=1.4, indice=3.0, tau=1.0)
    oud = mix(*[(corde_note(x, 1.0, 0.994), 0.3 + 0.11 * i, 0.55)
                for i, x in enumerate(MINEUR_HARMONIQUE[:6] + ("E5",))])
    return reverb(mix((gong, 0.0, 0.8), (oud, 0.0, 0.8), (scintillement(1.0, 40), 1.0, 0.4),
                      (choeur(("E3", "B3"), 1.8), 0.2, 0.35)), taille=1.2, melange=0.35, queue=0.6)


def app_fermeture():
    descente = mix(*[(corde_note(x, 0.9, 0.994), 0.13 * i, 0.6)
                     for i, x in enumerate(("E5", "C5", "B4", "G#4", "E4"))])
    return reverb(mix((descente, 0.0, 1.0), (claque(120, 0.12), 0.75, 0.7)),
                  taille=1.0, melange=0.3, queue=0.5)


def exodia():
    # hommage à l'Interdit : cinq coups qui montent (cinq pièces réunies), puis l'explosion
    couches = []
    for i, nom in enumerate(("E3", "G#3", "B3", "E4", "G#4")):
        couches.append((grosse_caisse(note(nom) * 1.5, note(nom) * 0.5, 0.18, 0.5), 0.2 * i, 0.8))
        couches.append((cuivre(note(nom), 0.18, attaque=0.005, brillance=2500), 0.2 * i, 0.5))
    couches.append((accord(("E4", "G#4", "B4", "E5"), 1.0, brillance=3500, attaque=0.01), 1.0, 0.8))
    couches.append((sature(decroit(bruit(1.0), 0.25), 2.0), 1.0, 0.35))
    couches.append((scintillement(0.8, 50), 1.05, 0.5))
    return reverb(mix(*couches), taille=1.2, melange=0.3, queue=0.6)


def pose_carte(f):
    # carte posée sur le disque de duel : claquement puis verrou mécanique
    return lambda: mix((claque(f, 0.1), 0.0, 1.0), (clic(5200, 0.008), 0.07, 0.6),
                       (clic(6100, 0.008), 0.095, 0.5))


def retire_carte():
    return souffle(4200, 1400, 0.15, q=1.2, forme=0.15)


def revele_carte():
    return mix((retourne(1), 0.0, 1.0),
               (cloches(("B5", "E6", "G#6"), ecart=0.04, tau=0.15, rapport=1.5, indice=1.0), 0.2, 0.5))


def glisse(a, b):
    return lambda: souffle(a, b, 0.12, q=1.4, forme=0.25)


def classe():
    hauteur = MINEUR_HARMONIQUE[_alea.randrange(len(MINEUR_HARMONIQUE))]
    return mix((retourne(1), 0.0, 0.8),
               (cloche(note(hauteur) * 2, 0.45, rapport=1.5, indice=1.0, tau=0.14), 0.15, 0.5))


def melange_paquet():
    # mélange du deck : une rafale de pichenettes, puis le verrou du disque
    couches = [(chiquenaude(_alea.uniform(2400, 4200), 0.03), 0.04 * k, 0.55) for k in range(12)]
    couches.append((claque(130, 0.1), 0.5, 0.8))
    couches.append((clic(5000, 0.01), 0.56, 0.6))
    return mix(*couches)


def invocation():
    # « Je l'invoque en mode attaque ! » : montée d'énergie, impact, chœur majeur
    couches = [(montee(0.6, 200, 6000), 0.0, 0.6),
               (grosse_caisse(160, 35, 0.3, 0.9), 0.6, 1.0),
               (choeur(("E4", "G#4", "B4"), 1.0), 0.6, 0.7),
               (scintillement(0.7, 40), 0.65, 0.5)]
    return reverb(mix(*couches), taille=1.1, melange=0.3, queue=0.5)


def recherche_vide():
    a = adsr(osc("carre", note("B3"), 0.12, rapport=0.3), 0.003, 0.03, 0.6, 0.03)
    b = adsr(osc("carre", note("A3"), 0.2, rapport=0.3), 0.003, 0.05, 0.5, 0.08)
    return passe_bas(mix((a, 0.0, 1.0), (b, 0.13, 1.0)), 1600)


def boite_deck():
    return mix((claque(110, 0.12), 0.0, 1.0), (clic(4800, 0.01), 0.1, 0.7),
               (cloches(("E6", "B6"), ecart=0.08, tau=0.25, rapport=1.5, indice=1.0), 0.16, 0.5))


def pioche():
    # « Je pioche ! » : la carte glisse hors du deck, et le cœur des cartes scintille
    return mix((souffle(1500, 5500, 0.18, q=1.3, forme=0.5), 0.0, 1.0),
               (cloches(("B5", "E6", "G#6", "B6"), ecart=0.05, tau=0.2, rapport=1.5, indice=0.8), 0.15, 0.6),
               (scintillement(0.5, 25), 0.18, 0.4))


def duel():
    # « C'est l'heure du duel ! » : deux coups de tambour, puis un accord de cuivres
    couches = [(grosse_caisse(90, 40, 0.25, 0.7), 0.0, 1.0),
               (coup(80, 1800, 0.08, 0.4, 1.3), 0.0, 0.5),
               (grosse_caisse(90, 40, 0.25, 0.7), 0.25, 1.0),
               (coup(80, 1800, 0.08, 0.4, 1.3), 0.25, 0.5),
               (accord(("E3", "B3", "E4", "B4"), 0.9, brillance=2800, attaque=0.02), 0.5, 0.9)]
    return reverb(mix(*couches), taille=1.2, melange=0.3, queue=0.5)


def activation():
    # carte face cachée qui se révèle : souffle inversé qui gonfle, puis éclat
    inverse = renverse(reverb(cloches(("E5", "G#5", "B5"), ecart=0.0, tau=0.25, rapport=1.5, indice=1.5),
                              taille=1.0, melange=0.8, queue=0.3))
    return mix((inverse, 0.0, 0.7), (metal(880, 0.4, 0.15), len(inverse) / 44100.0, 0.5),
               (scintillement(0.4, 20), len(inverse) / 44100.0, 0.4))


def perte_lp(longueur, nb, f0):
    return lambda: roulement(f0, f0 * 0.62, longueur, nb, depart_coup=True)


def barriere():
    return mix((cloches(("B5", "E6", "G#6", "B6"), ecart=0.0, tau=0.2, rapport=2.7, indice=2.0), 0.0, 0.8),
               (metal(700, 0.4, 0.12), 0.0, 0.5))


def attaque_directe():
    souffle_fort = sature(decroit(svf(bruit(0.5), 900, q=0.7, mode="bas"), 0.12), 2.5)
    return mix((souffle_fort, 0.0, 0.9), (grosse_caisse(100, 30, 0.25, 0.8), 0.0, 0.8),
               (roulement(2200, 1100, 0.85, 22), 0.15, 0.7))


def gain_lp():
    return mix((roulement(1400, 2400, 0.45, 12), 0.0, 0.8),
               (cloches(("E6", "B6"), ecart=0.08, tau=0.2, rapport=1.5, indice=0.8), 0.45, 0.6))


def position_defense(f0, f1):
    return lambda: mix((claque(f0, 0.1), 0.0, 1.0),
                       (adsr(osc("triangle", f1, 0.3), 0.05, 0.05, 0.6, 0.15), 0.05, 0.25))


def destruction(rare=False):
    if rare:
        # le royaume des ombres : un souffle fantomatique qui s'éloigne
        fantome = renverse(reverb(choeur(("E4", "B4"), 0.6), taille=1.2, melange=0.9, queue=0.6))
        return mix((fantome, 0.0, 0.8), (souffle(3000, 200, 0.8, q=0.8, forme=0.1), 0.6, 0.6))
    eclats = [(decroit(svf(bruit(0.3), _alea.uniform(3000, 8000), q=3.0), 0.05), _alea.uniform(0.0, 0.15), 0.4)
              for _ in range(8)]
    eclats.append((decroit(osc("sinus", 600, 0.6, freq_fin=90), 0.2), 0.0, 0.6))
    eclats.append((coup(100, 3000, 0.1, 0.4, 1.8), 0.0, 0.8))
    return reverb(mix(*eclats), taille=0.9, melange=0.25, queue=0.4)


def victoire(rare=False):
    # mélodie originale en mi : la gamme mineure harmonique se résout en majeur
    phrase = [("B4", 0.0, 0.12), ("C5", 0.12, 0.12), ("D#5", 0.24, 0.12), ("E5", 0.36, 0.5),
              ("G#5", 0.9, 0.12), ("B5", 1.02, 0.12), ("E6", 1.14, 1.1)]
    couches = [(cuivre(note(x), d, attaque=0.012, brillance=3000), t, 0.7) for x, t, d in phrase]
    couches.append((accord(("E4", "G#4", "B4", "E5"), 1.2, brillance=2500, attaque=0.02), 1.14, 0.6))
    couches += [(grosse_caisse(95, 45, 0.3, 0.8), t, 0.6) for t in (0.0, 0.36, 1.14)]
    if rare:
        # un petit Kuriboh s'invite : deux cris aigus et ronds
        for t in (1.5, 1.68):
            couches.append((decroit(osc("sinus", 1500, 0.12, freq_fin=2300, vibrato=(28, 0.04)), 0.05), t, 0.6))
    return reverb(mix(*couches), taille=1.0, melange=0.25, queue=0.6)


def defaite():
    # les LP tombent à zéro, le disque bipe en continu, l'ombre se referme
    couches = [(roulement(2200, 900, 1.2, 30), 0.0, 0.7),
               (adsr(osc("sinus", 1000, 0.9), 0.005, 0.0, 1.0, 0.25), 1.25, 0.35),
               (choeur(("E3", "G3", "B3"), 1.8), 1.2, 0.8),
               (grosse_caisse(70, 26, 0.6, 1.4), 1.2, 0.8)]
    return reverb(mix(*couches), taille=1.3, melange=0.35, queue=0.6)


SONS = {
    "survol_1": frottement(5200), "survol_2": frottement(6000),
    "valider": valider,
    "deplacement_1": deplacement(3400), "deplacement_2": deplacement(3900),
    "ouverture": eventail, "fermeture": rassemble, "onglet": tape_paquet,
    "refus": refus, "erreur": erreur, "succes": succes, "info": info,
    "transition": lambda: retourne(1), "retour": lambda: retourne(-1),
    "app_ouverture": app_ouverture, "app_fermeture": app_fermeture, "exodia": exodia,
    "pose_carte_1": pose_carte(170), "pose_carte_2": pose_carte(190),
    "retire_carte": retire_carte, "revele_carte": revele_carte,
    "glisse_1": glisse(1400, 4400), "glisse_2": glisse(1700, 5000),
    "classe": classe, "melange_paquet": melange_paquet,
    "invocation": invocation, "recherche_vide": recherche_vide,
    "boite_deck": boite_deck, "pioche": pioche, "duel": duel,
    "activation": activation,
    "perte_lp_1": perte_lp(0.42, 12, 2200), "perte_lp_2": perte_lp(0.42, 12, 2050),
    "barriere": barriere, "attaque_directe": attaque_directe, "gain_lp": gain_lp,
    "defense": position_defense(150, 220), "attaque_position": position_defense(190, 330),
    "destruction": destruction, "royaume_ombres": lambda: destruction(True),
    "victoire": victoire, "victoire_kuriboh": lambda: victoire(True),
    "defaite": defaite,
}
