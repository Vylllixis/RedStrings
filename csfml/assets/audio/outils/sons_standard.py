"""Pack standard de RedStrings : synthwave, rétro, « fil rouge ».

Chaque entrée de SONS donne le nom du fichier (sans .ogg) et sa fonction de
synthèse. Les références aux jeux vidéo sont des clins d'œil de STYLE
(timbres, gestes sonores) : aucune mélodie ni aucun enregistrement d'un jeu
n'est reproduit.
"""

from motifs import (accord, bip, chiptune, choeur, clic, cloches, corde_note, coup,
                    cuivre, grosse_caisse, metal, montee, renverse, scintillement, souffle)
from synthe import (adsr, bruit, cloche, decroit, echo, gain, mix, note, osc, passe_bas,
                    reverb, sature, svf)


def survol(f):
    return lambda: mix((bip(f, 0.03, "sinus", 0.008), 0.0, 1.0),
                       (bip(f * 2, 0.02, "triangle", 0.005), 0.0, 0.2))


def valider():
    # geste de menu de JRPG : deux notes qui montent, très brèves
    return echo(mix((bip(note("E6"), 0.06, "triangle", 0.03), 0.0, 1.0),
                    (bip(note("B6"), 0.09, "triangle", 0.04), 0.055, 1.0),
                    (bip(note("B6"), 0.09, "carre", 0.02, 0.25), 0.055, 0.15)),
                0.09, 0.3, 2, 0.25)


def deplacement(f):
    return lambda: mix((bip(f, 0.035, "triangle", 0.01), 0.0, 1.0), (clic(6000, 0.006), 0.0, 0.2))


def ouverture():
    return mix((souffle(400, 3200, 0.2, forme=0.6), 0.0, 0.8),
               (cloche(note("A6"), 0.4, rapport=2.0, indice=1.2, tau=0.12), 0.08, 0.35))


def fermeture():
    return mix((souffle(3000, 380, 0.17, forme=0.2), 0.0, 0.8),
               (bip(160, 0.08, "sinus", 0.03), 0.07, 0.4))


def onglet():
    return mix((bip(880, 0.07, "sinus", 0.018), 0.0, 1.0), (clic(2600, 0.01), 0.0, 0.6))


def saisie(centre, grave):
    return lambda: mix((clic(centre, 0.02, 1.2), 0.0, 1.0), (bip(grave, 0.03, "sinus", 0.008), 0.0, 0.35))


def effacement():
    return mix((clic(1500, 0.025, 1.0), 0.0, 1.0), (bip(420, 0.05, "triangle", 0.015), 0.0, 0.3))


def refus():
    # petit « bonk » mou, sans agressivité
    son = osc("triangle", 230, 0.11, freq_fin=150)
    return passe_bas(decroit(son, 0.035), 1800)


def valeur():
    return mix((bip(1100, 0.022, "sinus", 0.007), 0.0, 1.0),
               (bip(2200, 0.015, "triangle", 0.004), 0.0, 0.15))


def erreur():
    a = passe_bas(adsr(osc("carre", note("A4"), 0.09, rapport=0.3), 0.003, 0.03, 0.6, 0.02), 2400)
    b = passe_bas(adsr(osc("carre", note("F4"), 0.13, rapport=0.3), 0.003, 0.04, 0.5, 0.05), 2000)
    return mix((a, 0.0, 1.0), (b, 0.085, 1.0))


def erreur_rare():
    # le « trombone triste » des dessins animés, en trois notes qui glissent
    notes = (("D4", "C#4", 0.22), ("C#4", "C4", 0.22), ("C4", "B3", 0.6))
    couches = []
    t = 0.0
    for a, b, d in notes:
        s = osc("scie", note(a), d, freq_fin=note(b), vibrato=(6.0, 0.012 if d > 0.3 else 0.0))
        couches.append((adsr(passe_bas(s, 1300), 0.02, 0.05, 0.8, 0.08), t, 1.0))
        t += d
    return mix(*couches)


