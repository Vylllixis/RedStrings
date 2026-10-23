"""Petite bibliothèque de synthèse sonore, en Python pur (aucune dépendance).

Un signal est une liste de flottants entre -1 et 1, à SR échantillons par
seconde. Tout est déterministe : la même graine redonne les mêmes fichiers.
"""

import math
import random
import struct
import wave

SR = 44100
DEUX_PI = 2.0 * math.pi
_alea = random.Random(2026)


def graine(valeur):
    """Repart d'une graine connue, pour que chaque son soit reproductible."""
    _alea.seed(valeur)


def n(secondes):
    return max(1, int(round(secondes * SR)))


def note(nom):
    """« A4 », « C#5 », « Eb3 » ou un numéro MIDI -> fréquence en Hz."""
    if isinstance(nom, (int, float)):
        return 440.0 * 2.0 ** ((nom - 69) / 12.0)
    noms = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
    demi = noms[nom[0]]
    i = 1
    while i < len(nom) and nom[i] in "#b":
        demi += 1 if nom[i] == "#" else -1
        i += 1
    octave = int(nom[i:])
    return 440.0 * 2.0 ** ((demi + 12 * (octave + 1) - 69) / 12.0)


def silence(secondes):
    return [0.0] * n(secondes)


# ── Oscillateurs ────────────────────────────────────────────────────────

def _polyblep(t, dt):
    """Adoucit la discontinuité des ondes carrées et en dents de scie (moins d'aliasing)."""
    if t < dt:
        t /= dt
        return t + t - t * t - 1.0
    if t > 1.0 - dt:
        t = (t - 1.0) / dt
        return t * t + t + t + 1.0
    return 0.0


def osc(forme, freq, duree, freq_fin=None, rapport=0.5, vibrato=(0.0, 0.0), phase=0.0):
    """Oscillateur : sinus, carre, triangle, scie. freq_fin : glissando exponentiel."""
    total = n(duree)
    out = [0.0] * total
    ph = phase
    f0 = float(freq)
    f1 = float(freq_fin) if freq_fin else f0
    vit, prof = vibrato
    for i in range(total):
        f = f0 * (f1 / f0) ** (i / total) if f1 != f0 else f0
        if prof:
            f *= 1.0 + prof * math.sin(DEUX_PI * vit * i / SR)
        dt = f / SR
        ph += dt
        ph -= math.floor(ph)
        if forme == "sinus":
            v = math.sin(DEUX_PI * ph)
        elif forme == "carre":
            v = (1.0 if ph < rapport else -1.0) + _polyblep(ph, dt) \
                - _polyblep((ph - rapport) % 1.0, dt)
        elif forme == "triangle":
            v = 4.0 * abs(ph - 0.5) - 1.0
        else:
            v = 2.0 * ph - 1.0 - _polyblep(ph, dt)
        out[i] = v
    return out


def bruit(duree):
    return [_alea.uniform(-1.0, 1.0) for _ in range(n(duree))]


def cloche(freq, duree, rapport=3.5, indice=2.5, tau=0.6):
    """Synthèse FM : timbre de cloche ou de clochette, qui s'éteint en `tau` secondes."""
    total = n(duree)
    out = [0.0] * total
    for i in range(total):
        t = i / SR
        env = math.exp(-t / tau)
        mod = indice * env * math.sin(DEUX_PI * freq * rapport * t)
        out[i] = env * math.sin(DEUX_PI * freq * t + mod)
    return out


def corde(freq, duree, amorti=0.996, eclat=0.5):
    """Corde pincée (Karplus-Strong) : guitare, harpe, oud."""
    periode = max(2, int(SR / freq))
    tampon = [_alea.uniform(-1.0, 1.0) * eclat + (1.0 - eclat) * math.sin(DEUX_PI * k / periode)
              for k in range(periode)]
    total = n(duree)
    out = [0.0] * total
    j = 0
    for i in range(total):
        a = tampon[j]
        b = tampon[(j + 1) % periode]
        tampon[j] = amorti * 0.5 * (a + b)
        out[i] = a
        j = (j + 1) % periode
    return out


# ── Enveloppes et dynamique ─────────────────────────────────────────────

def adsr(sig, a=0.005, d=0.05, s=0.7, r=0.05):
    total = len(sig)
    na, nd, nr = n(a), n(d), n(r)
    out = list(sig)
    for i in range(total):
        if i < na:
            g = i / na
        elif i < na + nd:
            g = 1.0 - (1.0 - s) * (i - na) / nd
        else:
            g = s
        if i > total - nr:
            g *= max(0.0, (total - i) / nr)
        out[i] *= g
    return out


def decroit(sig, tau, attaque=0.002):
    na = n(attaque)
    return [v * min(1.0, i / na) * math.exp(-(i / SR) / tau) for i, v in enumerate(sig)]


