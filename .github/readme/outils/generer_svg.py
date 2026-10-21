#!/usr/bin/env python3
"""
Génère les illustrations animées du README de RedStrings.

    python3 .github/readme/outils/generer_svg.py

Toutes les animations sont en SVG autonome (SMIL, sans script ni police
externe) : GitHub les affiche dans une balise <img>. Les données des classes
sont LUES dans src/personnage/personnage.c : couleurs, catégories et
modificateurs restent ceux du jeu. À relancer après une modification du
catalogue.

Règle de robustesse : chaque élément a pour valeur de base son état FINAL,
et l'animation commence par l'état caché (values="0;0;1"). Un afficheur qui
ignore les animations montre donc l'image complète.
"""
import math
import os
import random
import re
from xml.sax.saxutils import escape

RACINE = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SORTIE = os.path.join(RACINE, ".github", "readme")

# ── Palette du jeu (csfml/include/gui.h) ───────────────────────────────
FOND = "#090b18"
SURFACE = "#161a2e"
SURFACE_HAUTE = "#212744"
TEXTE = "#eceffe"
DOUX = "#96a0c0"
FAIBLE = "#5c6688"
CYAN = "#38bdf8"
ROUGE = "#f43f5e"
VERT = "#34d399"
BLEU = "#60a5fa"
OR = "#fbbf24"
SOIN = "#67e8f9"
STATS = [("PV", VERT), ("ATQ", ROUGE), ("DEF", BLEU), ("SOIN", SOIN)]

SANS = "'Inter','Segoe UI','Noto Sans','Helvetica Neue',Arial,sans-serif"
MONO = ("'JetBrains Mono','Fira Code','Cascadia Code',ui-monospace,"
        "SFMono-Regular,Menlo,Consolas,'DejaVu Sans Mono',monospace")


def e(s):
    return escape(str(s), {'"': "&quot;"})


def ecrire(nom, contenu):
    chemin = os.path.join(SORTIE, nom)
    with open(chemin, "w", encoding="utf-8") as f:
        f.write(contenu)
    print(f"  {nom:22s} {len(contenu.encode()) / 1024:6.1f} Ko")


def hexa(rgb):
    return "#%02x%02x%02x" % rgb


# ── Données réelles : le catalogue de personnage.c ─────────────────────

def lire_classes():
    src = open(os.path.join(RACINE, "src", "personnage", "personnage.c"), encoding="utf-8").read()
    entete = open(os.path.join(RACINE, "includes", "personnage.h"), encoding="utf-8").read()
    debut = entete.index("typedef enum classe")
    ids = re.findall(r"^\s{4}([A-Z_]+),$", entete[debut:entete.index("NB_CLASSES", debut)], re.M)
    lignes = re.findall(
        r'\[(\w+)\] = \{"([^"]+)", "([^"]+)",\s*(CAT_\w+), \{(\d+), (\d+), (\d+)\}, '
        r'\{(-?\d+), (-?\d+), (-?\d+), (-?\d+)\}, (EMB_\w+)\}', src)
    classes = []
    for l in lignes:
        classes.append({
            "id": ids.index(l[0]), "nom": l[1], "description": l[2], "categorie": l[3],
            "couleur": hexa((int(l[4]), int(l[5]), int(l[6]))),
            "mods": [int(l[7]), int(l[8]), int(l[9]), int(l[10])], "embleme": l[11],
        })
    assert len(classes) == len(ids), "catalogue incomplet"
    return sorted(classes, key=lambda c: c["id"])


CATEGORIES = [("CAT_CORPS_A_CORPS", "CORPS À CORPS"), ("CAT_MAGIE", "MAGIE"),
              ("CAT_DISTANCE", "DISTANCE"), ("CAT_SOUTIEN", "SOUTIEN")]

# Valeurs par défaut de l'écran de création (csfml/src/scene_creation.c).
BASE_CREATION = [50, 30, 20, 20]


def stat_finale(classe, i, base):
    return max(0, min(99, base + classe["mods"][i]))


# ── Briques communes ───────────────────────────────────────────────────

def apparition(debut, duree=0.6, total=None):
    """Opacité 0 → 1 à `debut`, sans éclair avant (la base reste 1)."""
    total = total or debut + duree
    k = debut / total
    return (f'<animate attributeName="opacity" values="0;0;1" keyTimes="0;{k:.4f};1" '
            f'dur="{total:.2f}s" fill="freeze"/>')


def glissement(debut, dx=0, dy=20, duree=0.7):
    total = debut + duree
    k = debut / total
    return (f'<animateTransform attributeName="transform" type="translate" '
            f'values="{dx} {dy};{dx} {dy};0 0" keyTimes="0;{k:.4f};1" dur="{total:.2f}s" '
            f'calcMode="spline" keySplines="0 0 1 1;.2 .8 .2 1" fill="freeze"/>')


def trace(debut, duree=1.2):
    """Tracé progressif d'un chemin muni de pathLength="1"."""
    total = debut + duree
    k = debut / total
    return (f'<animate attributeName="stroke-dashoffset" values="1;1;0" keyTimes="0;{k:.4f};1" '
            f'dur="{total:.2f}s" calcMode="spline" keySplines="0 0 1 1;.6 0 .3 1" fill="freeze"/>')


def hexagone_pts(cx, cy, r, rot=0.0):
    return " ".join(f"{cx + r * math.cos(rot - math.pi / 2 + k * math.pi / 3):.1f},"
                    f"{cy + r * math.sin(rot - math.pi / 2 + k * math.pi / 3):.1f}"
                    for k in range(6))


def biseau(x, y, w, h, c=14):
    """Silhouette des panneaux du jeu : coins haut-gauche et bas-droite coupés."""
    return f"M{x + c},{y} H{x + w} V{y + h - c} L{x + w - c},{y + h} H{x} V{y + c} Z"


def defs_communes():
    return f'''
    <filter id="lueur" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur stdDeviation="4" result="b"/>
      <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
    <filter id="flou" x="-80%" y="-80%" width="260%" height="260%"><feGaussianBlur stdDeviation="10"/></filter>
    <linearGradient id="panneau" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#1a1f3a"/><stop offset="1" stop-color="#0c0e1c"/>
    </linearGradient>
    <pattern id="points" width="26" height="26" patternUnits="userSpaceOnUse">
      <circle cx="2" cy="2" r="1" fill="#fff" opacity=".05"/>
    </pattern>'''


def cadre(w, h, rx=24):
    return (f'<rect width="{w}" height="{h}" rx="{rx}" fill="{FOND}"/>'
            f'<rect width="{w}" height="{h}" rx="{rx}" fill="url(#points)"/>')


def bordure(w, h, rx=24):
    return (f'<rect x=".5" y=".5" width="{w - 1}" height="{h - 1}" rx="{rx}" fill="none" '
            f'stroke="#fff" stroke-opacity=".08"/>')


def etoiles(w, h, n, graine, couleurs=("#ffffff", "#ffffff", CYAN, ROUGE)):
    random.seed(graine)
    out = []
    for _ in range(n):
        x, y = random.uniform(0, w), random.uniform(0, h)
        r = random.uniform(.5, 1.6)
        d = random.uniform(2.5, 6)
        out.append(f'<circle cx="{x:.0f}" cy="{y:.0f}" r="{r:.1f}" fill="{random.choice(couleurs)}" '
                   f'opacity=".5"><animate attributeName="opacity" values=".15;.7;.15" dur="{d:.1f}s" '
                   f'begin="-{random.uniform(0, d):.1f}s" repeatCount="indefinite"/></circle>')
    return "".join(out)


def vague(phase, amp, w=1280, y0=300, n=9, x0=-40):
    pts = []
    for i in range(n + 1):
        x = x0 + i * (w - 2 * x0) / n
        pts.append((x, y0 + amp * math.sin(i * 1.1 + phase)))
    d = f"M{pts[0][0]:.1f},{pts[0][1]:.1f}"
    for i in range(1, len(pts)):
        (xa, ya), (xb, yb) = pts[i - 1], pts[i]
        cx = (xa + xb) / 2
        d += f" C{cx:.1f},{ya:.1f} {cx:.1f},{yb:.1f} {xb:.1f},{yb:.1f}"
    return d


# ════════════════════════════════════════════════════════════════════
#  1. Bannière
# ════════════════════════════════════════════════════════════════════

