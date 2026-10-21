/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "my_string.h"
#include "personnage.h"

/*
 * Le catalogue complet. Une ligne par classe :
 *   nom, description, catégorie, couleur {r, g, b}, {PV, ATQ, DEF}, emblème.
 * Les modificateurs ont une somme nulle : une classe gagne d'un côté ce
 * qu'elle perd de l'autre. Les couleurs sont choisies pour rester
 * distinctes deux à deux et lisibles sur le fond sombre de l'interface.
 */
const info_classe_t classes[NB_CLASSES] = {
    [GUERRIER] = {"Guerrier", "Combattant polyvalent",
        CAT_CORPS_A_CORPS, {239, 68, 68}, {0, 0, 0}, EMB_EPEES},
    [PALADIN] = {"Paladin", "Rempart sacré en armure lourde",
        CAT_CORPS_A_CORPS, {238, 229, 39}, {10, -15, 5}, EMB_BOUCLIER},
    [BERSERKER] = {"Berserker", "Rage aveugle, frappe sans garde",
        CAT_CORPS_A_CORPS, {203, 40, 91}, {5, 15, -20}, EMB_HACHE},
    [CHEVALIER_NOIR] = {"Chevalier noir", "Lame maudite, armure d'ombre",
        CAT_CORPS_A_CORPS, {134, 87, 241}, {-10, 5, 5}, EMB_LAME},
    [SAMOURAI] = {"Samouraï", "Maître du sabre, coup décisif",
        CAT_CORPS_A_CORPS, {251, 110, 15}, {-5, 10, -5}, EMB_LAME},
    [DUELLISTE] = {"Duelliste", "Escrime vive et précise",
        CAT_CORPS_A_CORPS, {228, 127, 133}, {-10, 10, 0}, EMB_EPEES},
    [LANCIER] = {"Lancier", "Allonge et charge brutale",
        CAT_CORPS_A_CORPS, {237, 243, 252}, {0, 5, -5}, EMB_LANCE},
    [GLADIATEUR] = {"Gladiateur", "Champion de l'arène",
        CAT_CORPS_A_CORPS, {161, 119, 75}, {10, 5, -15}, EMB_BOUCLIER},
    [MAGE] = {"Mage", "Arcanes offensives",
        CAT_MAGIE, {217, 70, 239}, {-10, 15, -5}, EMB_ETOILE},
    [ARCHIMAGE] = {"Archimage", "Maîtrise absolue des arcanes",
        CAT_MAGIE, {41, 100, 216}, {-15, 20, -5}, EMB_ETOILE},
    [ELEMENTALISTE] = {"Élémentaliste", "Feu, glace et foudre",
        CAT_MAGIE, {22, 216, 244}, {-5, 15, -10}, EMB_FLAMME},
    [NECROMANCIEN] = {"Nécromancien", "Draine la vie des vivants",
        CAT_MAGIE, {154, 251, 95}, {5, 5, -10}, EMB_CRANE},
    [INVOCATEUR] = {"Invocateur", "Appelle des esprits alliés",
        CAT_MAGIE, {153, 150, 235}, {5, 0, -5}, EMB_PORTAIL},
    [DRUIDE] = {"Druide", "Force de la nature",
        CAT_MAGIE, {68, 134, 42}, {5, -5, 0}, EMB_FEUILLE},
    [ILLUSIONNISTE] = {"Illusionniste", "Trompe l'œil et l'esprit",
        CAT_MAGIE, {250, 179, 236}, {-5, 0, 5}, EMB_LUNE},
    [CHRONOMANCIEN] = {"Chronomancien", "Plie le cours du temps",
        CAT_MAGIE, {151, 183, 222}, {-10, 5, 5}, EMB_SABLIER},
    [ARCHER] = {"Archer", "Tir rapide à distance",
        CAT_DISTANCE, {34, 197, 94}, {-5, 10, -5}, EMB_ARC},
    [CHASSEUR] = {"Chasseur", "Pisteur des terres sauvages",
        CAT_DISTANCE, {145, 146, 45}, {0, 10, -10}, EMB_ARC},
    [TIREUR_ELITE] = {"Tireur d'élite", "Un tir, une cible",
        CAT_DISTANCE, {221, 227, 124}, {-15, 20, -5}, EMB_CIBLE},
    [ASSASSIN] = {"Assassin", "Frappe dans l'ombre",
        CAT_DISTANCE, {120, 114, 121}, {-20, 20, 0}, EMB_LAME},
    [OMBRELAME] = {"Ombrelame", "Lame tissée d'ombre",
        CAT_DISTANCE, {166, 22, 254}, {-5, 5, 0}, EMB_LUNE},
    [CORSAIRE] = {"Corsaire", "Pirate des mers lointaines",
        CAT_DISTANCE, {7, 170, 250}, {10, 0, -10}, EMB_ANCRE},
    [PRETRE] = {"Prêtre", "Foi inébranlable",
        CAT_SOUTIEN, {239, 220, 174}, {15, -15, 0}, EMB_CROIX},
    [BARDE] = {"Barde", "Chants qui galvanisent",
        CAT_SOUTIEN, {197, 82, 156}, {0, -5, 5}, EMB_NOTE},
    [ALCHIMISTE] = {"Alchimiste", "Potions et mixtures",
        CAT_SOUTIEN, {113, 244, 217}, {10, -5, -5}, EMB_FIOLE},
    [INGENIEUR] = {"Ingénieur", "Machines et tourelles",
        CAT_SOUTIEN, {244, 178, 105}, {-5, -5, 10}, EMB_ENGRENAGE},
    [GARDIEN] = {"Gardien", "Mur vivant, défense ultime",
        CAT_SOUTIEN, {50, 113, 158}, {5, -20, 15}, EMB_BOUCLIER},
    [MOINE] = {"Moine", "Corps et esprit en harmonie",
        CAT_SOUTIEN, {240, 177, 29}, {5, 10, -15}, EMB_ORBE},
};

