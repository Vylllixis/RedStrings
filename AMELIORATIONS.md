# 🛠️ RedStrings — Suivi des améliorations

> Analyse de la version console : `includes/`, `src/`, `Makefile`.
> La partie `csfml/` n'est pas analysée ici.
>
> Chaque point marqué **🧪 Prouvé** a été reproduit en lançant vraiment le programme.
> Ce n'est donc pas une supposition.

---

## 📖 Comment lire ce document

| Priorité | Signification |
|:---:|---|
| 🔴 | **Bug réel** ou faille : à corriger en premier |
| 🟠 | **Fragile** : marche aujourd'hui, mais cassera facilement |
| 🟡 | **Qualité** : lisibilité, style, bonnes pratiques |
| 🟢 | **Bonus** : pour aller plus loin en « C avancé » |

| Difficulté | |
|:---:|---|
| ⭐ | 5 minutes, quelques lignes |
| ⭐⭐ | Une petite fonction à réécrire |
| ⭐⭐⭐ | Refonte de plusieurs fichiers |

---

## ✅ Tableau de suivi

> Coche les cases au fur et à mesure (`[ ]` → `[x]`). L'ordre proposé va du plus important au moins important.

### 🔐 Sécurité & robustesse
- [x] **S1** 🔴 ⭐⭐ Boucle infinie sur Ctrl+D (fin d'entrée) 🧪
- [x] **S2** 🔴 ⭐ `scanf` du nom jamais vérifié → nom non initialisé
- [x] **S3** 🟠 ⭐⭐ `scanf("%d")` accepte `12abc` et peut déborder 🧪
- [x] **S4** 🟠 ⭐ Deux personnages peuvent avoir le même nom 🧪
- [x] **S5** 🟡 ⭐ `sprintf` sans limite dans `repeter()`
- [x] **S6** 🟡 ⭐ Laisser gcc vérifier les formats de `boiteLigne()`

### 🗂️ Organisation des `.h` et rangement
- [x] **O1** 🔴 ⭐ Les `#include` sont **avant** le garde d'inclusion dans `my.h`
- [x] **O2** 🟠 ⭐ `my.h` inclut des bibliothèques jamais utilisées
- [x] **O3** 🟠 ⭐⭐⭐ Découper `my.h` en plusieurs headers
- [x] **O4** 🟡 ⭐ `findPersonage` n'a pas de prototype 🧪
- [x] **O5** 🟢 ⭐⭐ Ranger `src/` en sous-dossiers

### 📐 Lisibilité & conception
- [x] **L1** 🟠 ⭐ Nombres magiques (`30`, `29`, `99`, `36`)
- [x] **L2** 🟠 ⭐⭐ Une table de classes au lieu des `if / else`
- [x] **L3** 🟡 ⭐ Code dupliqué (`lireChoix` ≈ `lireEntier`)
- [x] **L4** 🟡 ⭐ Valeurs de retour qui ne servent à rien
- [x] **L5** 🟡 ⭐ `const` manquant sur `my_strcmp` / `my_strlen`
- [x] **L6** 🟡 ⭐ Conventions de nommage mélangées
- [x] **L7** 🟡 ⭐ Alignement fait avec des espaces dans les chaînes

### ⚔️ Logique du jeu
- [x] **J1** 🟠 ⭐ Un personnage peut s'attaquer lui-même 🧪
- [x] **J2** 🟡 ⭐ Un personnage mort peut encore attaquer
- [x] **J3** 🟢 ⭐ Séparer le calcul des dégâts de l'affichage

### 🧰 Makefile
- [x] **M1** 🔴 ⭐ Modifier un `.h` ne recompile rien 🧪
- [x] **M2** 🟠 ⭐ `make doc` plante (pas de `Doxyfile`) 🧪 — règles `doc` retirées en attendant le Doxyfile
- [x] **M3** 🟡 ⭐ Flags de compilation plus stricts + règle `debug`

### 🚀 Pour aller plus loin
- [ ] **B1** 🟢 ⭐⭐⭐ Allocation dynamique (`malloc` / `realloc`)
- [ ] **B2** 🟢 ⭐⭐ Sauvegarder / charger les personnages dans un fichier
- [ ] **B3** 🟢 ⭐⭐ Tests unitaires avec `assert`

---

## 👍 Ce qui est déjà bien

Avant les critiques, voici ce que tu fais déjà correctement. Garde ces réflexes.

| ✔️ | Où | Pourquoi c'est bien |
|---|---|---|
| `scanf("%29s", ...)` | `creerPersonnage.c` | Tu limites la taille lue, donc pas de débordement du tableau `name[30]` |
| `viderBuffer()` après chaque `scanf` | partout | Tu as compris le problème du `\n` qui reste dans le buffer |
| `vsnprintf(texte, sizeof(texte), ...)` | `boite.c` | `snprintf` + `sizeof` : c'est exactement la bonne façon de faire |
| `static` sur les fonctions internes | `afficherQuestion`, `repeter`... | Ces fonctions restent privées à leur fichier |
| `const personnage_t *p` | `afficherPersonnage` | Le compilateur garantit que l'affichage ne modifie pas le personnage |
| PV et dégâts ramenés à 0 | `attaquer.c` | Pas de PV négatifs |
| Gardes d'inclusion `#ifndef` | tous les `.h` | Évite les doubles définitions |
| Une fonction par fichier, fonctions courtes | `src/` | Facile à lire et à relire |

---

# 🔐 Partie 1 : Sécurité & robustesse

## S1 🔴 Boucle infinie sur Ctrl+D 🧪

**📍 Où :** `lireChoix()` dans [src/redstrings.c](src/redstrings.c#L36-L48) et `lireEntier()` dans [src/creerPersonnage.c](src/creerPersonnage.c#L14-L30)

**🧪 Preuve :** j'ai lancé `./redstrings < /dev/null` (comme si l'utilisateur appuyait sur Ctrl+D). En **2 secondes**, le programme a affiché **26 millions de lignes** d'erreur, et il ne s'arrête jamais.

**🤔 Pourquoi ?** `scanf` ne renvoie pas que `0` ou `1` :

```
scanf("%d", &x) renvoie :
   1   → un nombre a été lu          ✅
   0   → l'utilisateur a tapé "abc"  ❌ on redemande
  EOF  → l'entrée est FERMÉE (-1)    ❌ on redemande... et on n'aura JAMAIS de réponse
```

```
         ┌──────────────────────┐
         │ scanf("%d") → EOF    │◄─────┐
         └──────────┬───────────┘      │
                    │ != 1             │
         ┌──────────▼───────────┐      │
         │ viderBuffer()        │      │  ♾️ pour toujours
         │ afficherErreur(...)  │──────┘
         └──────────────────────┘
```

**✅ Correction :** distinguer le cas `EOF` et le remonter à l'appelant.

```c
// AVANT
while (scanf("%d", &choix) != 1) {
    viderBuffer();
    afficherErreur("Tape un nombre entre 0 et 3");
    ...
}

// APRÈS
int ret = scanf("%d", &choix);

while (ret != 1) {
    if (ret == EOF)
        return 0;           // entrée fermée → on choisit "Quitter"
    viderBuffer();
    afficherErreur("Tape un nombre entre 0 et 3");
    ...
    ret = scanf("%d", &choix);
}
```

> 💡 La vraie solution propre, c'est **S3** (`fgets` + `strtol`), qui corrige S1, S2 et S3 d'un coup.

---

## S2 🔴 Le `scanf` du nom n'est jamais vérifié

**📍 Où :** [src/creerPersonnage.c:43](src/creerPersonnage.c#L43) et [src/redstrings.c:56](src/redstrings.c#L56)

```c
scanf("%29s", p->name);      // ← et si ça échoue ?
```

**🤔 Le problème :** si la lecture échoue (Ctrl+D), `p->name` n'est **jamais rempli**. Le tableau `tabPersonnage` est une variable locale non initialisée, donc `name` contient **n'importe quoi** (des octets aléatoires, peut-être sans `\0` final). Ensuite `printf("%s", p->name)` lit la mémoire jusqu'à tomber par hasard sur un `\0` : c'est un **comportement indéfini**.

**✅ Correction :**

```c
if (scanf("%29s", p->name) != 1)
    return -1;               // création annulée
```

et dans `menuAjouter`, n'incrémenter `len` que si la création a réussi :

```c
if (creerPersonnage(&tab[*len]) == 0)
    (*len)++;
```

> 🧠 **Règle d'or :** *toute* fonction de lecture (`scanf`, `fgets`, `fopen`, `malloc`...) peut échouer. **On vérifie toujours sa valeur de retour.**

---

## S3 🟠 `scanf("%d")` est trop permissif 🧪

**🧪 Preuve :** à la question « Points de vie », j'ai tapé `12abc`. Le personnage a été créé avec **12 PV** sans aucune erreur.

| Saisie | Ce que fait `scanf("%d")` | Ce qu'on voudrait |
|---|---|---|
| `42` | ✅ 42 | ✅ 42 |
| `abc` | ❌ renvoie 0 | ❌ erreur |
| `12abc` | ⚠️ lit 12 et ignore `abc` | ❌ erreur |
| `99999999999` | 💥 **comportement indéfini** (dépasse un `int`) | ❌ erreur |

**✅ Correction : lire la ligne entière, puis la convertir avec `strtol`.**

C'est la méthode recommandée en C : `fgets` lit une ligne **avec une taille maximale**, et `strtol` dit **exactement** où la conversion s'est arrêtée.

```c
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Lit une ligne. Renvoie 0 si OK, -1 si l'entrée est fermée (Ctrl+D). */
int lireLigne(char *buf, size_t taille)
{
    size_t len = 0;

    if (fgets(buf, (int)taille, stdin) == NULL)
        return -1;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';         // on retire le '\n'
    else
        viderBuffer();               // ligne trop longue : on jette la fin
    return 0;
}

/* Renvoie 0 et remplit *resultat, ou -1 si l'entrée est fermée. */
int lireEntier(const char *question, int min, int max, int *resultat)
{
    char buf[32];
    char *fin = NULL;
    long valeur = 0;

    while (1) {
        afficherQuestion(question, min, max);
        if (lireLigne(buf, sizeof(buf)) == -1)
            return -1;
        errno = 0;
        valeur = strtol(buf, &fin, 10);
        if (fin != buf && *fin == '\0' && errno == 0
            && valeur >= min && valeur <= max) {
            *resultat = (int)valeur;
            return 0;
        }
        afficherErreur("Valeur invalide");
    }
}
```

```
strtol("12abc", &fin, 10)

   buf → [1][2][a][b][c][\0]
               ▲
              fin      *fin == 'a' ≠ '\0'  →  ❌ refusé
```

> 💡 Avec `lireLigne`, tu peux aussi lire le **nom** du personnage, et `lireChoix` devient simplement `lireEntier("Choix", 0, 3, &choix)` (voir **L3**).

---

## S4 🟠 Deux personnages peuvent porter le même nom 🧪

**📍 Où :** [src/creerPersonnage.c](src/creerPersonnage.c) et le commentaire de [includes/struct.h:16](includes/struct.h#L16) : *« sert d'identifiant »*

**🧪 Preuve :** j'ai créé deux « Bob ». `findPersonage` renvoie toujours le **premier** : le second Bob est **inaccessible** pour toujours.

**✅ Correction :** un identifiant doit être **unique**. Dans `menuAjouter` (qui a accès au tableau), on vérifie avec `findPersonage` avant d'accepter le nom :

```c
if (findPersonage(tab, *len, nom) != -1) {
    afficherErreur("Ce nom est déjà pris");
    return;
}
```

> Pour ça, il faut séparer « lire le nom » de « remplir les stats », ou passer le tableau à `creerPersonnage`.

---

## S5 🟡 `sprintf` sans limite dans `repeter()`

**📍 Où :** [src/afficherPersonnage.c:10-17](src/afficherPersonnage.c#L10-L17)

```c
char pleine[64];
repeter(pleine, "█", remplies);   // "█" = 3 octets en UTF-8 !
```

**Le calcul aujourd'hui :**

```
valeur max = 99  →  remplies = (99 + 4) / 5 = 20
20 × 3 octets + 1 ('\0') = 61 octets  ≤  64   ✅ ça passe... de justesse
```

**Le jour où tu autorises 120 PV :**

```
remplies = 24  →  24 × 3 + 1 = 73 octets  >  64   💥 débordement de tampon
```

**✅ Correction :** borner `remplies` **et** passer la taille du tampon.

```c
static void repeter(char *dest, size_t taille, const char *motif, int fois)
{
    size_t n = 0;

    dest[0] = '\0';
    for (int i = 0; i < fois && n < taille; i++)
        n += (size_t)snprintf(dest + n, taille - n, "%s", motif);
}

// dans afficherStat :
if (remplies > LARGEUR_BARRE)
    remplies = LARGEUR_BARRE;
repeter(pleine, sizeof(pleine), "█", remplies);
```

> 🧠 **Règle :** `sprintf` → `snprintf`, `strcpy` → `strncpy`/`snprintf`, `gets` → `fgets`. Toujours la version qui connaît la taille.

---

## S6 🟡 Laisser gcc vérifier les formats de `boiteLigne()`

`printf("%d", "texte")` déclenche un warning, mais `boiteLigne(GRIS, "%d", "texte")` **n'en déclenche pas** : gcc ne sait pas que ta fonction marche comme `printf`.

**✅ Une ligne suffit dans le header :**

```c
void boiteLigne(const char *couleur, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
//                         ▲      ▲  ▲
//                         │      │  └ les arguments variables commencent au 3e paramètre
//                         │      └ le format est le 2e paramètre
//                         └ vérifier comme printf
```

Avec `-Werror`, une erreur de format devient alors une **erreur de compilation**, au lieu d'un affichage faux en pleine partie.

---

# 🗂️ Partie 2 : Organisation des `.h` (ce que ton prof appelle « fractionner »)

## O1 🔴 Les `#include` sont en dehors du garde

**📍 Où :** [includes/my.h:6-13](includes/my.h#L6-L13)

```c
// AVANT ❌                          // APRÈS ✅
#include <unistd.h>                  #ifndef MY_H
#include <stdio.h>                       #define MY_H
#include "struct.h"
#include "couleurs.h"                    #include <stdio.h>
                                         #include "struct.h"
#ifndef MY_H                             #include "couleurs.h"
    #define MY_H                         ...
    ...                              #endif
```

**🤔 Pourquoi c'est important :** le garde `#ifndef` sert à ce que le fichier soit lu **une seule fois**. Tout ce qui est **au-dessus** du `#ifndef` est relu à **chaque** inclusion. Ça marche ici parce que `struct.h` a son propre garde, mais c'est un piège : le garde doit **toujours** être la toute première chose du fichier.

---

## O2 🟠 Des bibliothèques incluses pour rien

| Header | Utilisé ? | Action |
|---|:---:|---|
| `<unistd.h>` | ❌ aucune fonction (`write`, `read`...) utilisée | supprimer |
| `<stdlib.h>` | ❌ ni `malloc`, ni `exit`, ni `strtol` (pour l'instant) | à ne mettre **que** dans les `.c` qui en ont besoin |
| `<stdio.h>` | ✅ `printf`, `scanf` | garder, mais dans les `.c` concernés |
| `<stdarg.h>` | ✅ dans `boite.c` uniquement | 👍 déjà bien placé |

---

## O3 🟠 Découper `my.h`

**🤔 Le problème :** `my.h` est un header « fourre-tout ». Chaque `.c` inclut **tout**, même ce dont il n'a pas besoin. Conséquences :
- impossible de savoir d'un coup d'œil de quoi dépend un fichier ;
- modifier un seul prototype concerne **tous** les fichiers ;
- `my_strcmp.c` connaît les personnages, les couleurs et les boîtes... alors qu'il compare juste deux chaînes.

**✅ Le principe :** **un header = un thème**, et **chaque `.c` inclut seulement ce qu'il utilise.**

```
includes/                        AVANT                    APRÈS
                           ┌─────────────┐
                           │    my.h     │      ┌──────────────┐  personnage_t, Classe_t,
                           │  (tout est  │ ───► │ personnage.h │  NOM_MAX, STAT_MAX,
                           │   dedans)   │      └──────────────┘  creer / afficher / attaquer
                           └─────────────┘      ┌──────────────┐
                                           ───► │   boite.h    │  boiteHaut / Separateur /
                                                └──────────────┘  Bas / Ligne, LARGEUR_BOITE
                                                ┌──────────────┐
                                           ───► │   saisie.h   │  viderBuffer, lireLigne,
                                                └──────────────┘  lireEntier
                                                ┌──────────────┐
                                           ───► │ my_string.h  │  my_strlen, my_strcmp
                                                └──────────────┘
                                                ┌──────────────┐
                                           ───► │ redstrings.h │  redstrings()
                                                └──────────────┘
                           ┌─────────────┐      ┌──────────────┐
                           │ couleurs.h  │ ───► │  couleurs.h  │  (inchangé, il est déjà bien)
                           └─────────────┘      └──────────────┘
```

**Qui inclut quoi, après découpage :**

| Fichier `.c` | `personnage.h` | `boite.h` | `saisie.h` | `my_string.h` | `couleurs.h` |
|---|:---:|:---:|:---:|:---:|:---:|
| `main.c` | | | | | *(juste `redstrings.h`)* |
| `redstrings.c` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `creerPersonnage.c` | ✅ | ✅ | ✅ | | ✅ |
| `afficherPersonnage.c` | ✅ | ✅ | | | ✅ |
| `attaquer.c` | ✅ | ✅ | | | ✅ |
| `boite.c` | | ✅ | | | ✅ |
| `viderBuffer.c` / `saisie.c` | | | ✅ | | |
| `my_strcmp.c`, `my_strlen.c` | | | | ✅ | |

> Un header doit **se suffire à lui-même** : s'il utilise `size_t`, il inclut `<stddef.h>`. Test simple : un `.c` qui contient **uniquement** `#include "boite.h"` doit compiler.

> ⚠️ **Attention à la version CSFML :** `csfml/include/gui.h` fait `#include "struct.h"`. Si tu renommes `struct.h` en `personnage.h`, mets aussi à jour cette ligne. Autre option : garder un `struct.h` qui ne contient que les types.

---

## O4 🟡 `findPersonage` n'a pas de prototype 🧪

**📍 Où :** [src/redstrings.c:10](src/redstrings.c#L10)

Compilé avec `-Wmissing-prototypes` :
```
src/redstrings.c:10:5: warning: no previous prototype for 'findPersonage'
```

Elle n'est utilisée que dans ce fichier, donc → **`static`**. (Petite faute au passage : *Personage* → *Personnage*.)

```c
static int findPersonnage(const personnage_t *tab, int size, const char *name)
```

---

## O5 🟢 Ranger `src/` en sous-dossiers

```
src/
├── main.c
├── redstrings.c              ← boucle du menu
├── personnage/
│   ├── creerPersonnage.c
│   ├── afficherPersonnage.c
│   └── attaquer.c
├── ui/
│   ├── boite.c
│   └── saisie.c              ← viderBuffer + lireLigne + lireEntier
└── utils/
    ├── my_strcmp.c
    └── my_strlen.c
```

✨ Bonne nouvelle : ton Makefile utilise `find ./src -name '*.c'`, il trouve **déjà** les sous-dossiers. Rien à changer côté Makefile principal.

> ⚠️ **Attention :** `csfml/Makefile` (lignes 17-18) compile `../src/my_strcmp.c` et `../src/my_strlen.c` avec ces chemins exacts. Si tu les déplaces dans `src/utils/`, corrige ces deux lignes.

---

# 📐 Partie 3 : Lisibilité & conception

## L1 🟠 Nombres magiques

Un « nombre magique », c'est un nombre écrit en dur dont on ne connaît pas l'origine. Le danger : **la même information est écrite à plusieurs endroits**, et le jour où tu en changes un seul, tout casse.

| Nombre | Où | Lié à |
|---|---|---|
| `30` | `struct.h` → `name[30]` | ... |
| `30` | `redstrings.c` → `char nom[30]` | ⚠️ doit rester égal au précédent |
| `29` | `"%29s"` × 2 | ⚠️ doit valoir 30 − 1 |
| `0, 99` | `lireEntier(..., 0, 99)` × 3 | bornes des stats |
| `36` | `LARGEUR_BOITE` dans `boite.c` | ⚠️ doit être égal au nombre de `─` dans `LIGNE` (`couleurs.h`) |

**✅ Correction :**

```c
// personnage.h
#define NOM_MAX   30
#define STAT_MIN  0
#define STAT_MAX  99

typedef struct {
    char name[NOM_MAX];
    ...
} personnage_t;
```

Pour `"%29s"`, il existe une astuce de préprocesseur (en bonus) :

```c
#define STR_(x) #x
#define STR(x)  STR_(x)
#define NOM_LONGUEUR 29                        // NOM_MAX - 1
scanf("%" STR(NOM_LONGUEUR) "s", p->name);     // devient "%29s"
```

> Et si tu passes à `fgets` (S3), le problème disparaît : `lireLigne(p->name, sizeof(p->name))`.

Pour `LARGEUR_BOITE` / `LIGNE` : mets les deux **côte à côte** dans `boite.h`, avec un commentaire qui dit qu'ils sont liés.

---

## L2 🟠 Une table de classes au lieu des `if / else`

**Aujourd'hui, l'information « Mage = magenta » est écrite à 3 endroits :**

```
struct.h            → commentaire  /**< Affiché en magenta. */
afficherPersonnage  → if (p->classe == MAGE) { ... MAGENTA; }
creerPersonnage     → printf(... MAGENTA "Mage" ...)
```

Ajouter une classe « Voleur », c'est modifier **3 fichiers** et ne pas en oublier un.

**✅ Avec une table (programmation « dirigée par les données ») :**

```c
// personnage.h
typedef enum {
    GUERRIER,
    MAGE,
    ARCHER,
    NB_CLASSES          // ← vaut automatiquement 3
} classe_t;

typedef struct {
    const char *nom;
    const char *couleur;
} infoClasse_t;

extern const infoClasse_t CLASSES[NB_CLASSES];
```

```c
// personnage.c
const infoClasse_t CLASSES[NB_CLASSES] = {
    [GUERRIER] = { "Guerrier", ROUGE   },
    [MAGE]     = { "Mage",     MAGENTA },
    [ARCHER]   = { "Archer",   VERT    },
};
```

```c
// afficherPersonnage : 10 lignes → 1 ligne
const infoClasse_t *info = &CLASSES[p->classe];

// creerPersonnage : une boucle au lieu de 3 printf
for (int i = 0; i < NB_CLASSES; i++)
    printf("    " GRAS "%d" RESET "  %s%s" RESET "\n",
        i + 1, CLASSES[i].couleur, CLASSES[i].nom);
p->classe = lireEntier("Choix", 1, NB_CLASSES) - 1;
```

**Ajouter « Voleur » = 2 lignes**, et le menu se met à jour tout seul. 🎉

---

## L3 🟡 Code dupliqué

**1.** `lireChoix()` et `lireEntier()` font la même chose : afficher une question, lire un entier, recommencer si c'est faux. → `lireChoix()` peut disparaître :

```c
choix = lireEntier("Choix", 0, 3);
```

(Il faut alors déplacer `lireEntier` dans `saisie.c` et l'enlever du `static` de `creerPersonnage.c`.)

**2.** Dans `lireEntier`, le bloc `afficherQuestion` + `scanf` + `viderBuffer` est écrit **deux fois** (avant la boucle et dedans). Une boucle `do ... while` est faite pour ça :

```c
do {
    afficherQuestion(question, min, max);
    ok = scanf("%d", &valeur);
    viderBuffer();
} while (ok != 1 || valeur < min || valeur > max);
```

---

## L4 🟡 Valeurs de retour qui ne servent à rien

`creerPersonnage()` et `afficherPersonnage()` renvoient `int`, mais **toujours `0`**, et personne ne lit ce résultat.

Deux options :
- ✅ **la rendre utile** : `creerPersonnage` renvoie `-1` si la saisie a échoué (lien avec S1/S2) ;
- ✅ **ou la supprimer** : `void afficherPersonnage(...)`.

> Une valeur de retour, c'est une promesse au lecteur : « regarde-moi, je peux t'apprendre quelque chose ». Si elle vaut toujours 0, c'est une fausse promesse.

---

## L5 🟡 `const` manquant

```c
int my_strcmp(char *str1, char *str2);   // ❌ laisse croire qu'elle modifie les chaînes
int my_strlen(char *str);                // ❌ idem

int my_strcmp(const char *str1, const char *str2);   // ✅
int my_strlen(const char *str);                      // ✅
```

Sans `const`, tu ne peux pas appeler `my_strcmp(p->name, ...)` quand `p` est un `const personnage_t *` : gcc affiche un warning, et avec ton `-Werror` la compilation échoue. Le `const` dit « je lis seulement », et le compilateur le vérifie pour toi.

**Autres remarques :**
- `my_strcmp` renvoie `1` en cas de différence. Le vrai `strcmp` renvoie `< 0`, `0` ou `> 0` (pour trier). Si tu veux juste tester l'égalité, un nom comme `my_strequal` serait plus honnête. Sinon, renvoie `str1[i] - str2[i]`.
- `my_strlen` n'est pas utilisée par la version console (elle l'est par la CSFML, donc **ne la supprime pas**).
- Une longueur ne peut pas être négative : le type habituel est `size_t`.

---

## L6 🟡 Conventions de nommage mélangées

| Élément | Exemples actuels | Problème |
|---|---|---|
| Fonctions | `creerPersonnage` (camelCase) / `my_strlen` (snake_case) / `findPersonage` (anglais) | 3 styles |
| Types | `personnage_t` / `Classe_t` | majuscule ou pas ? |
| Champs | `name` (anglais) / `pv`, `atk`, `def` | mélange anglais / français |
| Variables | `nameClasse` | moitié anglais, moitié français |

**✅ Choisis une règle et applique-la partout**, par exemple :
- fonctions en `camelCase` français : `creerPersonnage`, `trouverPersonnage` ;
- types en minuscules + `_t` : `personnage_t`, `classe_t` ;
- macros en MAJUSCULES : `NOM_MAX`.

Ce n'est pas « plus juste », c'est **plus prévisible** : on n'a pas à se demander comment s'appelle une fonction.

---

## L7 🟡 Alignement avec des espaces dans les chaînes

```c
lireEntier("Points de vie", 0, 99);
lireEntier("Attaque      ", 0, 99);   // ← espaces ajoutés à la main pour aligner
lireEntier("Défense      ", 0, 99);
```

Si tu renommes « Attaque » en « Force », il faut recompter les espaces. `printf` sait aligner tout seul :

```c
printf("  " GRAS "%-13s" RESET ..., question);
//                 ▲
//                 └ "-13" = aligné à gauche sur 13 caractères
```

> ⚠️ Petit piège : `%-13s` compte les **octets**, pas les caractères. `"Défense"` contient un `é` (2 octets en UTF-8), donc il faudra une colonne de plus. Ta fonction `largeurVisible()` de `boite.c` résout exactement ce problème : bien vu de l'avoir écrite !

---

# ⚔️ Partie 4 : Logique du jeu

## J1 🟠 Un personnage peut s'attaquer lui-même 🧪

**🧪 Preuve :** Attaquant = `Bob`, Défenseur = `Bob` → `⚔ Bob attaque Bob`, `PV restants 0`.

```c
// menuCombat, après avoir trouvé idDef :
if (idAtk == idDef) {
    afficherErreur("Un personnage ne peut pas s'attaquer lui-même");
    return;
}
```

## J2 🟡 Un mort peut encore attaquer

Un personnage à `0 PV` peut toujours être choisi comme attaquant (et se faire attaquer de nouveau).

```c
if (tab[idAtk].pv == 0) {
    afficherErreur("Ce personnage est mort");
    return;
}
```

## J3 🟢 Séparer le calcul de l'affichage

Tes marqueurs `// [calcul_degats]` montrent que tu vois déjà ces lignes comme un bloc à part. Fais-en une vraie fonction :

```c
int calculerDegats(const personnage_t *attaquant, const personnage_t *cible)
{
    int degats = attaquant->atk * (100 - cible->def) / 100;

    return degats < 0 ? 0 : degats;
}
```

**Avantages :**
- une fonction **pure** (pas de `printf`, pas de modification) → testable facilement (voir **B3**) ;
- la version CSFML pourrait la **réutiliser** au lieu de recopier la formule.

---

# 🧰 Partie 5 : Makefile

## M1 🔴 Modifier un `.h` ne recompile rien 🧪

**🧪 Preuve :** après `make`, j'ai fait `touch includes/struct.h`, puis `make` :
```
make: Rien à faire pour « all ».
```

**🤔 Pourquoi c'est grave :** imagine que tu ajoutes un champ `int vitesse;` dans `personnage_t`. Seuls les `.c` modifiés depuis seront recompilés. Les autres `.o` gardent **l'ancienne taille** de la structure. Résultat : des bugs mémoire incompréhensibles, qui disparaissent après un `make re`.

```
 struct.h modifié ──X──► make ne le sait pas
                         │
 attaquer.o (ancien)     │   pense que personnage_t fait 48 octets
 creerPersonnage.o (neuf)    pense que personnage_t fait 52 octets   💥
```

**✅ Correction : gcc peut générer les dépendances pour toi.**

```makefile
CPPFLAGS	=	-I./includes -MMD -MP
DEPS	=	$(OBJS:.o=.d)

# tout en bas du Makefile :
-include $(DEPS)
```

- `-MMD` : pour chaque `.o`, gcc écrit un `.d` avec la liste des `.h` utilisés ;
- `-MP` : évite une erreur si tu supprimes un `.h` ;
- `-include` : charge ces fichiers s'ils existent (le `-` ignore leur absence au premier build).

Pense à ajouter `$(DEPS)` dans la règle `clean`. Ton `.gitignore` ignore déjà les `*.d` 👍

---

## M2 🟠 `make doc` plante 🧪

La règle appelle `doxygen Doxyfile`, mais **le fichier `Doxyfile` n'existe pas** → erreur.

```sh
doxygen -g Doxyfile     # génère un Doxyfile par défaut
```

Puis, dans le `Doxyfile` : `INPUT = src includes`, `RECURSIVE = YES`, `OUTPUT_DIRECTORY = docs`, `OUTPUT_LANGUAGE = French`.

> Tes `/**< ... */` sont bien du Doxygen 👍 Il manque des blocs `/** @brief ... @param ... @return ... */` au-dessus des **fonctions** dans les headers.

---

## M3 🟡 Des flags plus stricts

```makefile
# AVANT
CFLAGS	=	-W -Wall -Werror

# APRÈS
CFLAGS	=	-std=c11 -Wall -Wextra -Werror -pedantic -Wshadow -Wmissing-prototypes
```

| Flag | Ce qu'il apporte |
|---|---|
| `-Wextra` | c'est le nouveau nom de `-W` (même chose, en plus lisible) |
| `-std=c11` | fixe la version du C, le code compile pareil partout |
| `-pedantic` | refuse les extensions non standard de gcc |
| `-Wshadow` | signale une variable qui en cache une autre du même nom |
| `-Wmissing-prototypes` | aurait trouvé **O4** tout seul |

**Une règle `debug` pour traquer les bugs mémoire :**

```makefile
debug:	CFLAGS += -g3 -fsanitize=address,undefined
debug:	LDFLAGS += -fsanitize=address,undefined
debug:	re
```

(Il faut alors utiliser `$(LDFLAGS)` dans la règle d'édition de liens : `$(CC) -o $(NAME) $(OBJS) $(LDFLAGS)`.)

Avec `make debug`, le moindre débordement de tampon (comme **S5**) arrête le programme avec la ligne exacte du problème. Indispensable en « C avancé ».

---

# 🏎️ Partie 6 : Et l'optimisation ?

Pour être honnête : **la performance n'est pas un problème dans ce programme.** Il attend l'utilisateur 99,99 % du temps. Une « optimisation » qui rend le code moins lisible serait ici une **mauvaise** idée.

Quelques petites choses à savoir quand même :

| Où | Actuel | Mieux | Gain |
|---|---|---|---|
| `boiteLigne` | `printf(" ")` dans une boucle | `printf("%*s", n, "")` | un seul appel au lieu de 36 |
| `afficherPersonnage` | chaîne de `if` | table `CLASSES[]` (**L2**) | accès direct + plus lisible |
| `findPersonage` | parcours linéaire | ✅ parfait pour 10 éléments | rien à faire |

🧠 **Le saviez-vous ?** `sizeof(personnage_t)` vaut **48** et non 46 (30 + 4 × 4). Le compilateur ajoute **2 octets de remplissage** (*padding*) après `name[30]` pour que les `int` soient alignés sur 4 octets. Ce n'est pas un problème ici, mais c'est une question classique d'examen.

```
 octet: 0 ............ 29 30 31 32  36  40  44  48
        [   name[30]    ][pad][pv][atk][def][cls]
                          ▲
                          └ 2 octets perdus pour l'alignement
```

---

# 🚀 Partie 7 : Pour aller plus loin

## B1 🟢 Allocation dynamique

Aujourd'hui : `personnage_t tabPersonnage[10];`, donc 10 personnages maximum, à jamais.

```c
typedef struct {
    personnage_t *data;
    int len;
    int capacite;
} roster_t;

int ajouter(roster_t *r, const personnage_t *p)
{
    if (r->len == r->capacite) {
        int nouvelle = r->capacite == 0 ? 4 : r->capacite * 2;
        personnage_t *tmp = realloc(r->data, (size_t)nouvelle * sizeof(*tmp));

        if (tmp == NULL)
            return -1;          // ⚠️ ne jamais écrire r->data = realloc(r->data, ...)
        r->data = tmp;
        r->capacite = nouvelle;
    }
    r->data[r->len++] = *p;
    return 0;
}
```

N'oublie pas le `free(r->data)` à la fin, puis vérifie avec `valgrind ./redstrings` qu'il n'y a **aucune fuite**.

## B2 🟢 Sauvegarde dans un fichier

Option « 4 : Sauvegarder » / « 5 : Charger » avec `fopen`, `fprintf`, `fscanf`, `fclose`. Un format texte simple :

```
Bob 50 30 20 0
Alice 40 45 10 1
```

> Ça fait travailler toutes les vérifications vues en Partie 1 : `fopen` peut renvoyer `NULL`, `fscanf` peut échouer, le fichier peut être corrompu...

## B3 🟢 Tests unitaires avec `assert`

```c
#include <assert.h>

int main(void)
{
    personnage_t a = { "A", 50, 40, 0, GUERRIER };
    personnage_t b = { "B", 50, 40, 50, MAGE };

    assert(calculerDegats(&a, &b) == 20);   // 40 × (100 - 50) / 100
    assert(my_strcmp("abc", "abc") == 0);
    assert(my_strcmp("abc", "abd") != 0);
    assert(my_strcmp("ab", "abc") != 0);
    return 0;
}
```

Une règle `make tests` qui compile ce fichier, et tu vérifies en 1 seconde que tes modifications n'ont rien cassé.

---

# 🗺️ Ordre conseillé

```
 Étape 1 ─ Corriger les bugs (1 h)
 │   M1  dépendances Makefile      ← en premier, sinon tu vas compiler des .o périmés
 │   O1  garde d'inclusion
 │   S1 + S2 + S3  saisie sûre (lireLigne + lireEntier avec strtol)
 │   J1 + S4  auto-attaque, noms en double
 ▼
 Étape 2 ─ Ranger (1 h)
 │   O2 + O3  découper my.h
 │   O4 + O5  static, sous-dossiers  (⚠️ mettre à jour csfml/)
 │   L1  constantes nommées
 ▼
 Étape 3 ─ Embellir (1 h)
 │   L2  table de classes
 │   L3 → L7  doublons, const, nommage
 │   M2 + M3  doc, flags, make debug
 ▼
 Étape 4 ─ Bonus « C avancé »
     J3 + B3  calculerDegats + tests
     B1  malloc / realloc
     B2  sauvegarde fichier
```

> 💡 **Conseil :** fais **un commit par point** (ex. `fix(S1): gérer EOF dans lireChoix`). Ton prof verra ta progression dans l'historique Git, et tu pourras revenir en arrière facilement si une modification casse quelque chose.

---

## 🧪 Annexe : comment j'ai testé

Tu peux refaire ces tests toi-même :

```sh
# S1 : boucle infinie (Ctrl+C pour arrêter)
./redstrings < /dev/null

# S3 : "12abc" accepté comme 12
printf '1\nBob\n12abc\n5\n5\n1\n2\nBob\n0\n' | ./redstrings

# J1 + S4 : deux Bob, puis Bob attaque Bob
printf '1\nBob\n50\n50\n0\n1\n1\nBob\n10\n10\n10\n2\n3\nBob\nBob\n0\n' | ./redstrings

# O4 : warnings stricts
gcc -std=c11 -Wall -Wextra -pedantic -Wshadow -Wmissing-prototypes -Iincludes src/*.c

# M1 : le header modifié n'est pas pris en compte
make && touch includes/struct.h && make
```