def banniere(classes):
    W, H = 1280, 460
    HORIZON = 300
    random.seed(11)
    # Sol en perspective : des lignes qui avancent vers le spectateur.
    sol = []
    for i in range(-12, 13):
        sol.append(f'<line x1="{640 + i * 34}" y1="{HORIZON}" x2="{640 + i * 150}" y2="{H}" '
                   f'stroke="{CYAN}" stroke-opacity=".07"/>')
    for k in range(7):
        sol.append(f'<line x1="0" x2="{W}" y1="{HORIZON}" y2="{HORIZON}" stroke="{CYAN}" stroke-opacity="0">'
                   f'<animate attributeName="y1" values="{HORIZON};{H}" dur="7s" begin="-{k:.1f}s" '
                   f'repeatCount="indefinite" calcMode="spline" keySplines=".55 0 1 .45" keyTimes="0;1"/>'
                   f'<animate attributeName="y2" values="{HORIZON};{H}" dur="7s" begin="-{k:.1f}s" '
                   f'repeatCount="indefinite" calcMode="spline" keySplines=".55 0 1 .45" keyTimes="0;1"/>'
                   f'<animate attributeName="stroke-opacity" values="0;.16" dur="7s" begin="-{k:.1f}s" '
                   f'repeatCount="indefinite"/></line>')
    # Réseau de nœuds reliés par des fils qui se tracent l'un après l'autre.
    noeuds = [(110, 92), (250, 58), (360, 130), (190, 210), (470, 70), (930, 64), (1030, 140),
              (1160, 84), (1090, 230), (820, 120), (1220, 190), (60, 250)]
    liens = [(0, 1), (1, 2), (2, 3), (3, 11), (1, 4), (4, 2), (5, 6), (6, 7), (7, 10), (6, 8),
             (9, 5), (9, 6), (8, 10)]
    reseau = []
    for k, (a, b) in enumerate(liens):
        (xa, ya), (xb, yb) = noeuds[a], noeuds[b]
        reseau.append(f'<path d="M{xa},{ya} L{xb},{yb}" stroke="{ROUGE}" stroke-opacity=".32" '
                      f'stroke-width="1" fill="none" pathLength="1" stroke-dasharray="1" '
                      f'stroke-dashoffset="0">{trace(0.6 + k * 0.12, 0.8)}</path>')
    for k, (x, y) in enumerate(noeuds):
        reseau.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="{ROUGE}">{apparition(0.5 + k * 0.08, .4)}'
                      f'<animate attributeName="r" values="2.2;3.6;2.2" dur="{3 + k % 4}s" '
                      f'begin="{k * .3:.1f}s" repeatCount="indefinite"/></circle>')
    # Particules qui montent (deux plans de parallaxe : lentes et petites, rapides et grandes).
    particules = []
    for plan, (n, vmin, vmax, rmin, rmax, op) in enumerate([(26, 12, 20, .6, 1.3, .45),
                                                            (14, 6, 10, 1.3, 2.4, .8)]):
        for _ in range(n):
            x, y = random.uniform(20, W - 20), random.uniform(120, H)
            d = random.uniform(vmin, vmax)
            col = random.choice([CYAN, ROUGE, "#fff", OR])
            particules.append(
                f'<circle cx="{x:.0f}" cy="{y:.0f}" r="{random.uniform(rmin, rmax):.1f}" fill="{col}" opacity="0">'
                f'<animateTransform attributeName="transform" type="translate" values="0 0;'
                f'{random.uniform(-30, 30):.0f} -{random.uniform(140, 260):.0f}" dur="{d:.1f}s" '
                f'begin="-{random.uniform(0, d):.1f}s" repeatCount="indefinite"/>'
                f'<animate attributeName="opacity" values="0;{op};0" dur="{d:.1f}s" '
                f'begin="-{random.uniform(0, d):.1f}s" repeatCount="indefinite"/></circle>')
    # Le fil rouge principal qui ondule, parcouru par une impulsion.
    vagues = [vague(p, 22, y0=320) for p in (0, 1.3, 2.6, 3.9, 5.2, 0)]
    # Deux sceaux (les deux camps) qui tournent de part et d'autre du logo.
    def sceau(cx, col, sens, debut):
        return (f'<g opacity="1">{apparition(debut, .8)}'
                f'<g><animateTransform attributeName="transform" type="rotate" from="0 {cx} 168" '
                f'to="{360 * sens} {cx} 168" dur="40s" repeatCount="indefinite"/>'
                f'<polygon points="{hexagone_pts(cx, 168, 54)}" fill="none" stroke="{col}" stroke-opacity=".55"/>'
                f'<circle cx="{cx}" cy="168" r="70" fill="none" stroke="{col}" stroke-opacity=".25" '
                f'stroke-dasharray="3 9"/></g>'
                f'<polygon points="{hexagone_pts(cx, 168, 36, math.pi / 6)}" fill="{col}" fill-opacity=".08" '
                f'stroke="{col}" stroke-width="1.5"><animate attributeName="fill-opacity" '
                f'values=".05;.2;.05" dur="4s" repeatCount="indefinite"/></polygon>'
                f'<circle cx="{cx}" cy="168" r="5" fill="{col}" filter="url(#lueur)"/></g>')
    # Orbes des 4 catégories, couleur de leur classe emblématique (vraies couleurs).
    par_cat = {c: [k for k in classes if k["categorie"] == c] for c, _ in CATEGORIES}
    orbes = []
    for i, (cat, label) in enumerate(CATEGORIES):
        x = 280 + i * 240
        col = par_cat[cat][0]["couleur"]
        d = 2.4 + i * .25
        orbes.append(
            f'<g>{apparition(d, .6)}'
            f'<circle cx="{x}" cy="392" r="22" fill="{col}" opacity=".18" filter="url(#flou)">'
            f'<animate attributeName="r" values="18;30;18" dur="3.4s" begin="{i * .6}s" repeatCount="indefinite"/></circle>'
            f'<polygon points="{hexagone_pts(x, 392, 9)}" fill="{col}"/>'
            f'<circle cx="{x}" cy="392" r="9" fill="none" stroke="{col}" stroke-width="1.5">'
            f'<animate attributeName="r" values="9;24" dur="2.6s" begin="{i * .7}s" repeatCount="indefinite"/>'
            f'<animate attributeName="opacity" values=".9;0" dur="2.6s" begin="{i * .7}s" repeatCount="indefinite"/></circle>'
            f'<text x="{x}" y="430" text-anchor="middle" font-family="{SANS}" font-size="11.5" '
            f'font-weight="700" letter-spacing="2.5" fill="{col}">{label}</text>'
            f'<text x="{x}" y="446" text-anchor="middle" font-family="{SANS}" font-size="10.5" '
            f'fill="{FAIBLE}">{len(par_cat[cat])} classes</text></g>')
    titre_y = 186
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="REDSTRINGS — jeu de combat au tour par tour écrit en C">
  <defs>{defs_communes()}
    <radialGradient id="ciel" cx="50%" cy="34%" r="80%">
      <stop offset="0" stop-color="#1d2353"/><stop offset=".5" stop-color="#0e1230"/><stop offset="1" stop-color="{FOND}"/>
    </radialGradient>
    <linearGradient id="reflet" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#fff" stop-opacity="0"/>
      <stop offset=".45" stop-color="#fff" stop-opacity="0"/>
      <stop offset=".5" stop-color="#fff" stop-opacity=".85"/>
      <stop offset=".55" stop-color="#fff" stop-opacity="0"/>
      <stop offset="1" stop-color="#fff" stop-opacity="0"/>
      <animateTransform attributeName="gradientTransform" type="translate" values="-1.2 0;1.2 0;1.2 0"
        keyTimes="0;.45;1" dur="6s" begin="2s" repeatCount="indefinite"/>
    </linearGradient>
    <linearGradient id="horizon" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{FOND}" stop-opacity="0"/><stop offset="1" stop-color="{FOND}"/>
    </linearGradient>
    <clipPath id="cadre"><rect width="{W}" height="{H}" rx="28"/></clipPath>
    <clipPath id="balayage"><rect x="0" y="0" width="{W}" height="{H}">
      <animate attributeName="width" values="0;0;{W}" keyTimes="0;.25;1" dur="1.8s" calcMode="spline"
        keySplines="0 0 1 1;.7 0 .3 1" fill="freeze"/></rect></clipPath>
  </defs>
  <g clip-path="url(#cadre)">
    <rect width="{W}" height="{H}" fill="url(#ciel)"/>
    <rect width="{W}" height="{H}" fill="url(#points)"/>
    <g>{etoiles(W, HORIZON, 70, 3)}</g>
    <g>{"".join(sol)}</g>
    <rect y="{HORIZON - 10}" width="{W}" height="{H - HORIZON + 10}" fill="url(#horizon)" opacity=".55"/>
    <ellipse cx="640" cy="{HORIZON}" rx="520" ry="10" fill="{ROUGE}" opacity=".35" filter="url(#flou)"/>
    <g>{"".join(reseau)}</g>
    <circle cx="640" cy="{titre_y - 20}" r="220" fill="{ROUGE}" opacity=".08" filter="url(#flou)">
      <animate attributeName="opacity" values=".05;.12;.05" dur="5s" repeatCount="indefinite"/></circle>
    {sceau(150, CYAN, 1, 1.4)}
    {sceau(1130, ROUGE, -1, 1.6)}
    <g>{"".join(particules)}</g>
    <path d="{vagues[0]}" fill="none" stroke="{ROUGE}" stroke-width="10" stroke-opacity=".08">
      <animate attributeName="d" values="{';'.join(vagues)}" dur="8s" repeatCount="indefinite"/></path>
    <path id="fil" d="{vagues[0]}" fill="none" stroke="{ROUGE}" stroke-width="2.6" stroke-linecap="round"
      filter="url(#lueur)" pathLength="1" stroke-dasharray="1" stroke-dashoffset="0">{trace(.3, 2.2)}
      <animate attributeName="d" values="{';'.join(vagues)}" dur="8s" repeatCount="indefinite"/></path>
    <circle r="5" fill="#fff" filter="url(#lueur)" opacity="0">
      <animate attributeName="opacity" values="0;0;1" keyTimes="0;.9;1" dur="2.6s" fill="freeze"/>
      <animateMotion dur="5s" begin="2.6s" repeatCount="indefinite" path="{vagues[0]}"/></circle>
    <g clip-path="url(#balayage)">
      <g opacity=".5" filter="url(#flou)">
        <text x="640" y="{titre_y}" text-anchor="middle" font-family="{SANS}" font-size="112" font-weight="900"
          letter-spacing="14" fill="{ROUGE}">REDSTRINGS</text>
        <animate attributeName="opacity" values=".25;.6;.25" dur="4s" repeatCount="indefinite"/>
      </g>
      <text x="640" y="{titre_y}" text-anchor="middle" font-family="{SANS}" font-size="112" font-weight="900"
        letter-spacing="14"><tspan fill="{ROUGE}">RED</tspan><tspan fill="{TEXTE}">STRINGS</tspan></text>
      <text x="640" y="{titre_y}" text-anchor="middle" font-family="{SANS}" font-size="112" font-weight="900"
        letter-spacing="14" fill="url(#reflet)" opacity=".55">REDSTRINGS</text>
    </g>
    <g>{apparition(1.6, .8)}{glissement(1.6, dy=10)}
      <text x="640" y="{titre_y + 50}" text-anchor="middle" font-family="{SANS}" font-size="21" fill="{DOUX}">Crée tes personnages, compose tes équipes, affronte l’ordinateur au tour par tour.</text>
      <text x="640" y="{titre_y + 84}" text-anchor="middle" font-family="{MONO}" font-size="13" letter-spacing="3" fill="{FAIBLE}">C11  ·  CSFML  ·  LIBMY.A  ·  CONSOLE + FENÊTRE  ·  ZÉRO FUITE MÉMOIRE</text>
    </g>
    <g>{"".join(orbes)}</g>
  </g>
  {bordure(W, H, 28)}
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  2. Séparateur
# ════════════════════════════════════════════════════════════════════

def separateur():
    W, H = 1200, 36
    ws = [vague(p, 6, w=W, y0=18, n=12) for p in (0, 1.6, 3.2, 4.8, 0)]
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="séparateur">
  <defs>
    <linearGradient id="fondu" x1="0" x2="1">
      <stop offset="0" stop-color="{ROUGE}" stop-opacity="0"/><stop offset=".5" stop-color="{ROUGE}"/>
      <stop offset="1" stop-color="{ROUGE}" stop-opacity="0"/>
    </linearGradient>
    <filter id="l" x="-10%" y="-200%" width="120%" height="500%"><feGaussianBlur stdDeviation="3"/></filter>
  </defs>
  <path d="{ws[0]}" fill="none" stroke="url(#fondu)" stroke-width="2">
    <animate attributeName="d" values="{';'.join(ws)}" dur="6s" repeatCount="indefinite"/></path>
  <circle r="9" fill="{ROUGE}" opacity=".5" filter="url(#l)">
    <animateMotion dur="6s" repeatCount="indefinite" path="{ws[0]}" keyPoints="0.1;0.9;0.1" keyTimes="0;.5;1" calcMode="spline" keySplines=".5 0 .5 1;.5 0 .5 1"/></circle>
  <circle r="3.5" fill="#fff">
    <animateMotion dur="6s" repeatCount="indefinite" path="{ws[0]}" keyPoints="0.1;0.9;0.1" keyTimes="0;.5;1" calcMode="spline" keySplines=".5 0 .5 1;.5 0 .5 1"/></circle>
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  3. Catalogue des classes (vraies données)
# ════════════════════════════════════════════════════════════════════