static const char *NOMS_CATEGORIES[NB_CATEGORIES] = {
    [CAT_CORPS_A_CORPS] = "Corps à corps",
    [CAT_MAGIE] = "Magie",
    [CAT_DISTANCE] = "Distance",
    [CAT_SOUTIEN] = "Soutien",
};

const char *nom_categorie(categorie_t c)
{
    return NOMS_CATEGORIES[c];
}

/** Séquence construite une fois à partir de la couleur RGB de la table. */
const char *couleur_ansi(classe_t c)
{
    static char sequences[NB_CLASSES][24];

    if (sequences[c][0] == '\0')
        snprintf(sequences[c], sizeof(sequences[c]), "\033[38;2;%d;%d;%dm",
            classes[c].couleur.r, classes[c].couleur.g, classes[c].couleur.b);
    return sequences[c];
}

int classe_valide(int id)
{
    return id >= 0 && id < NB_CLASSES;
}

void ordre_classes(classe_t ordre[NB_CLASSES])
{
    int n = 0;

    for (int cat = 0; cat < NB_CATEGORIES; cat++) {
        for (int i = 0; i < NB_CLASSES; i++) {
            if (classes[i].categorie == (categorie_t)cat)
                ordre[n++] = (classe_t)i;
        }
    }
}

int stat_finale(classe_t c, stat_t s, int base)
{
    int v = base + classes[c].mods[s];

    if (v < STAT_MIN)
        return STAT_MIN;
    return v > STAT_MAX ? STAT_MAX : v;
}

void appliquer_classe(personnage_t *p)
{
    p->pv = stat_finale(p->classe, STAT_PV, p->pv);
    p->atq = stat_finale(p->classe, STAT_ATQ, p->atq);
    p->def = stat_finale(p->classe, STAT_DEF, p->def);
}

int trouver_personnage(const personnage_t *tab, int size, const char *nom)
{
    for (int i = 0; i < size; i++) {
        if (my_strcmp(tab[i].nom, nom) == 0)
            return i;
    }
    return -1;
}
