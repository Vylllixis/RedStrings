"""Les quatre musiques d'ambiance, composées pour RedStrings (mélodies originales).

Chaque musique est une boucle exacte de 16 mesures. Tout ce qui déborde de la
fin (queue d'une note, écho, réverbération) est replié au début : la boucle
s'enchaîne sans coupure audible.

- standard/menu   : synthwave calme, la mineur, 96 BPM
- standard/combat : synthwave nerveuse, ré mineur, 132 BPM
- yugioh/menu     : ambiance égyptienne, mi mineur harmonique, 84 BPM (oud, doum-tek)
- yugioh/combat   : « duel », mi mineur, 140 BPM (cordes, cuivres, timbales)
"""

import math

import synthe
from synthe import adsr, bruit, corde, decroit, gain, note, osc, passe_bas, svf

SR_MUSIQUE = 32000


class Boucle:
    """Mélange stéréo d'une boucle de `mesures` mesures à `bpm` battements par minute."""

    def __init__(self, bpm, mesures=16):
        self.temps = 60.0 / bpm
        self.total = int(round(self.temps * 4 * mesures * SR_MUSIQUE))
        self.g = [0.0] * self.total
        self.d = [0.0] * self.total

    def t(self, mesure, temps=0.0):
        """Instant (en secondes) d'un temps donné d'une mesure (comptées à partir de 0)."""
        return (mesure * 4 + temps) * self.temps

    def ajouter(self, sig, debut, volume=1.0, pan=0.0, bus=None):
        cible_g, cible_d = bus if bus else (self.g, self.d)
        a = math.cos((pan + 1.0) * math.pi / 4.0) * volume
        b = math.sin((pan + 1.0) * math.pi / 4.0) * volume
        k = int(round(debut * SR_MUSIQUE)) % self.total
        for v in sig:
            cible_g[k] += v * a
            cible_d[k] += v * b
            k += 1
            if k == self.total:
                k = 0

    def bus(self):
        return [0.0] * self.total, [0.0] * self.total

    def echo(self, bus, delai, retour=0.4, fois=4, melange=0.35):
        """Écho ping-pong replié dans la boucle (la queue revient au début)."""
        g, d = bus
        pas = int(round(delai * SR_MUSIQUE))
        src_g, src_d = list(g), list(d)
        for k in range(1, fois + 1):
            vol = melange * retour ** (k - 1)
            dec = (pas * k) % self.total
            entree = src_d if k % 2 else src_g       # alterne les côtés
            sortie = g if k % 2 else d
            for i in range(self.total):
                sortie[(i + dec) % self.total] += entree[i] * vol

    def salle(self, bus, melange=0.25):
        """Petite réverbération (échos multiples) repliée dans la boucle."""
        g, d = bus
        src = [(x + y) * 0.5 for x, y in zip(g, d)]
        for delai, vol, cote in ((0.031, 0.5, g), (0.047, 0.45, d), (0.071, 0.35, g),
                                 (0.113, 0.3, d), (0.167, 0.22, g), (0.241, 0.15, d)):
            dec = int(delai * SR_MUSIQUE)
            for i in range(self.total):
                cote[(i + dec) % self.total] += src[i] * vol * melange

    def fusionner(self, bus, volume=1.0):
        for i in range(self.total):
            self.g[i] += bus[0][i] * volume
            self.d[i] += bus[1][i] * volume

    def master(self, crete=0.85):
        """Compression douce, puis crête à -1,4 dB."""
        m = max(max(abs(v) for v in self.g), max(abs(v) for v in self.d)) or 1.0
        g = [math.tanh(1.6 * v / m) for v in self.g]
        d = [math.tanh(1.6 * v / m) for v in self.d]
        m = max(max(abs(v) for v in g), max(abs(v) for v in d)) or 1.0
        return [v * crete / m for v in g], [v * crete / m for v in d]


# ── Instruments ─────────────────────────────────────────────────────────

_cache = {}


def nappe(noms, duree, coupure=1400.0):
    """Nappe synthwave : scies désaccordées, filtre doux. Mise en cache par accord."""
    cle = (tuple(noms), round(duree, 3), coupure)
    if cle not in _cache:
        somme = None
        for x in noms:
            for desaccord in (0.997, 1.003):
                s = osc("scie", note(x) * desaccord, duree)
                somme = s if somme is None else [a + b for a, b in zip(somme, s)]
        sig = passe_bas(somme, coupure)
        _cache[cle] = adsr([v / (2 * len(noms)) for v in sig], duree * 0.25, 0.2, 0.9, duree * 0.3)
    return _cache[cle]


def basse(nom, duree, coupure=700.0):
    s = [a + 0.6 * b for a, b in zip(osc("scie", note(nom), duree), osc("sinus", note(nom), duree))]
    return adsr(passe_bas(s, coupure), 0.004, 0.08, 0.7, 0.03)


def pincee(nom, duree=0.25, rapport=0.3, tau=0.12, coupure=3200.0):
    return passe_bas(decroit(osc("carre", note(nom), duree, rapport=rapport), tau), coupure)


def lead(nom, duree, forme="scie", coupure=2600.0):
    s = osc(forme, note(nom), duree, vibrato=(5.2, 0.006))
    return adsr(passe_bas(s, coupure), 0.02, 0.1, 0.75, min(0.12, duree * 0.3))


def kick(fort=1.0):
    corps = decroit(osc("sinus", 140, 0.35, freq_fin=42), 0.09)
    return gain(corps, fort)


def caisse_claire(ton=190.0):
    peau = decroit(osc("triangle", ton, 0.15, freq_fin=ton * 0.8), 0.04)
    timbre = decroit(svf(bruit(0.2), 3500, q=0.7, mode="bande"), 0.06)
    return [a * 0.5 + b for a, b in zip(peau, timbre)]


def charleston(duree=0.05):
    return decroit(svf(bruit(duree), 8000, q=0.9, mode="haut"), duree / 3)


def cymbale():
    return decroit(svf(bruit(1.6), 6500, q=0.5, mode="haut"), 0.5)


def doum():
    return decroit(osc("sinus", 95, 0.4, freq_fin=60), 0.14)


def tek():
    return decroit(svf(bruit(0.06), 2200, q=1.5, mode="bande"), 0.015)


def timbale(nom="E2"):
    return [a + 0.3 * b for a, b in zip(decroit(osc("sinus", note(nom), 1.0, freq_fin=note(nom) * 0.97), 0.35),
                                        decroit(svf(bruit(1.0), 400, q=0.8, mode="bas"), 0.08))]


def oud(nom, duree=0.9):
    return corde(note(nom), duree, amorti=0.993, eclat=0.6)


def ney(nom, duree):
    """Flûte de roseau : sinus + souffle, vibrato lent."""
    s = osc("sinus", note(nom), duree, vibrato=(4.5, 0.01))
    souffle = svf(bruit(duree), note(nom) * 2, q=3.0, mode="bande")
    return adsr([a + 0.15 * b for a, b in zip(s, souffle)], 0.12, 0.1, 0.85, 0.2)


def cordes(nom, duree):
    s = [a + b for a, b in zip(osc("scie", note(nom), duree), osc("scie", note(nom) * 1.005, duree))]
    return adsr(passe_bas(s, 2400), 0.006, 0.06, 0.5, 0.04)


def cuivres(nom, duree):
    from motifs import cuivre
    return cuivre(note(nom), duree, attaque=0.03, brillance=2200)