def catalogue(classes):
    W, H = 1280, 640
    COL_W, X0, ECART = 292, 26, 18
    Y0, LIGNE = 128, 56
    par_cat = {c: [k for k in classes if k["categorie"] == c] for c, _ in CATEGORIES}
    colonnes = []
    for i, (cat, label) in enumerate(CATEGORIES):
        x = X0 + i * (COL_W + ECART)
        liste = par_cat[cat]
        d0 = 0.3 + i * 0.25
        bas = Y0 + (len(liste) - 1) * LIGNE + 25
        tete = (f'<g>{apparition(d0, .5)}{glissement(d0, dy=-12)}'
                f'<polygon points="{hexagone_pts(x + 22, 74, 13)}" fill="none" stroke="{TEXTE}" stroke-opacity=".7"/>'
                f'<text x="{x + 22}" y="78.5" text-anchor="middle" font-family="{SANS}" font-size="11" '
                f'font-weight="800" fill="{TEXTE}">{len(liste)}</text>'
                f'<text x="{x + 46}" y="79" font-family="{SANS}" font-size="13.5" font-weight="800" '
                f'letter-spacing="2.6" fill="{TEXTE}">{label}</text>'
                f'<path d="M{x + 46},{92} H{x + COL_W}" stroke="{ROUGE}" stroke-opacity=".5" pathLength="1" '
                f'stroke-dasharray="1" stroke-dashoffset="0">{trace(d0 + .2, .8)}</path></g>')
        fil = (f'<path id="fil{i}" d="M{x + 22},{88} V{bas}" stroke="{ROUGE}" stroke-opacity=".45" '
               f'stroke-width="1.5" fill="none" pathLength="1" stroke-dasharray="1" stroke-dashoffset="0">'
               f'{trace(d0 + .3, 1.4)}</path>'
               f'<circle r="4" fill="#fff" filter="url(#lueur)" opacity="0">'
               f'<animate attributeName="opacity" values="0;0;.9;.9;0" keyTimes="0;.05;.15;.85;1" dur="4.5s" '
               f'begin="{2.5 + i * 1.1:.1f}s" repeatCount="indefinite"/>'
               f'<animateMotion dur="4.5s" begin="{2.5 + i * 1.1:.1f}s" repeatCount="indefinite" '
               f'path="M{x + 22},{88} V{bas}"/></circle>')
        cartes = []
        for r, c in enumerate(liste):
            y = Y0 + r * LIGNE
            d = d0 + 0.5 + r * 0.09
            barres = []
            for s, (nom_stat, col) in enumerate(STATS):
                bx = x + COL_W - 66 + s * 14
                m = c["mods"][s]
                h = abs(m) / 20 * 15
                yb = y + 25 - (h if m > 0 else 0)
                barres.append(
                    f'<rect x="{bx}" y="{y + 9}" width="9" height="32" rx="2" fill="#fff" fill-opacity=".04"/>'
                    f'<line x1="{bx - 1}" x2="{bx + 10}" y1="{y + 25}" y2="{y + 25}" stroke="#fff" stroke-opacity=".18"/>')
                if m:
                    barres.append(
                        f'<rect x="{bx}" y="{yb:.1f}" width="9" height="{h:.1f}" rx="2" fill="{col}" '
                        f'fill-opacity="{1 if m > 0 else .45}">'
                        f'<animate attributeName="height" values="0;0;{h:.1f}" keyTimes="0;{(d + .3) / (d + 1):.3f};1" '
                        f'dur="{d + 1:.2f}s" calcMode="spline" keySplines="0 0 1 1;.2 .8 .2 1" fill="freeze"/>'
                        + (f'<animate attributeName="y" values="{y + 25};{y + 25};{yb:.1f}" keyTimes="0;{(d + .3) / (d + 1):.3f};1" '
                           f'dur="{d + 1:.2f}s" calcMode="spline" keySplines="0 0 1 1;.2 .8 .2 1" fill="freeze"/>' if m > 0 else '')
                        + '</rect>')
            cartes.append(
                f'<g>{apparition(d, .45)}{glissement(d, dx=-24, dy=0, duree=.6)}'
                f'<path d="{biseau(x + 40, y, COL_W - 40, 50, 10)}" fill="url(#panneau)" stroke="{c["couleur"]}" stroke-opacity=".35"/>'
                f'<polygon points="{hexagone_pts(x + 22, y + 25, 14)}" fill="{FOND}" stroke="{c["couleur"]}" stroke-width="2"/>'
                f'<polygon points="{hexagone_pts(x + 22, y + 25, 7)}" fill="{c["couleur"]}">'
                f'<animate attributeName="opacity" values="1;.45;1" dur="3s" begin="{r * .37 + i * .5:.2f}s" repeatCount="indefinite"/></polygon>'
                f'<text x="{x + 54}" y="{y + 22}" font-family="{SANS}" font-size="14.5" font-weight="700" fill="{c["couleur"]}">{e(c["nom"])}</text>'
                f'<text x="{x + 54}" y="{y + 39}" font-family="{SANS}" font-size="10.5" fill="{FAIBLE}">{e(c["description"])}</text>'
                f'{"".join(barres)}</g>')
        colonnes.append(tete + fil + "".join(cartes))
    legende = "".join(
        f'<rect x="{430 + k * 120}" y="{H - 30}" width="10" height="10" rx="2" fill="{col}"/>'
        f'<text x="{446 + k * 120}" y="{H - 21}" font-family="{SANS}" font-size="11.5" fill="{DOUX}">{nom}</text>'
        for k, (nom, col) in enumerate(STATS))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="Catalogue des {len(classes)} classes de RedStrings, par catégorie, avec leurs modificateurs de statistiques">
  <defs>{defs_communes()}</defs>
  {cadre(W, H)}
  <text x="{W / 2}" y="36" text-anchor="middle" font-family="{SANS}" font-size="12" font-weight="700" letter-spacing="4" fill="{FAIBLE}">CATALOGUE · {len(classes)} CLASSES · DONNÉES DE src/personnage/personnage.c</text>
  {"".join(colonnes)}
  <text x="420" y="{H - 21}" text-anchor="end" font-family="{SANS}" font-size="11.5" fill="{FAIBLE}">Modificateurs (haut = bonus, bas = malus) :</text>
  {legende}
  {bordure(W, H)}
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  4. Profils comparés : un radar qui se transforme d'une classe à l'autre
# ════════════════════════════════════════════════════════════════════

def profils(classes):
    W, H = 1280, 470
    noms = ["Guerrier", "Berserker", "Paladin", "Archimage", "Assassin", "Prêtre", "Gardien", "Druide"]
    choix = [next(c for c in classes if c["nom"] == n) for n in noms]
    N = len(choix)
    TENUE, MORPH = 2.2, 0.7
    T = N * (TENUE + MORPH)
    CX, CY, R = 330, 264, 138
    angles = [-math.pi / 2 + k * math.pi / 2 for k in range(4)]

    def points(c, echelle=1.0):
        vals = [stat_finale(c, k, BASE_CREATION[k]) / 99 for k in range(4)]
        return " ".join(f"{CX + math.cos(a) * R * max(v, .05) * echelle:.1f},"
                        f"{CY + math.sin(a) * R * max(v, .05) * echelle:.1f}" for a, v in zip(angles, vals))

    # Images clés : tenue puis transformation vers la suivante.
    kt, vals_pts, vals_col = [], [], []
    for k, c in enumerate(choix):
        t0 = k * (TENUE + MORPH)
        for t in (t0, t0 + TENUE):
            kt.append(t / T)
            vals_pts.append(points(c))
            vals_col.append(c["couleur"])
    kt.append(1.0)
    vals_pts.append(points(choix[0]))
    vals_col.append(choix[0]["couleur"])
    kts = ";".join(f"{k:.4f}" for k in kt)
    splines = ";".join(".45 0 .25 1" for _ in range(len(kt) - 1))

    def visible_pendant(k):
        a = k * (TENUE + MORPH) / T
        b = ((k + 1) * (TENUE + MORPH) - MORPH * .5) / T
        if k == 0:
            return (f'<animate attributeName="opacity" values="1;0;1" keyTimes="0;{b:.4f};{1 - MORPH * .5 / T:.4f}" '
                    f'calcMode="discrete" dur="{T}s" repeatCount="indefinite"/>')
        return (f'<animate attributeName="opacity" values="0;1;0" keyTimes="0;{a - MORPH * .5 / T:.4f};{b:.4f}" '
                f'calcMode="discrete" dur="{T}s" repeatCount="indefinite"/>')

    grille = []
    for n in range(1, 5):
        r = R * n / 4
        grille.append(f'<polygon points="{" ".join(f"{CX + math.cos(a) * r:.1f},{CY + math.sin(a) * r:.1f}" for a in angles)}" '
                      f'fill="none" stroke="#fff" stroke-opacity="{.16 if n == 4 else .06}"/>')
    axes = []
    for k, ((nom, col), a) in enumerate(zip(STATS, angles)):
        x, y = CX + math.cos(a) * R, CY + math.sin(a) * R
        lx, ly = CX + math.cos(a) * (R + 30), CY + math.sin(a) * (R + 30) + 4
        axes.append(f'<line x1="{CX}" y1="{CY}" x2="{x:.1f}" y2="{y:.1f}" stroke="{col}" stroke-opacity=".3"/>'
                    f'<text x="{lx:.1f}" y="{ly:.1f}" text-anchor="middle" font-family="{SANS}" font-size="13" '
                    f'font-weight="800" letter-spacing="1.5" fill="{col}">{nom}</text>')
    # Valeurs affichées aux sommets, propres à chaque classe.
    valeurs = []
    for k, c in enumerate(choix):
        txt = []
        for s, a in enumerate(angles):
            v = stat_finale(c, s, BASE_CREATION[s])
            r = R * max(v / 99, .05)
            x, y = CX + math.cos(a) * r, CY + math.sin(a) * r
            dx = {0: 0, 1: 16, 2: 0, 3: -16}[s]
            dy = {0: -12, 1: 4, 2: 20, 3: 4}[s]
            anchor = {0: "middle", 1: "start", 2: "middle", 3: "end"}[s]
            txt.append(f'<text x="{x + dx:.1f}" y="{y + dy:.1f}" text-anchor="{anchor}" font-family="{SANS}" '
                       f'font-size="13" font-weight="800" fill="{STATS[s][1]}">{v}</text>')
        valeurs.append(f'<g opacity="{1 if k == 0 else 0}">{visible_pendant(k)}{"".join(txt)}</g>')
    # Liste des classes, avec un curseur qui suit le radar.
    liste = []
    for k, c in enumerate(choix):
        y = 92 + k * 42
        fin = [stat_finale(c, s, BASE_CREATION[s]) for s in range(4)]
        cellules = "".join(f'<text x="{1000 + s * 56}" y="{y + 5}" text-anchor="end" font-family="{MONO}" '
                           f'font-size="14" font-weight="700" fill="{STATS[s][1]}">{v}</text>'
                           for s, v in enumerate(fin))
        liste.append(f'<polygon points="{hexagone_pts(706, y, 11)}" fill="{FOND}" stroke="{c["couleur"]}" stroke-width="2"/>'
                     f'<polygon points="{hexagone_pts(706, y, 5)}" fill="{c["couleur"]}"/>'
                     f'<text x="726" y="{y + 5}" font-family="{SANS}" font-size="15" font-weight="700" fill="{TEXTE}">{e(c["nom"])}</text>'
                     f'<text x="862" y="{y + 5}" font-family="{SANS}" font-size="11" fill="{FAIBLE}">{e(dict(CATEGORIES)[c["categorie"]].lower())}</text>'
                     + cellules)
    curseur_vals, curseur_kt = [], []
    for k in range(N):
        t0 = k * (TENUE + MORPH)
        for t in (t0, t0 + TENUE):
            curseur_kt.append(t / T)
            curseur_vals.append(f"0 {k * 42}")
    curseur_kt.append(1.0)
    curseur_vals.append("0 0")
    entetes = "".join(f'<text x="{1000 + s * 56}" y="62" text-anchor="end" font-family="{SANS}" font-size="11" '
                      f'font-weight="800" letter-spacing="1" fill="{col}">{nom}</text>' for s, (nom, col) in enumerate(STATS))
    noms_grands = "".join(
        f'<g opacity="{1 if k == 0 else 0}">{visible_pendant(k)}'
        f'<text x="{CX}" y="44" text-anchor="middle" font-family="{SANS}" font-size="24" font-weight="900" '
        f'letter-spacing="3" fill="{c["couleur"]}">{e(c["nom"].upper())}</text>'
        f'<text x="{CX}" y="64" text-anchor="middle" font-family="{SANS}" font-size="12" fill="{DOUX}">{e(c["description"])}</text></g>'
        for k, c in enumerate(choix))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="Comparaison animée des statistiques finales de huit classes sur un radar à quatre axes">
  <defs>{defs_communes()}</defs>
  {cadre(W, H)}
  <circle cx="{CX}" cy="{CY}" r="168" fill="{choix[0]["couleur"]}" opacity=".10" filter="url(#flou)">
    <animate attributeName="fill" values="{';'.join(vals_col)}" keyTimes="{kts}" dur="{T}s" repeatCount="indefinite"/></circle>
  {"".join(grille)}{"".join(axes)}
  <polygon points="{vals_pts[0]}" fill="{vals_col[0]}" fill-opacity=".28" stroke="{vals_col[0]}" stroke-width="2.5" stroke-linejoin="round" filter="url(#lueur)">
    <animate attributeName="points" values="{';'.join(vals_pts)}" keyTimes="{kts}" calcMode="spline" keySplines="{splines}" dur="{T}s" repeatCount="indefinite"/>
    <animate attributeName="fill" values="{';'.join(vals_col)}" keyTimes="{kts}" dur="{T}s" repeatCount="indefinite"/>
    <animate attributeName="stroke" values="{';'.join(vals_col)}" keyTimes="{kts}" dur="{T}s" repeatCount="indefinite"/>
  </polygon>
  {"".join(valeurs)}
  {noms_grands}
  <text x="964" y="{H - 30}" text-anchor="middle" font-family="{SANS}" font-size="11.5" fill="{FAIBLE}">Statistiques finales pour la base par défaut de la création ({" / ".join(map(str, BASE_CREATION))}) + modificateurs de la classe</text>
  <path d="M640,40 V{H - 40}" stroke="#fff" stroke-opacity=".06"/>
  {entetes}
  <g>
    <path d="{biseau(686, 92 - 19, 556, 38, 9)}" fill="#fff" fill-opacity=".05" stroke="{ROUGE}" stroke-opacity=".6"/>
    <path d="M686,{92 - 10} V{92 + 10}" stroke="{ROUGE}" stroke-width="3"/>
    <animateTransform attributeName="transform" type="translate" values="{';'.join(curseur_vals)}" keyTimes="{';'.join(f"{k:.4f}" for k in curseur_kt)}" calcMode="spline" keySplines="{';'.join('.45 0 .25 1' for _ in range(len(curseur_kt) - 1))}" dur="{T}s" repeatCount="indefinite"/>
  </g>
  {"".join(liste)}
  {bordure(W, H)}
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  5. Les trois actions (nombres des tests unitaires)
# ════════════════════════════════════════════════════════════════════