def succes():
    return mix((cloches(("C6", "E6", "G6", "C7"), ecart=0.045, tau=0.12, rapport=2.0, indice=0.8), 0.0, 1.0),
               (bip(note("C7"), 0.25, "sinus", 0.1), 0.135, 0.3))


def info():
    return cloche(note("A6"), 0.5, rapport=3.5, indice=1.5, tau=0.15)


def transition():
    riser = osc("sinus", 220, 0.3, freq_fin=660)
    return mix((souffle(300, 2600, 0.34, q=1.0, forme=0.55), 0.0, 1.0),
               (decroit(riser, 0.12), 0.0, 0.12))


def retour():
    return souffle(2600, 280, 0.3, q=1.0, forme=0.25)


def app_ouverture():
    # mise sous tension : grave qui gonfle, accord La mineur (add9) qui s'ouvre,
    # puis le « fil rouge » pincé qui monte
    duree = 2.0
    nappe = accord(("A3", "C4", "E4", "B4"), duree, brillance=1800, attaque=0.6, vibrato=0.003)
    grave = adsr(osc("sinus", note("A1"), duree), 0.8, 0.2, 0.9, 0.6)
    fil = mix(*[(corde_note(x, 0.9), 0.35 + 0.12 * i, 0.5) for i, x in
                enumerate(("A3", "C4", "E4", "G4", "B4", "E5"))])
    final = cloche(note("A5"), 1.0, rapport=2.0, indice=1.0, tau=0.35)
    return reverb(mix((nappe, 0.0, 0.55), (grave, 0.0, 0.6), (fil, 0.0, 0.7), (final, 1.05, 0.5)),
                  taille=1.1, melange=0.3, queue=0.6)


def app_fermeture():
    return reverb(mix((cloches(("E6", "C6", "A5"), ecart=0.28, tau=0.4, rapport=2.0, indice=1.0), 0.0, 1.0),
                      (accord(("A3", "E4"), 1.3, brillance=900, attaque=0.2), 0.0, 0.35)),
                  taille=1.0, melange=0.3, queue=0.5)


def konami():
    # gratification 8 bits des années 80 : arpèges éclair, puis un accord tenu
    montant = ["C5", "E5", "G5", "C6", "E6", "G6", "C7"]
    return mix((chiptune(montant, pas=0.045), 0.0, 1.0),
               (chiptune(montant, pas=0.035), 0.34, 0.8),
               (chiptune(["C6", "G6", "C7"], pas=0.0, tenue=0.45), 0.62, 0.7))


def carte_selection(f):
    # « pop » d'objet ramassé, avec une étincelle
    return lambda: mix((decroit(osc("sinus", f, 0.07, freq_fin=f * 2.3), 0.03), 0.0, 1.0),
                       (bip(f * 4.2, 0.08, "sinus", 0.025), 0.03, 0.25))


def carte_retrait():
    return decroit(osc("sinus", 1250, 0.09, freq_fin=480), 0.035)


def carte_ouverture():
    return mix((souffle(2000, 6000, 0.14, forme=0.3), 0.0, 0.8),
               (cloche(note("E6"), 0.6, rapport=2.0, indice=1.0, tau=0.18), 0.06, 0.5))


def defilement(a, b):
    return lambda: souffle(a, b, 0.13, forme=0.3)


def classe():
    return mix((cloche(note("G6"), 0.4, rapport=2.0, indice=1.0, tau=0.12), 0.0, 1.0),
               (cloche(note("D7"), 0.4, rapport=2.0, indice=1.0, tau=0.1), 0.035, 0.6))


def equipe():
    stab = accord(("C5", "E5", "G5", "C6"), 0.32, brillance=3200, attaque=0.005)
    return echo(decroit(stab, 0.14), 0.12, 0.35, 3, 0.35)


def personnage_cree():
    # « niveau supérieur » à la manière des RPG : arpège qui monte, accord cuivré
    arpege = chiptune(["G4", "C5", "E5", "G5"], pas=0.075, rapport=0.3)
    final = accord(("C5", "E5", "G5", "C6"), 0.85, brillance=3000, attaque=0.02)
    return reverb(mix((arpege, 0.0, 0.8), (final, 0.3, 0.9),
                      (scintillement(0.7, 30), 0.3, 0.5)), taille=0.9, melange=0.22, queue=0.4)