def gain(sig, g):
    return [v * g for v in sig]


def sature(sig, force=2.0):
    k = math.tanh(force)
    return [math.tanh(force * v) / k for v in sig]


def normaliser(sig, crete=0.89):
    m = max((abs(v) for v in sig), default=0.0)
    return sig if m < 1e-9 else [v * crete / m for v in sig]


def fondus(sig, debut=0.002, fin=0.012):
    out = list(sig)
    nd, nf = min(n(debut), len(out)), min(n(fin), len(out))
    for i in range(nd):
        out[i] *= i / nd
    for i in range(nf):
        out[-1 - i] *= i / nf
    return out


def rogner(sig, seuil=0.0005):
    """Retire le silence de fin (au-dessous du seuil), en gardant une queue de 10 ms."""
    fin = len(sig)
    while fin > 0 and abs(sig[fin - 1]) < seuil:
        fin -= 1
    return sig[:min(len(sig), fin + n(0.01))] or [0.0]


# ── Filtres ─────────────────────────────────────────────────────────────

def _valeur(x, i, total):
    return x(i / total) if callable(x) else x


def passe_bas(sig, fc):
    """Filtre passe-bas à un pôle ; fc peut être une fonction de la position (0..1)."""
    out = [0.0] * len(sig)
    y = 0.0
    total = len(sig)
    for i, v in enumerate(sig):
        a = 1.0 - math.exp(-DEUX_PI * _valeur(fc, i, total) / SR)
        y += a * (v - y)
        out[i] = y
    return out


def passe_haut(sig, fc):
    bas = passe_bas(sig, fc)
    return [v - b for v, b in zip(sig, bas)]


def svf(sig, fc, q=0.7, mode="bande"):
    """Filtre à variables d'état (passe-bas, passe-bande ou passe-haut), résonant."""
    out = [0.0] * len(sig)
    bas = bande = 0.0
    total = len(sig)
    amort = 1.0 / q
    for i, v in enumerate(sig):
        f = 2.0 * math.sin(math.pi * min(_valeur(fc, i, total), SR / 6.0) / SR)
        f = min(f, 0.9 * (2.0 - amort))     # au-delà, le filtre diverge
        haut = v - bas - amort * bande
        bande += f * haut
        bas += f * bande
        out[i] = bas if mode == "bas" else (bande if mode == "bande" else haut)
    return out


# ── Assemblage et effets ────────────────────────────────────────────────

def mix(*couches):
    """couches : (signal, décalage en secondes, gain). La longueur suit la plus longue."""
    total = max(n(dec) + len(sig) for sig, dec, _ in couches)
    out = [0.0] * total
    for sig, dec, g in couches:
        d = n(dec) if dec > 0 else 0
        for i, v in enumerate(sig):
            out[d + i] += v * g
    return out


def echo(sig, temps=0.18, retour=0.35, fois=4, melange=0.4):
    d = n(temps)
    out = list(sig) + [0.0] * (d * fois)
    for k in range(1, fois + 1):
        g = melange * retour ** (k - 1)
        for i, v in enumerate(sig):
            out[i + d * k] += v * g
    return out


def reverb(sig, taille=1.0, melange=0.25, queue=1.2):
    """Réverbération de Schroeder (4 filtres en peigne, 2 passe-tout)."""
    entree = list(sig) + [0.0] * n(queue)
    peignes = [int(d * taille) for d in (1557, 1617, 1491, 1422)]
    humide = [0.0] * len(entree)
    for d in peignes:
        tampon = [0.0] * d
        j = 0
        for i, v in enumerate(entree):
            y = tampon[j]
            tampon[j] = v + y * 0.82
            humide[i] += y * 0.25
            j = (j + 1) % d
    for d in (225, 556):
        tampon = [0.0] * d
        j = 0
        for i, v in enumerate(humide):
            y = tampon[j]
            tampon[j] = v + y * 0.5
            humide[i] = y - v * 0.5
            j = (j + 1) % d
    return [s + melange * h for s, h in zip(entree, humide)]


def ecrire_wav(chemin, gauche, droite=None, sr=None):
    """WAV 16 bits, mono ou stéréo."""
    sr = sr or SR
    with wave.open(chemin, "wb") as f:
        f.setnchannels(1 if droite is None else 2)
        f.setsampwidth(2)
        f.setframerate(sr)
        if droite is None:
            donnees = struct.pack("<%dh" % len(gauche),
                                  *(int(max(-1.0, min(1.0, v)) * 32767) for v in gauche))
        else:
            donnees = struct.pack("<%dh" % (2 * len(gauche)),
                                  *(int(max(-1.0, min(1.0, v)) * 32767)
                                    for paire in zip(gauche, droite) for v in paire))
        f.writeframes(donnees)