def boucle(periode, *etapes, attr="opacity", spline=True, extra=""):
    """Animation en boucle : etapes = (temps_en_s, valeur), le dernier temps vaut la période."""
    kt = ";".join(f"{t / periode:.4f}" for t, _ in etapes)
    vals = ";".join(str(v) for _, v in etapes)
    s = ""
    if spline:
        s = ' calcMode="spline" keySplines="' + ";".join(".4 0 .2 1" for _ in range(len(etapes) - 1)) + '"'
    return (f'<animate attributeName="{attr}" values="{vals}" keyTimes="{kt}" dur="{periode}s" '
            f'repeatCount="indefinite"{s}{extra}/>')


def boucle_translation(periode, *etapes):
    kt = ";".join(f"{t / periode:.4f}" for t, _ in etapes)
    vals = ";".join(v for _, v in etapes)
    sp = ";".join(".4 0 .2 1" for _ in range(len(etapes) - 1))
    return (f'<animateTransform attributeName="transform" type="translate" values="{vals}" keyTimes="{kt}" '
            f'calcMode="spline" keySplines="{sp}" dur="{periode}s" repeatCount="indefinite"/>')


def jeton(x, y, r, col, sol=True, nom=None):
    out = ""
    if sol:
        out += (f'<ellipse cx="{x}" cy="{y + r * 1.25}" rx="{r * 1.05}" ry="{r * .3}" fill="#000" opacity=".45"/>'
                f'<ellipse cx="{x}" cy="{y + r * 1.25}" rx="{r * 1.05}" ry="{r * .3}" fill="none" stroke="{col}" stroke-opacity=".5"/>')
    out += (f'<circle cx="{x}" cy="{y}" r="{r * 1.6}" fill="{col}" opacity=".16" filter="url(#flou)"/>'
            f'<polygon points="{hexagone_pts(x, y, r)}" fill="#11142a" stroke="{col}" stroke-width="2.2"/>'
            f'<polygon points="{hexagone_pts(x, y, r * .78, math.pi / 6)}" fill="none" stroke="{col}" stroke-opacity=".35"/>'
            f'<polygon points="{hexagone_pts(x, y, r * .34)}" fill="{col}"/>')
    if nom:
        out += (f'<text x="{x}" y="{y - r - 12}" text-anchor="middle" font-family="{SANS}" font-size="{max(11, r * .38):.0f}" '
                f'font-weight="700" fill="{TEXTE}">{e(nom)}</text>')
    return out


def barre_pv(x, y, w, h, frac_debut, frac_fin, t_debut, t_fin, periode, couleur=VERT):
    """Barre de PV qui passe de frac_debut à frac_fin, avec une trace qui suit."""
    a, b = frac_debut * w, frac_fin * w
    trace_pv = (f'<rect x="{x}" y="{y}" width="{a:.1f}" height="{h}" rx="{h / 2}" fill="{ROUGE if b < a else VERT}" opacity=".5">'
                + boucle(periode, (0, f"{a:.1f}"), (t_debut + .45, f"{a:.1f}"), (t_fin + .5, f"{b:.1f}"),
                         (periode - .4, f"{b:.1f}"), (periode, f"{a:.1f}"), attr="width") + '</rect>')
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{h / 2}" fill="#fff" fill-opacity=".08"/>'
            + trace_pv +
            f'<rect x="{x}" y="{y}" width="{a:.1f}" height="{h}" rx="{h / 2}" fill="{couleur}">'
            + boucle(periode, (0, f"{a:.1f}"), (t_debut, f"{a:.1f}"), (t_fin, f"{b:.1f}"),
                     (periode - .4, f"{b:.1f}"), (periode, f"{a:.1f}"), attr="width") + '</rect>')


def nombre(x, y, texte, col, t, periode, taille=30):
    return (f'<text x="{x}" y="{y}" text-anchor="middle" font-family="{SANS}" font-size="{taille}" font-weight="900" '
            f'fill="{col}" stroke="{FOND}" stroke-width="4" paint-order="stroke" opacity="0">{e(texte)}'
            + boucle(periode, (0, 0), (t, 0), (t + .12, 1), (t + 1.3, 1), (t + 1.8, 0), (periode, 0))
            + boucle_translation(periode, (0, "0 10"), (t, "0 10"), (t + 1.8, "0 -26"), (periode, "0 -26"))
            + '</text>')