def personnage_cree_rare():
    # version 8 bits pure, pour les nostalgiques
    return mix((chiptune(["C5", "E5", "G5", "C6", "E6", "G6"], pas=0.06), 0.0, 1.0),
               (chiptune(["E6", "G6", "C7"], pas=0.0, tenue=0.5), 0.38, 0.8))


def recherche_vide():
    # « hmm ? » : deux notes, la seconde monte comme une question
    a = bip(note("E5"), 0.12, "triangle", 0.06)
    b = decroit(osc("triangle", note("G5"), 0.24, freq_fin=note("A5")), 0.12)
    return mix((a, 0.0, 1.0), (b, 0.13, 1.0))


def sauvegarde():
    # point de sauvegarde de RPG : arpège de cloches apaisant
    return reverb(cloches(("D6", "F#6", "A6", "D7"), ecart=0.11, tau=0.45, rapport=2.0, indice=0.9),
                  taille=1.0, melange=0.3, queue=0.6)


def chargement():
    return mix((cloches(("C5", "G5", "C6"), ecart=0.08, tau=0.2, rapport=1.0, indice=0.6), 0.0, 1.0),
               (scintillement(0.4, 18), 0.18, 0.4))


def combat_debut():
    # « Round 1 » : montée, puis gros coup et gong
    gong = metal(196.0, 1.4, tau=0.6)
    return reverb(mix((montee(0.5, 250, 5000), 0.0, 0.5),
                      (grosse_caisse(170, 38, 0.25, 0.9), 0.5, 1.0),
                      (gong, 0.5, 0.45)), taille=1.2, melange=0.3, queue=0.5)


def combat_tour():
    return cloches(("A5", "E6"), ecart=0.09, tau=0.2, rapport=2.0, indice=0.8)


def action(a, b):
    return lambda: mix((souffle(a, b, 0.17, q=1.4, forme=0.4), 0.0, 1.0),
                       (souffle(b, a, 0.1, q=0.8, forme=0.2), 0.08, 0.3))


def impact(grave, eclat):
    return lambda: coup(grave, eclat, 0.07, 0.3, 1.8)


def impact_garde():
    return mix((metal(540.0, 0.5, 0.16), 0.0, 1.0), (coup(90, 1800, 0.04, 0.2, 1.2), 0.0, 0.5))


def impact_lourd():
    craquement = sature(decroit(svf(bruit(0.4), 1200, q=0.6, mode="bas"), 0.12), 3.0)
    return reverb(mix((coup(85, 2600, 0.14, 0.6, 2.4), 0.0, 1.0), (craquement, 0.01, 0.6),
                      (grosse_caisse(110, 30, 0.2, 0.7), 0.0, 0.8)), taille=0.8, melange=0.2, queue=0.4)


def soin():
    # étincelles qui montent, à la manière des soins des jeux d'aventure
    return mix((cloches(("C6", "E6", "G6", "C7", "E7"), ecart=0.065, tau=0.3, rapport=2.0, indice=0.7), 0.0, 1.0),
               (scintillement(0.8, 35), 0.1, 0.5),
               (adsr(osc("sinus", note("C5"), 0.8, freq_fin=note("C6")), 0.3, 0.1, 0.6, 0.3), 0.0, 0.2))


def garde():
    vwoom = passe_bas(osc("scie", 110, 0.45, freq_fin=330), lambda p: 300 + 2500 * p)
    return mix((adsr(vwoom, 0.03, 0.1, 0.7, 0.15), 0.0, 0.7),
               (cloche(note("A5"), 0.5, rapport=2.0, indice=1.2, tau=0.15), 0.18, 0.4))


def garde_fin():
    verre = decroit(osc("sinus", 1500, 0.32, freq_fin=650), 0.12)
    return mix((verre, 0.0, 1.0), (clic(5000, 0.01), 0.0, 0.3))