def actions(classes):
    W, H, P = 1280, 440, 7.0
    c = {k["nom"]: k["couleur"] for k in classes}
    PW, PH = 400, 384

    SOLS_DUEL = (f'<ellipse cx="110" cy="290" rx="120" ry="30" fill="#0e1430" stroke="{CYAN}" stroke-opacity=".5"/>'
                 f'<ellipse cx="300" cy="186" rx="80" ry="18" fill="#0e1430" stroke="{ROUGE}" stroke-opacity=".5"/>')
    SOLS_ALLIES = f'<ellipse cx="185" cy="292" rx="172" ry="32" fill="#0e1430" stroke="{CYAN}" stroke-opacity=".5"/>'

    def panneau(x, num, titre, sous_titre, couleur, corps, legende, sols=SOLS_DUEL):
        return (f'<g transform="translate({x},28)">'
                f'<path d="{biseau(0, 0, PW, PH, 22)}" fill="url(#panneau)" stroke="#fff" stroke-opacity=".08"/>'
                f'<path d="M{PW - 28},{-6} H{PW + 6} V28" fill="none" stroke="{couleur}" stroke-width="2"/>'
                f'<path d="M-6,{PH - 28} V{PH + 6} H28" fill="none" stroke="{couleur}" stroke-width="2"/>'
                f'<text x="24" y="38" font-family="{SANS}" font-size="12" font-weight="800" letter-spacing="2" fill="{couleur}">{num}</text>'
                f'<text x="52" y="38" font-family="{SANS}" font-size="19" font-weight="900" letter-spacing="2.5" fill="{TEXTE}">{titre}</text>'
                f'<text x="24" y="60" font-family="{SANS}" font-size="12" fill="{DOUX}">{sous_titre}</text>'
                f'{sols}{corps}'
                f'<path d="M24,{PH - 52} H{PW - 24}" stroke="#fff" stroke-opacity=".07"/>'
                f'<text x="24" y="{PH - 28}" font-family="{MONO}" font-size="12" fill="{TEXTE}">{legende[0]}</text>'
                f'<text x="24" y="{PH - 11}" font-family="{SANS}" font-size="11" fill="{FAIBLE}">{legende[1]}</text>'
                f'</g>')

    # ── Attaquer : Guerrier (40 ATQ) contre une cible à 50 % de défense → 20.
    ti = 1.05
    eclats = "".join(
        f'<circle cx="300" cy="160" r="3" fill="{c["Guerrier"]}" opacity="0">'
        + boucle(P, (0, 0), (ti, 0), (ti + .05, 1), (ti + .6, 0), (P, 0))
        + boucle_translation(P, (0, "0 0"), (ti, "0 0"),
                             (ti + .6, f"{math.cos(k * .785) * 46:.0f} {math.sin(k * .785) * 46:.0f}"),
                             (P, f"{math.cos(k * .785) * 46:.0f} {math.sin(k * .785) * 46:.0f}"))
        + '</circle>' for k in range(8))
    attaque = (
        f'<g>{boucle_translation(P, (0, "0 0"), (ti, "0 0"), (ti + .08, "5 -3"), (ti + .16, "-5 2"), (ti + .24, "3 -1"), (ti + .3, "0 0"), (P, "0 0"))}'
        f'{jeton(300, 160, 26, c["Berserker"], nom="Cible")}'
        f'<polygon points="{hexagone_pts(300, 160, 26)}" fill="#fff" opacity="0">'
        + boucle(P, (0, 0), (ti, 0), (ti + .04, .85), (ti + .4, 0), (P, 0)) + '</polygon></g>'
        + barre_pv(258, 98, 84, 5, 1, .6, ti, ti + .5, P)
        + f'<g>{boucle_translation(P, (0, "0 0"), (.6, "0 0"), (.8, "-10 8"), (ti, "130 -82"), (ti + .45, "0 0"), (P, "0 0"))}'
        + jeton(110, 252, 38, c["Guerrier"], nom="Guerrier · ATQ 40") + '</g>'
        + f'<path d="M150,230 L280,170" stroke="{c["Guerrier"]}" stroke-width="10" stroke-linecap="round" opacity="0">'
        + boucle(P, (0, 0), (.85, 0), (ti, .35), (ti + .25, 0), (P, 0)) + '</path>'
        + "".join(f'<path d="{d}" stroke="#fff" stroke-width="3" stroke-linecap="round" opacity="0">'
                  + boucle(P, (0, 0), (ti, 0), (ti + .05, 1), (ti + .4, 0), (P, 0)) + '</path>'
                  for d in ("M272,132 L328,188", "M328,132 L272,188"))
        + eclats + nombre(345, 128, "-20", ROUGE, ti, P))
    # ── Soigner : Prêtre (soin 35) sur un allié à 40 / 100 → 75 / 100.
    ts = 1.6
    arc_soin = "M128,240 Q196,130 246,238"
    orbes = "".join(
        f'<circle r="4.5" fill="{SOIN}" filter="url(#lueur)" opacity="0">'
        + boucle(P, (0, 0), (.95 + k * .12, 0), (1.0 + k * .12, 1), (ts, 1), (ts + .1, 0), (P, 0))
        + f'<animateMotion path="{arc_soin}" dur="{P}s" repeatCount="indefinite" keyPoints="0;0;1;1" '
          f'keyTimes="0;{(.95 + k * .12) / P:.4f};{(ts - .05) / P:.4f};1" calcMode="linear"/></circle>'
        for k in range(4))
    montee = "".join(
        f'<circle cx="{254 + k * 9}" cy="256" r="2.4" fill="{SOIN}" opacity="0">'
        + boucle(P, (0, 0), (ts + k * .05, 0), (ts + .1 + k * .05, .9), (ts + 1.2, 0), (P, 0))
        + boucle_translation(P, (0, "0 0"), (ts, "0 0"), (ts + 1.2, f"{(k - 2) * 4} -60"), (P, f"{(k - 2) * 4} -60"))
        + '</circle>' for k in range(5))
    soin = (
        f'<g>{jeton(96, 262, 34, c["Prêtre"], nom="Prêtre · SOIN 35")}'
        f'<circle cx="96" cy="262" r="48" fill="none" stroke="{SOIN}" stroke-width="2" opacity="0">'
        + boucle(P, (0, 0), (.6, 0), (.8, .8), (1.4, .3), (1.7, 0), (P, 0)) + '</circle></g>'
        + f'<g>{jeton(272, 254, 30, c["Archer"], nom="Allié")}'
        f'<circle cx="272" cy="254" r="40" fill="none" stroke="{SOIN}" stroke-width="3" opacity="0">'
        + boucle(P, (0, 0), (ts, 0), (ts + .1, 1), (ts + .9, 0), (P, 0))
        + boucle(P, (0, 30), (ts, 30), (ts + .9, 70), (P, 70), attr="r") + '</circle></g>'
        + barre_pv(228, 190, 88, 6, .4, .75, ts, ts + .6, P)
        + f'<path d="{arc_soin}" fill="none" stroke="{SOIN}" stroke-width="10" stroke-opacity=".18" pathLength="1" stroke-dasharray="1" stroke-dashoffset="1">'
        + boucle(P, (0, 1), (.95, 1), (ts, 0), (ts + .8, 0), (ts + 1.2, -1), (P, -1), attr="stroke-dashoffset") + '</path>'
        + f'<path d="{arc_soin}" fill="none" stroke="{SOIN}" stroke-width="2.5" pathLength="1" stroke-dasharray="1" stroke-dashoffset="1">'
        + boucle(P, (0, 1), (.95, 1), (ts, 0), (ts + .8, 0), (ts + 1.2, -1), (P, -1), attr="stroke-dashoffset") + '</path>'
        + orbes + montee + nombre(336, 222, "+35", VERT, ts, P))
    # ── Défendre : garde -50 %, un coup de 40 devient 20, puis la garde expire.
    tg, tc, tf = .7, 2.4, 4.9
    defense = (
        f'<g>{boucle_translation(P, (0, "0 0"), (2.0, "0 0"), (tc, "-120 66"), (tc + .45, "0 0"), (P, "0 0"))}'
        + jeton(300, 160, 26, c["Berserker"], nom="Berserker · 40") + '</g>'
        + jeton(110, 252, 38, c["Guerrier"])
        + f'<text x="110" y="166" text-anchor="middle" font-family="{SANS}" font-size="14" font-weight="700" fill="{TEXTE}">Guerrier</text>'
        + f'<g opacity="0">'
        + boucle(P, (0, 0), (tg, 0), (tg + .3, 1), (tf, 1), (tf + .6, 0), (P, 0))
        + f'<polygon points="{hexagone_pts(110, 252, 58)}" fill="{BLEU}" fill-opacity=".12" stroke="{BLEU}" stroke-width="2.5">'
        + f'<animateTransform attributeName="transform" type="rotate" from="0 110 252" to="360 110 252" dur="16s" repeatCount="indefinite"/>'
        + '</polygon>'
        + f'<polygon points="{hexagone_pts(110, 252, 50, math.pi / 6)}" fill="none" stroke="{BLEU}" stroke-opacity=".45"/>'
        + f'<polygon points="{hexagone_pts(110, 252, 58)}" fill="#fff" opacity="0">'
        + boucle(P, (0, 0), (tc, 0), (tc + .05, .7), (tc + .5, 0), (P, 0)) + '</polygon></g>'
        + f'<text x="110" y="144" text-anchor="middle" font-family="{SANS}" font-size="13" font-weight="900" letter-spacing="2" fill="{BLEU}" opacity="0">GARDE −50 %'
        + boucle(P, (0, 0), (tg + .2, 0), (tg + .5, 1), (tc - .3, 1), (tc, 0), (P, 0)) + '</text>'
        + barre_pv(70, 176, 80, 6, 1, .8, tc, tc + .5, P)
        + f'<text x="190" y="214" text-anchor="middle" font-family="{SANS}" font-size="20" font-weight="900" fill="{DOUX}" text-decoration="line-through" opacity="0">40'
        + boucle(P, (0, 0), (tc, 0), (tc + .1, 1), (tc + .6, 1), (tc + .9, 0), (P, 0)) + '</text>'
        + nombre(196, 190, "-20", BLEU, tc + .35, P)
        + f'<text x="110" y="356" text-anchor="middle" font-family="{SANS}" font-size="12" font-weight="700" fill="{FAIBLE}" opacity="0">tour suivant : la garde expire'
        + boucle(P, (0, 0), (tf - .2, 0), (tf + .1, 1), (tf + 1.4, 1), (tf + 1.8, 0), (P, 0)) + '</text>')
    corps = (panneau(24, "01", "ATTAQUER", "Un ennemi vivant · dégâts au moins 1", ROUGE, attaque,
                     ("40 × (100 − 50) / 100 = 20", "attaque × (100 − défense) / 100, PV jamais négatifs"))
             + panneau(440, "02", "SOIGNER", "Un allié vivant blessé, soi compris", SOIN, soin,
                       ("40 / 100  →  75 / 100  (+35)", "plafonné aux PV max · impossible si personne n'est blessé"),
                       sols=SOLS_ALLIES)
             + panneau(856, "03", "DÉFENDRE", "Soi-même · effet temporaire", BLEU, defense,
                       ("coup de 40 en garde  →  20", "−50 % jusqu'au début de son prochain tour")))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="Les trois actions du combat : attaquer (20 dégâts), soigner (+35 PV), défendre (dégâts divisés par deux)">
  <defs>{defs_communes()}</defs>
  {cadre(W, H)}
  {corps}
  {bordure(W, H)}
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  6. Ordre des tours : alternance, et un personnage vaincu est sauté
# ════════════════════════════════════════════════════════════════════

def tours(classes):
    W, H, P = 1280, 300, 13.0
    c = {k["nom"]: k["couleur"] for k in classes}
    ordre = [("Kaelen", "Guerrier", 0), ("Varek", "Berserker", 1), ("Elara", "Prêtre", 0),
             ("Nyx", "Assassin", 1), ("Sylas", "Archer", 0)]
    xs = [200 + k * 220 for k in range(5)]
    Y = 150
    pas = 1.2
    # Manche 1 : 0..4 ; Nyx tombe ; manche 2 : 0, 1, 2, (Nyx sauté), 4.
    arrets = [(k * pas, xs[k]) for k in range(5)]
    t_ko = 5 * pas + .2
    m2 = 6.6
    arrets2 = [(m2, xs[0]), (m2 + pas, xs[1]), (m2 + 2 * pas, xs[2]), (m2 + 3 * pas + .5, xs[4])]
    etapes = []
    for t, x in arrets:
        etapes += [(t, f"{x - xs[0]} 0"), (t + pas - .35, f"{x - xs[0]} 0")]
    etapes += [(m2 - .35, f"{xs[4] - xs[0]} 0")]
    etapes += [(m2, "0 0"), (m2 + pas - .35, "0 0"), (m2 + pas, f"{xs[1] - xs[0]} 0"),
               (m2 + 2 * pas - .35, f"{xs[1] - xs[0]} 0"), (m2 + 2 * pas, f"{xs[2] - xs[0]} 0"),
               (m2 + 3 * pas - .35, f"{xs[2] - xs[0]} 0"), (m2 + 3 * pas + .1, f"{xs[3] - xs[0]} -34"),
               (m2 + 3 * pas + .5, f"{xs[4] - xs[0]} 0"), (P - .6, f"{xs[4] - xs[0]} 0"), (P, "0 0")]
    curseur = (f'<g>{boucle_translation(P, *etapes)}'
               f'<circle cx="{xs[0]}" cy="{Y}" r="50" fill="none" stroke="#fff" stroke-width="2" stroke-opacity=".8">'
               f'<animate attributeName="r" values="46;52;46" dur="1.2s" repeatCount="indefinite"/></circle>'
               f'<polygon points="{xs[0]},{Y - 74} {xs[0] + 8},{Y - 66} {xs[0]},{Y - 58} {xs[0] - 8},{Y - 66}" fill="#fff" filter="url(#lueur)"/>'
               f'</g>')
    jetons = []
    for k, (nom, classe, eq) in enumerate(ordre):
        col = c[classe]
        equipe = CYAN if eq == 0 else ROUGE
        ko = nom == "Nyx"
        corps = jeton(xs[k], Y, 34, col, sol=False)
        if ko:
            corps = (f'<g>{boucle(P, (0, 1), (t_ko, 1), (t_ko + .4, .28), (P - .5, .28), (P, 1))}{corps}</g>'
                     f'<g opacity="0">{boucle(P, (0, 0), (t_ko, 0), (t_ko + .3, 1), (P - .5, 1), (P, 0))}'
                     f'<rect x="{xs[k] - 24}" y="{Y - 10}" width="48" height="20" rx="10" fill="{ROUGE}"/>'
                     f'<text x="{xs[k]}" y="{Y + 4.5}" text-anchor="middle" font-family="{SANS}" font-size="11" font-weight="900" fill="#fff">K.O.</text></g>'
                     f'<text x="{xs[k] + 22}" y="{Y - 82}" text-anchor="start" font-family="{SANS}" font-size="12" font-weight="800" fill="{ROUGE}" opacity="0">sauté'
                     + boucle(P, (0, 0), (m2 + 3 * pas - .4, 0), (m2 + 3 * pas, 1), (m2 + 3 * pas + 1.2, 1), (m2 + 3 * pas + 1.6, 0), (P, 0)) + '</text>')
        jetons.append(corps
                      + f'<rect x="{xs[k] - 22}" y="{Y + 50}" width="44" height="3" rx="1.5" fill="{equipe}"/>'
                      + f'<text x="{xs[k]}" y="{Y + 76}" text-anchor="middle" font-family="{SANS}" font-size="15" font-weight="700" fill="{TEXTE}">{nom}</text>'
                      + f'<text x="{xs[k]}" y="{Y + 94}" text-anchor="middle" font-family="{SANS}" font-size="11" fill="{col}">{classe} · {"allié" if eq == 0 else "adversaire"} {sum(1 for o in ordre[:k + 1] if o[2] == eq)}</text>')
    # Le coup qui met Nyx K.O. à la fin de la manche 1.
    coup = (f'<path d="M{xs[4]},{Y - 40} Q{(xs[3] + xs[4]) / 2},{Y - 110} {xs[3]},{Y - 40}" fill="none" stroke="{c["Archer"]}" '
            f'stroke-width="3" pathLength="1" stroke-dasharray="1" stroke-dashoffset="1">'
            + boucle(P, (0, 1), (4 * pas + .3, 1), (t_ko, 0), (t_ko + .5, 0), (t_ko + .9, -1), (P, -1), attr="stroke-dashoffset") + '</path>')
    manches = (f'<text x="640" y="40" text-anchor="middle" font-family="{SANS}" font-size="13" font-weight="800" letter-spacing="3" fill="{DOUX}">'
               f'<tspan>MANCHE 1'
               + f'<animate attributeName="opacity" values="1;0;1" keyTimes="0;{(m2 - .3) / P:.4f};{(P - .3) / P:.4f}" calcMode="discrete" dur="{P}s" repeatCount="indefinite"/></tspan></text>'
               f'<text x="640" y="40" text-anchor="middle" font-family="{SANS}" font-size="13" font-weight="800" letter-spacing="3" fill="{DOUX}" opacity="0">MANCHE 2'
               + f'<animate attributeName="opacity" values="0;1;0" keyTimes="0;{(m2 - .3) / P:.4f};{(P - .3) / P:.4f}" calcMode="discrete" dur="{P}s" repeatCount="indefinite"/></text>')
    fil = f'<path d="M{xs[0]},{Y} H{xs[-1]}" stroke="{ROUGE}" stroke-opacity=".35" stroke-width="1.5" stroke-dasharray="2 6"/>'
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="Ordre des tours : les équipes alternent, et un personnage vaincu est sauté">
  <defs>{defs_communes()}</defs>
  {cadre(W, H)}
  {manches}
  <text x="640" y="{H - 18}" text-anchor="middle" font-family="{SANS}" font-size="12" fill="{FAIBLE}">Ordre déterministe : les équipes alternent rang par rang · un personnage vaincu ne joue plus</text>
  {fil}{coup}{"".join(jetons)}{curseur}
  {bordure(W, H)}
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  7. Architecture : un moteur, deux interfaces, des événements qui circulent
# ════════════════════════════════════════════════════════════════════

def architecture():
    W, H = 1280, 580

    def noeud(x, y, w, h, titre, detail, col, debut):
        return (f'<g>{apparition(debut, .5)}{glissement(debut, dy=14, duree=.6)}'
                f'<path d="{biseau(x, y, w, h, 10)}" fill="url(#panneau)" stroke="{col}" stroke-opacity=".55"/>'
                f'<rect x="{x}" y="{y + 10}" width="3" height="{h - 10}" fill="{col}"/>'
                f'<text x="{x + 16}" y="{y + 24}" font-family="{MONO}" font-size="13.5" font-weight="700" fill="{TEXTE}">{e(titre)}</text>'
                f'<text x="{x + 16}" y="{y + 42}" font-family="{SANS}" font-size="11" fill="{DOUX}">{e(detail)}</text></g>')

    def zone(x, y, w, h, titre, col, debut):
        return (f'<g>{apparition(debut, .6)}'
                f'<path d="{biseau(x, y, w, h, 18)}" fill="{col}" fill-opacity=".035" stroke="{col}" stroke-opacity=".3" stroke-dasharray="4 6"/>'
                f'<text x="{x + w / 2}" y="{y + 26}" text-anchor="middle" font-family="{SANS}" font-size="12.5" font-weight="800" '
                f'letter-spacing="3" fill="{col}">{titre}</text></g>')

    def flux(d, col, debut, periode, nb=3, retour=False):
        out = (f'<path d="{d}" fill="none" stroke="{col}" stroke-opacity=".35" stroke-width="1.5" pathLength="1" '
               f'stroke-dasharray="1" stroke-dashoffset="0">{trace(debut, .9)}</path>')
        for k in range(nb):
            pts = "1;0" if retour else "0;1"
            out += (f'<circle r="4" fill="{col}" filter="url(#lueur)" opacity="0">'
                    f'<animate attributeName="opacity" values="0;1;1;0" keyTimes="0;.1;.85;1" dur="{periode}s" '
                    f'begin="{debut + 1 + k * periode / nb:.2f}s" repeatCount="indefinite"/>'
                    f'<animateMotion path="{d}" dur="{periode}s" begin="{debut + 1 + k * periode / nb:.2f}s" '
                    f'repeatCount="indefinite" keyPoints="{pts}" keyTimes="0;1" calcMode="linear"/></circle>')
        return out

    zones = (zone(24, 70, 300, 360, "CONSOLE", CYAN, .2)
             + zone(390, 70, 500, 420, "MOTEUR PARTAGÉ · SANS AFFICHAGE", ROUGE, .1)
             + zone(956, 70, 300, 360, "FENÊTRE CSFML", VERT, .3))
    noeuds = (noeud(44, 112, 260, 52, "redstrings.c", "menu principal", CYAN, .6)
              + noeud(44, 184, 260, 52, "bataille_console.c", "équipes, actions, journal", CYAN, .7)
              + noeud(44, 256, 260, 52, "creer / afficher_personnage", "création, fiche colorée", CYAN, .8)
              + noeud(44, 328, 260, 52, "saisie.c · boite.c", "saisies sûres, boîtes Unicode", CYAN, .9)
              + noeud(420, 112, 440, 52, "bataille.c", "tours · Attaquer / Soigner / Défendre · garde · IA · événements", ROUGE, .5)
              + noeud(420, 184, 210, 52, "personnage.c", "catalogue des 28 classes", ROUGE, .6)
              + noeud(650, 184, 210, 52, "combat.c", "calculer_degats()", ROUGE, .65)
              + noeud(420, 256, 210, 52, "roster.c", "tableau dynamique", ROUGE, .7)
              + noeud(650, 256, 210, 52, "sauvegarde.c", "6 champs + migration", ROUGE, .75)
              + noeud(976, 112, 260, 52, "scene_bataille.c", "arène, animations", VERT, .8)
              + noeud(976, 184, 260, 52, "scene_combat.c", "composition des équipes", VERT, .9)
              + noeud(976, 256, 260, 52, "scene_creation.c", "création, hologramme", VERT, 1.0)
              + noeud(976, 328, 260, 52, "hud.c · emblemes.c", "panneaux, 20 emblèmes", VERT, 1.1)
              + noeud(420, 418, 440, 52, "lib/libmy.a", "40 fonctions maison : my_strlen, my_strcmp, my_strcpy…", OR, 1.2)
              + noeud(650, 340, 210, 52, "personnages.txt", "nom pv atq def classe soin", DOUX, 1.3)
              + noeud(420, 340, 210, 52, "tests/tests.c", "assert + ASan, sans fenêtre", VERT, 1.35))
    flux_ = (flux("M304,210 C360,210 360,138 420,138", CYAN, 1.4, 2.6)
             + flux("M420,146 C360,146 360,218 304,218", ROUGE, 1.6, 2.6)
             + flux("M976,138 C930,138 900,138 860,138", VERT, 1.5, 2.2)
             + flux("M860,146 C900,146 930,146 976,146", ROUGE, 1.7, 2.2)
             + flux("M755,308 V340", DOUX, 1.8, 1.8, 2)
             + flux("M525,340 V308", VERT, 1.9, 1.8, 2))
    legende = "".join(
        f'<circle cx="{420 + k * 190}" cy="{H - 34}" r="4.5" fill="{col}" filter="url(#lueur)"/>'
        f'<text x="{432 + k * 190}" y="{H - 30}" font-family="{SANS}" font-size="12" fill="{DOUX}">{t}</text>'
        for k, (col, t) in enumerate([(CYAN, "actions (console)"), (VERT, "actions (fenêtre)"),
                                      (ROUGE, "événements résolus")]))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="Architecture : un moteur partagé sans affichage, utilisé par la console et par la fenêtre CSFML">
  <defs>{defs_communes()}</defs>
  {cadre(W, H)}
  <text x="{W / 2}" y="40" text-anchor="middle" font-family="{SANS}" font-size="12" font-weight="700" letter-spacing="4" fill="{FAIBLE}">LES INTERFACES ENVOIENT DES ACTIONS · LE MOTEUR RENVOIE DES ÉVÉNEMENTS DÉJÀ RÉSOLUS</text>
  {zones}{flux_}{noeuds}
  {legende}
  {bordure(W, H)}
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  8. Démo du terminal : rejoue une vraie session (couleurs du terminal)
# ════════════════════════════════════════════════════════════════════

class Scenario:
    """Lignes qui apparaissent à un instant précis d'une boucle SMIL synchronisée."""

    CW, LH, FS, X0, Y0 = 9.0, 21, 15, 34, 82

    def __init__(self):
        self.defs = []
        self.scenes = []
        self.curseur = []
        self.n = 0

    @staticmethod
    def a(t):
        return f"boucle.begin+{t:.2f}s"

    def montrer(self, t, fin=None):
        s = (f'<set attributeName="opacity" to="0" begin="boucle.begin"/>'
             f'<set attributeName="opacity" to="1" begin="{self.a(t)}"/>')
        if fin is not None:
            s += f'<set attributeName="opacity" to="0" begin="{self.a(fin)}"/>'
        return s

    def x(self, col):
        return self.X0 + col * self.CW

    def y(self, row):
        return self.Y0 + row * self.LH

    def texte(self, col, row, parties, t):
        spans = "".join(f'<tspan fill="{c}"{" font-weight=\"700\"" if (r and r[0]) else ""}>{e(s)}</tspan>'
                        for s, c, *r in parties)
        return (f'<text x="{self.x(col):.1f}" y="{self.y(row):.1f}" opacity="0" font-size="{self.FS}" '
                f'xml:space="preserve">{self.montrer(t)}{spans}</text>')

    def tape(self, col, row, s, t, couleur=TEXTE, vitesse=.07):
        self.n += 1
        cid = f"t{self.n}"
        largeurs = ";".join(f"{i * self.CW:.1f}" for i in range(len(s) + 1))
        duree = max(len(s) * vitesse, .01)
        self.defs.append(
            f'<clipPath id="{cid}"><rect x="{self.x(col):.1f}" y="{self.y(row) - self.LH + 4:.1f}" width="0" height="{self.LH}">'
            f'<set attributeName="width" to="0" begin="boucle.begin"/>'
            f'<animate attributeName="width" values="{largeurs}" calcMode="discrete" dur="{duree:.2f}s" '
            f'begin="{self.a(t)}" fill="freeze"/></rect></clipPath>')
        for i in range(len(s) + 1):
            self.curseur.append((t + i * vitesse, col + i, row))
        return (f'<text x="{self.x(col):.1f}" y="{self.y(row):.1f}" clip-path="url(#{cid})" font-size="{self.FS}" '
                f'fill="{couleur}" xml:space="preserve">{e(s)}</text>'), t + duree

    def boite(self, col, row, lignes, couleur, t, separateur=None):
        x, y = self.x(col) - 4, self.y(row) - 19
        w, h = 38 * self.CW, lignes * self.LH + 12
        out = (f'<g opacity="0">{self.montrer(t)}<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="7" '
               f'fill="{couleur}" fill-opacity=".05" stroke="{couleur}" stroke-width="1.4"/>')
        if separateur is not None:
            sy = self.y(row) + (separateur - .5) * self.LH + 3
            out += f'<line x1="{x:.1f}" x2="{x + w:.1f}" y1="{sy:.1f}" y2="{sy:.1f}" stroke="{couleur}" stroke-width="1.2"/>'
        return out + "</g>"

    def scene(self, t0, t1, corps):
        self.scenes.append(f'<g opacity="0">{self.montrer(t0, t1)}{corps}</g>')