def ko():
    chute = decroit(osc("sinus", 420, 0.7, freq_fin=55), 0.3)
    return reverb(mix((chute, 0.0, 0.9), (grosse_caisse(120, 30, 0.3, 0.9), 0.45, 1.0),
                      (coup(70, 1500, 0.15, 0.5, 1.6), 0.45, 0.6)), taille=1.3, melange=0.35, queue=0.7)


def fanfare(timbre_chiptune=False):
    # mélodie originale, au rythme de triolets des fanfares de victoire des JRPG
    phrase = [("C5", 0.0, 0.11), ("E5", 0.11, 0.11), ("G5", 0.22, 0.11), ("C6", 0.33, 0.45),
              ("A5", 0.8, 0.11), ("B5", 0.91, 0.11), ("C6", 1.02, 0.11), ("E6", 1.13, 1.0)]
    couches = []
    for nom, t, d in phrase:
        if timbre_chiptune:
            s = adsr(osc("carre", note(nom), d, rapport=0.25), 0.003, 0.05, 0.7, 0.05)
        else:
            s = cuivre(note(nom), d, attaque=0.015, brillance=3200)
        couches.append((s, t, 0.8))
    fin = accord(("C5", "E5", "G5"), 1.1, brillance=2600, attaque=0.02)
    couches.append((fin, 1.13, 0.0 if timbre_chiptune else 0.6))
    couches.append((grosse_caisse(110, 50, 0.3, 0.8), 0.0, 0.5))
    couches.append((grosse_caisse(110, 50, 0.3, 0.8), 1.13, 0.6))
    return reverb(mix(*couches), taille=1.0, melange=0.25, queue=0.6)


def defaite():
    # sombre et lent : grave, chœur mineur qui gonfle (clin d'œil aux jeux exigeants)
    drone = adsr(passe_bas(osc("scie", note("A1"), 2.6), 300), 0.05, 0.3, 0.8, 0.9)
    voix = choeur(("A3", "C4", "E4"), 2.4)
    return reverb(mix((grosse_caisse(80, 28, 0.6, 1.4), 0.0, 1.0), (drone, 0.0, 0.5),
                      (voix, 0.15, 0.9)), taille=1.4, melange=0.4, queue=0.8)


SONS = {
    "survol_1": survol(2000), "survol_2": survol(2240),
    "valider": valider,
    "deplacement_1": deplacement(1500), "deplacement_2": deplacement(1650),
    "ouverture": ouverture, "fermeture": fermeture, "onglet": onglet,
    "saisie_1": saisie(3200, 170), "saisie_2": saisie(3800, 190),
    "saisie_3": saisie(2900, 160), "saisie_4": saisie(4200, 200),
    "effacement": effacement, "refus": refus, "valeur": valeur,
    "erreur": erreur, "erreur_rare": erreur_rare, "succes": succes, "info": info,
    "transition": transition, "retour": retour,
    "app_ouverture": app_ouverture, "app_fermeture": app_fermeture, "konami": konami,
    "carte_selection_1": carte_selection(620), "carte_selection_2": carte_selection(700),
    "carte_retrait": carte_retrait, "carte_ouverture": carte_ouverture,
    "defilement_1": defilement(1500, 4200), "defilement_2": defilement(1800, 4800),
    "classe": classe, "equipe": equipe,
    "personnage_cree": personnage_cree, "personnage_cree_rare": personnage_cree_rare,
    "recherche_vide": recherche_vide, "sauvegarde": sauvegarde, "chargement": chargement,
    "combat_debut": combat_debut, "combat_tour": combat_tour,
    "action_1": action(800, 3200), "action_2": action(900, 3600),
    "impact_1": impact(120, 3000), "impact_2": impact(105, 2600), "impact_3": impact(135, 3400),
    "impact_garde": impact_garde, "impact_lourd": impact_lourd,
    "soin": soin, "garde": garde, "garde_fin": garde_fin, "ko": ko,
    "victoire": lambda: fanfare(False), "victoire_rare": lambda: fanfare(True),
    "defaite": defaite,
}