def ligne_pv(sc, row, t, rang, nom, classe, couleur, pv, pv_max, actif=False, garde=False):
    """Même mise en page que afficher_combattant() (src/ui/bataille_console.c)."""
    pleines = (pv * 12 + pv_max - 1) // pv_max if pv else 0
    barre = "■" * pleines
    vide = "□" * (12 - pleines)
    parties = [("  " + ("▶" if actif else " ") + str(rang), TEXTE, True), (" " + nom.ljust(14), TEXTE if pv else FAIBLE, True),
               (classe.ljust(15) + " ", couleur), (barre, VERT if pv else FAIBLE), (vide, FAIBLE),
               (f" {pv:>3}/{pv_max:<3}", DOUX)]
    if not pv:
        parties.append((" K.O.", ROUGE, True))
    elif garde:
        parties.append((" ◆ garde", BLEU))
    return sc.texte(0, row, parties, t)


def terminal(classes):
    c = {k["nom"]: k["couleur"] for k in classes}
    W, H = 900, 600
    sc = Scenario()
    invite = [("❯ ", ROUGE, True)]
    # ── Compilation
    b = [sc.texte(0, 0, [("~/RedStrings", CYAN, True), (" on ", FAIBLE), (" develop", "#d946ef")], 0),
         sc.texte(0, 1, invite, 0)]
    s, t = sc.tape(2, 1, "make", .5)
    b.append(s)
    lignes = [("CC", "src/personnage/bataille.c"), ("CC", "src/personnage/personnage.c"),
              ("CC", "src/personnage/sauvegarde.c"), ("CC", "src/ui/bataille_console.c"),
              ("CC", "src/redstrings.c"), ("AR", "lib/libmy.a  (40 fonctions)"), ("LD", "redstrings")]
    t += .3
    for i, (tag, f) in enumerate(lignes):
        b.append(sc.texte(0, 2 + i, [("  " + tag.ljust(7), BLEU, True), (f, TEXTE)], t + i * .15))
    t += len(lignes) * .15 + .15
    b.append(sc.texte(0, 9, [("  ✔      ", VERT, True), ("./redstrings est prêt", TEXTE)], t))
    b.append(sc.texte(0, 10, invite, t + .3))
    sc.curseur += [(0, 2, 1), (t + .3, 2, 10)]
    s, t = sc.tape(2, 10, "./redstrings", t + .6)
    b.append(s)
    fin = t + .5
    sc.scene(0, fin, "".join(b))
    # ── Menu, puis composition des équipes
    t0 = fin
    b = [sc.boite(0, 0, 8, CYAN, t0, separateur=1), sc.texte(1, 0, [("✦ Menu principal", TEXTE, True)], t0)]
    for i, (k, lab) in enumerate([("1", "Ajouter un personnage"), ("2", "Afficher un personnage"), ("3", "Combat"),
                                  ("4", "Sauvegarder"), ("5", "Charger"), ("0", "Quitter")]):
        b.append(sc.texte(1, 2 + i, [(k, TEXTE, True), ("  " + lab, DOUX)], t0 + .1 + i * .06))
    b.append(sc.texte(0, 9, [("  Choix", TEXTE, True), (" (0-5)", FAIBLE), (" › ", CYAN)], t0 + .6))
    sc.curseur.append((t0 + .6, 16, 9))
    s, t = sc.tape(16, 9, "3", t0 + 1.1)
    b.append(s)
    b.append(sc.texte(0, 11, [("  Ton équipe", TEXTE, True), (" (1 à 4, ligne vide pour terminer)", FAIBLE)], t + .3))
    t += .5
    for i, nom in enumerate(["Kaelen", "Elara", ""]):
        b.append(sc.texte(0, 12 + i, [(f"  {i + 1}.", FAIBLE), (" › ", CYAN)], t))
        sc.curseur.append((t, 8, 12 + i))
        s, t = sc.tape(8, 12 + i, nom, t + .3)
        b.append(s)
        t += .2
    b.append(sc.texte(0, 15, [("  Équipe adverse", TEXTE, True), (" (jouée par l'ordinateur)", FAIBLE)], t))
    b.append(sc.texte(0, 16, [("  1.", FAIBLE), (" › ", CYAN)], t + .2))
    sc.curseur.append((t + .2, 8, 16))
    s, t = sc.tape(8, 16, "Varek", t + .5)
    b.append(s)
    fin = t + .9
    sc.scene(t0, fin, "".join(b))
    # ── Le combat (session réelle, couleurs retirées → recolorées)
    t0 = fin
    b = []

    def plateau(row, t, varek, elara, actif_kaelen=False, garde=False, actif_varek=False):
        return [sc.texte(0, row, [("  ── Équipe adverse ──", FAIBLE)], t),
                ligne_pv(sc, row + 1, t, 1, "Varek", "Berserker", c["Berserker"], varek, 75, actif=actif_varek),
                sc.texte(0, row + 2, [("  ── Ton équipe ──", FAIBLE)], t),
                ligne_pv(sc, row + 3, t, 1, "Kaelen", "Guerrier", c["Guerrier"], 60, 60, actif=actif_kaelen, garde=garde),
                ligne_pv(sc, row + 4, t, 2, "Elara", "Prêtre", c["Prêtre"], elara, 55)]
    b += plateau(0, t0, 75, 55, actif_kaelen=True)
    b.append(sc.texte(0, 5, [("  Tour de ", TEXTE, True), ("Kaelen", c["Guerrier"], True)], t0 + .3))
    for i, a_ in enumerate(["Attaquer", "Soigner", "Défendre"]):
        b.append(sc.texte(4, 6 + i, [(str(i + 1), TEXTE, True), ("  " + a_, DOUX if i != 1 else FAIBLE)], t0 + .4 + i * .06))
    b.append(sc.texte(0, 9, [("  Action", TEXTE, True), (" (1-3)", FAIBLE), (" › ", CYAN)], t0 + .7))
    sc.curseur.append((t0 + .7, 17, 9))
    s, t = sc.tape(17, 9, "3", t0 + 1.3)
    b.append(s)
    b.append(sc.texte(0, 10, [("  » ", CYAN), ("Kaelen adopte une posture défensive.", TEXTE)], t + .4))
    b.append(sc.texte(0, 11, [("  » ", CYAN), ("Varek attaque Elara et inflige 52 dégâts.", TEXTE)], t + 1.4))
    b += plateau(13, t + 2.0, 75, 3, garde=True)
    b.append(sc.texte(0, 19, [("  … quelques tours plus tard …", FAIBLE)], t + 3.0))
    sc.curseur.append((t + 3.0, 31, 19))
    fin = t + 4.4
    sc.scene(t0, fin, "".join(b))
    t0 = fin
    b = []
    b += plateau(0, t0, 34, 0, actif_kaelen=True)
    sc.curseur.append((t0, 2, 6))
    b.append(sc.texte(0, 5, [("  Tour de ", TEXTE, True), ("Kaelen", c["Guerrier"], True)], t0 + .3))
    b.append(sc.texte(0, 6, [("  Action", TEXTE, True), (" (1-3)", FAIBLE), (" › ", CYAN)], t0 + .5))
    sc.curseur.append((t0 + .5, 17, 6))
    s, t = sc.tape(17, 6, "1", t0 + .9)
    b.append(s)
    b.append(sc.texte(0, 7, [("    1", TEXTE, True), ("  Varek          ", c["Berserker"]), ("-40 PV", ROUGE)], t + .2))
    b.append(sc.texte(0, 8, [("    0", TEXTE, True), ("  Retour", FAIBLE)], t + .25))
    b.append(sc.texte(0, 9, [("  Cible", TEXTE, True), (" (0-1)", FAIBLE), (" › ", CYAN)], t + .4))
    sc.curseur.append((t + .4, 16, 9))
    s, t = sc.tape(16, 9, "1", t + .8)
    b.append(s)
    b.append(sc.texte(0, 10, [("  » ", CYAN), ("Kaelen attaque Varek et inflige 40 dégâts.", TEXTE)], t + .4))
    b.append(sc.texte(0, 11, [("  » ", CYAN), ("Varek est vaincu.", TEXTE)], t + .9))
    b.append(sc.texte(0, 12, [("  » ", CYAN), ("L'équipe adverse a été vaincue.", TEXTE)], t + 1.3))
    b += plateau(14, t + 1.8, 0, 0)
    b.append(sc.texte(0, 20, [("  ✔ Victoire !", VERT, True)], t + 2.4))
    sc.curseur.append((t + 2.4, 15, 20))
    fin = t + 5.0
    sc.scene(t0, fin, "".join(b))
    boucle_s = round(fin + .4, 1)
    sc.curseur.sort()
    sets = "".join(f'<set attributeName="x" to="{sc.x(col):.1f}" begin="{sc.a(t)}"/>'
                   f'<set attributeName="y" to="{sc.y(row) - 14:.1f}" begin="{sc.a(t)}"/>' for t, col, row in sc.curseur)
    c0 = sc.curseur[0]
    curseur = (f'<rect x="{sc.x(c0[1]):.1f}" y="{sc.y(c0[2]) - 14:.1f}" width="{sc.CW - 1:.1f}" height="18" fill="{TEXTE}" opacity=".85">{sets}'
               f'<animate attributeName="opacity" values=".85;.85;0;0" keyTimes="0;.5;.5;1" dur="1s" repeatCount="indefinite"/></rect>')
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="Démo du terminal : compilation, composition des équipes et combat contre l'ordinateur, rejoués d'une vraie session">
  <defs>
    <linearGradient id="vitre" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#11152b"/><stop offset="1" stop-color="{FOND}"/></linearGradient>
    <filter id="ombre" x="-10%" y="-10%" width="120%" height="130%"><feGaussianBlur stdDeviation="12"/></filter>
    {"".join(sc.defs)}
  </defs>
  <rect width="0" height="0"><animate id="boucle" attributeName="x" from="0" to="0" dur="{boucle_s}s" begin="0s;boucle.end"/></rect>
  <rect x="30" y="34" width="{W - 60}" height="{H - 50}" rx="14" fill="#000" opacity=".5" filter="url(#ombre)"/>
  <rect x="12" y="8" width="{W - 24}" height="{H - 34}" rx="14" fill="url(#vitre)" stroke="#fff" stroke-opacity=".09"/>
  <path d="M12,22 a14,14 0 0 1 14,-14 h{W - 52} a14,14 0 0 1 14,14 v20 h-{W - 24} z" fill="#1a1f3a"/>
  <circle cx="38" cy="26" r="6.5" fill="#ff5f57"/><circle cx="60" cy="26" r="6.5" fill="#febc2e"/><circle cx="82" cy="26" r="6.5" fill="#28c840"/>
  <text x="{W / 2}" y="31" text-anchor="middle" font-family="{SANS}" font-size="13" fill="{DOUX}">redstrings — zsh — 90×26</text>
  <g font-family="{MONO}" fill="{TEXTE}">{"".join(sc.scenes)}{curseur}</g>
</svg>
'''


# ════════════════════════════════════════════════════════════════════
#  9. La formule des dégâts, expliquée en mouvement
# ════════════════════════════════════════════════════════════════════

GUERRIER = "#ef4444"
MAGE = "#d946ef"


def formule():
    W, H = 960, 360
    LOOP = 9
    U = 3.0                         # pixels par point
    BX, BY = 330, 168               # barre centrale
    atq, df, pv = 85, 30, 80
    deg = atq * (100 - df) // 100   # 59
    reste = pv - deg                # 21

    def at(t):
        return f"loop.begin+{t}s"

    def card(x, name, cls, col, lines):
        out = (f'<rect x="{x}" y="58" width="250" height="236" rx="18" fill="{SURFACE}" stroke="{col}" stroke-opacity=".55"/>'
               f'<rect x="{x + 22}" y="56" width="206" height="4" rx="2" fill="{col}"/>'
               f'<text x="{x + 24}" y="106" font-family="{SANS}" font-size="28" font-weight="800" fill="{TEXTE}">{name}</text>'
               f'<rect x="{x + 24}" y="120" width="{len(cls) * 10 + 22}" height="24" rx="12" fill="{col}" fill-opacity=".18"/>'
               f'<text x="{x + 35}" y="137" font-family="{SANS}" font-size="12" font-weight="700" letter-spacing="2" fill="{col}">{cls}</text>')
        return out + lines

    left = card(40, "Merlin", "MAGE", MAGE,
                f'<text x="64" y="196" font-family="{SANS}" font-size="13" letter-spacing="2" fill="{FAIBLE}">ATTAQUE</text>'
                f'<text x="64" y="252" font-family="{SANS}" font-size="56" font-weight="900" fill="{ROUGE}">{atq}</text>')
    pv_w = 200
    right = card(670, "Aragorn", "GUERRIER", GUERRIER,
                 f'<text x="694" y="182" font-family="{SANS}" font-size="13" letter-spacing="2" fill="{FAIBLE}">DÉFENSE</text>'
                 f'<text x="694" y="214" font-family="{SANS}" font-size="30" font-weight="900" fill="{BLEU}">{df}%</text>'
                 f'<text x="694" y="248" font-family="{SANS}" font-size="13" letter-spacing="2" fill="{FAIBLE}">POINTS DE VIE</text>'
                 f'<rect x="694" y="258" width="{pv_w}" height="14" rx="7" fill="#ffffff" fill-opacity=".07"/>'
                 f'<rect x="694" y="258" width="{pv_w * pv / 99:.1f}" height="14" rx="7" fill="{VERT}">'
                 f'<set attributeName="width" to="{pv_w * pv / 99:.1f}" begin="loop.begin"/>'
                 f'<animate attributeName="width" to="{pv_w * reste / 99:.1f}" dur=".9s" begin="{at(5.2)}" fill="freeze" '
                 f'calcMode="spline" keySplines=".3 0 .2 1" keyTimes="0;1"/>'
                 f'<set attributeName="fill" to="{VERT}" begin="loop.begin"/>'
                 f'<set attributeName="fill" to="{OR}" begin="{at(5.6)}"/></rect>'
                 f'<text x="894" y="250" text-anchor="end" font-family="{SANS}" font-size="20" font-weight="900" fill="{TEXTE}">'
                 f'<tspan>{pv}<set attributeName="opacity" to="1" begin="loop.begin"/><set attributeName="opacity" to="0" begin="{at(5.4)}"/></tspan></text>'
                 f'<text x="894" y="250" text-anchor="end" font-family="{SANS}" font-size="20" font-weight="900" fill="{OR}" opacity="0">{reste}'
                 f'<set attributeName="opacity" to="0" begin="loop.begin"/><set attributeName="opacity" to="1" begin="{at(5.4)}"/></text>')

    ticks = "".join(
        f'<line x1="{BX + v * U}" x2="{BX + v * U}" y1="{BY + 34}" y2="{BY + 40}" stroke="{FAIBLE}"/>'
        f'<text x="{BX + v * U}" y="{BY + 56}" text-anchor="middle" font-family="{MONO}" font-size="11" fill="{FAIBLE}">{v}</text>'
        for v in (0, 25, 50, 75, 100))
    keep_w = deg * U
    cut_w = (atq - deg) * U
    middle = f'''
  <text x="480" y="120" text-anchor="middle" font-family="{SANS}" font-size="13" letter-spacing="3" fill="{FAIBLE}">CE QUE MERLIN ENVOIE</text>
  <rect x="{BX}" y="{BY}" width="{100 * U}" height="28" rx="8" fill="#ffffff" fill-opacity=".05"/>
  {ticks}
  <g>
    <rect x="{BX}" y="{BY}" width="0" height="28" rx="8" fill="{ROUGE}">
      <set attributeName="width" to="0" begin="loop.begin"/>
      <animate attributeName="width" from="0" to="{keep_w}" dur="1s" begin="{at(.4)}" fill="freeze" calcMode="spline" keySplines=".2 .8 .2 1" keyTimes="0;1"/>
    </rect>
    <rect x="{BX + keep_w}" y="{BY}" width="0" height="28" rx="0" fill="{ROUGE}">
      <set attributeName="width" to="0" begin="loop.begin"/>
      <set attributeName="fill" to="{ROUGE}" begin="loop.begin"/>
      <set attributeName="opacity" to="1" begin="loop.begin"/>
      <animate attributeName="width" from="0" to="{cut_w}" dur=".3s" begin="{at(1.4)}" fill="freeze"/>
      <set attributeName="fill" to="{BLEU}" begin="{at(2.4)}"/>
      <animate attributeName="opacity" from="1" to="0" dur=".8s" begin="{at(3.2)}" fill="freeze"/>
      <animateTransform attributeName="transform" type="translate" values="0 0;0 0" dur=".01s" begin="loop.begin" fill="freeze"/>
      <animateTransform attributeName="transform" type="translate" from="0 0" to="30 -30" dur=".8s" begin="{at(3.2)}" fill="freeze"/>
    </rect>
  </g>
  <g opacity="0">
    <set attributeName="opacity" to="0" begin="loop.begin"/>
    <animate attributeName="opacity" from="0" to="1" dur=".4s" begin="{at(2.4)}" fill="freeze"/>
    <animate attributeName="opacity" from="1" to="0" dur=".4s" begin="{at(3.6)}" fill="freeze"/>
    <text x="{BX + keep_w + cut_w / 2}" y="{BY - 12}" text-anchor="middle" font-family="{SANS}" font-size="14" font-weight="800" fill="{BLEU}">🛡 −30 %</text>
  </g>
  <g opacity="0">
    <set attributeName="opacity" to="0" begin="loop.begin"/>
    <animate attributeName="opacity" from="0" to="1" dur=".4s" begin="{at(3.6)}" fill="freeze"/>
    <text x="{BX + keep_w / 2}" y="{BY + 20}" text-anchor="middle" font-family="{SANS}" font-size="16" font-weight="900" fill="#fff">{deg} dégâts</text>
  </g>
  <g opacity="0">
    <set attributeName="opacity" to="0" begin="loop.begin"/>
    <animate attributeName="opacity" values="0;1;1;0" keyTimes="0;.15;.7;1" dur="1.8s" begin="{at(5.1)}" fill="freeze"/>
    <text x="850" y="214" text-anchor="middle" font-family="{SANS}" font-size="40" font-weight="900" fill="{ROUGE}">−{deg}
      <animateTransform attributeName="transform" type="translate" from="0 10" to="0 -30" dur="1.8s" begin="{at(5.1)}" fill="freeze"/>
    </text>
  </g>
  <circle cx="{BX + keep_w}" cy="{BY + 14}" r="0" fill="{ROUGE}">
    <set attributeName="r" to="0" begin="loop.begin"/>
    <set attributeName="r" to="9" begin="{at(4.3)}"/>
    <set attributeName="r" to="0" begin="{at(5.1)}"/>
    <animate attributeName="cx" from="{BX + keep_w}" to="760" dur=".8s" begin="{at(4.3)}" fill="freeze" calcMode="spline" keySplines=".6 0 .9 .5" keyTimes="0;1"/>
    <animate attributeName="cy" values="{BY + 14};{BY - 40};250" dur=".8s" begin="{at(4.3)}" fill="freeze"/>
  </circle>'''

    formula_parts = [("Dégâts", TEXTE, 0), (" = ", FAIBLE, 0), ("85", ROUGE, .4), (" × (100 − ", FAIBLE, 0),
                     ("30", BLEU, 2.4), (") / 100", FAIBLE, 0), (" = ", FAIBLE, 0), ("59", OR, 3.6)]
    spans = ""
    for s, c, t in formula_parts:
        if t:
            spans += (f'<tspan fill="{c}" font-weight="900">{e(s)}'
                      f'<set attributeName="opacity" to=".25" begin="loop.begin"/>'
                      f'<set attributeName="opacity" to="1" begin="{at(t)}"/></tspan>')
        else:
            spans += f'<tspan fill="{c}">{e(s)}</tspan>'
    formula = (f'<text x="480" y="{H - 34}" text-anchor="middle" font-family="{MONO}" font-size="20">{spans}</text>')

    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-label="Animation : Merlin (attaque 85) frappe Aragorn (défense 30 %) : 59 dégâts, Aragorn passe de 80 à 21 PV">
  <rect width="0" height="0"><animate id="loop" attributeName="x" from="0" to="0" dur="{LOOP}s" begin="0s;loop.end"/></rect>
  <rect width="{W}" height="{H}" rx="24" fill="{FOND}"/>
  <rect x=".5" y=".5" width="{W - 1}" height="{H - 1}" rx="24" fill="none" stroke="#ffffff" stroke-opacity=".08"/>
  {left}
  {right}
  {middle}
  {formula}
</svg>
'''


if __name__ == "__main__":
    classes = lire_classes()
    print(f"{len(classes)} classes lues dans personnage.c")
    ecrire("banniere.svg", banniere(classes))
    ecrire("fil-rouge.svg", separateur())
    ecrire("classes.svg", catalogue(classes))
    ecrire("profils.svg", profils(classes))
    ecrire("actions.svg", actions(classes))
    ecrire("tours.svg", tours(classes))
    ecrire("architecture.svg", architecture())
    ecrire("demo-terminal.svg", terminal(classes))
    ecrire("combat.svg", formule())
