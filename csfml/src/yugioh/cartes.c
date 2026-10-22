/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ cartes.c ───────────────────────────────────────────────────────────
** │ But         Correspondance entre chaque classe et une carte de
** │             monstre Yu-Gi-Oh! : le choix suit le rôle de la classe
** │             (description, catégorie, statistiques), jamais seulement
** │             l'illustration ; chaque carte n'est utilisée qu'une fois.
** │ Données     cadre, attribut et niveau relevés sur db.ygoprodeck.com
** │ Contenu     carte_de_classe, couleur_cadre, encre_cadre, fond_titre,
** │             nom_cadre, couleur_attribut, nom_attribut,
** │             symbole_attribut
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "personnage.h"
#include "gui_anim.h"
#include "gui_config.h"
#include "gui_yugioh.h"

static const carte_classe_t CARTES[NB_CLASSES] = {
    /* Corps à corps */
    [GUERRIER] = {"Gilford the Legend", 69933858, CADRE_EFFET, ATTR_TERRE, 8},
    [PALADIN] = {"Paladin of White Dragon", 73398797, CADRE_RITUEL, ATTR_LUMIERE, 4},
    [BERSERKER] = {"Beast King Barbaros", 78651105, CADRE_EFFET, ATTR_TERRE, 8},
    [CHEVALIER_NOIR] = {"Dark Blade", 11321183, CADRE_NORMAL, ATTR_TENEBRES, 4},
    [SAMOURAI] = {"Legendary Six Samurai - Shi En", 29981921, CADRE_SYNCHRO,
        ATTR_TENEBRES, 5},
    /* Duelliste : l'épéiste du premier duelliste de la série, lame en avant. */
    [DUELLISTE] = {"Flame Swordsman", 45231177, CADRE_FUSION, ATTR_FEU, 5},
    /* Lancier : chevalier monté, deux lances, « allonge et charge brutale ». */
    [LANCIER] = {"Gaia The Fierce Knight", 6368038, CADRE_NORMAL, ATTR_TERRE, 7},
    [GLADIATEUR] = {"Gladiator Beast Laquari", 78868776, CADRE_EFFET, ATTR_FEU, 4},
    /* Magie */
    [MAGE] = {"Dark Magician", 46986414, CADRE_NORMAL, ATTR_TENEBRES, 7},
    [ARCHIMAGE] = {"Endymion, the Master Magician", 40732515, CADRE_EFFET,
        ATTR_TENEBRES, 7},
    /* Élémentaliste : la magicienne qui réunit les six attributs. */
    [ELEMENTALISTE] = {"Elemental Mistress Doriado", 99414168, CADRE_RITUEL,
        ATTR_LUMIERE, 3},
    [NECROMANCIEN] = {"Zombie Master", 17259470, CADRE_EFFET, ATTR_TENEBRES, 4},
    [INVOCATEUR] = {"Aleister the Invoker", 86120751, CADRE_EFFET, ATTR_TENEBRES, 4},
    /* Druide : seule carte « Druid » qui soit un druide humanoïde, soigneur. */
    [DRUIDE] = {"Aurkus, Lightsworn Druid", 7183277, CADRE_EFFET, ATTR_LUMIERE, 3},
    [ILLUSIONNISTE] = {"Illusionist Faceless Mage", 28546905, CADRE_NORMAL,
        ATTR_TENEBRES, 5},
    [CHRONOMANCIEN] = {"Time Wizard", 71625222, CADRE_EFFET, ATTR_LUMIERE, 2},
    /* Distance */
    [ARCHER] = {"Arcane Archer of the Forest", 55001420, CADRE_EFFET, ATTR_TERRE, 3},
    /* Chasseur : archère des terres sauvages, distincte de l'archer de la forêt. */
    [CHASSEUR] = {"Amazoness Archer", 91869203, CADRE_EFFET, ATTR_TERRE, 4},
    [TIREUR_ELITE] = {"Machina Sniper", 23782705, CADRE_EFFET, ATTR_TERRE, 4},
    /* Assassin : silhouette encapuchonnée qui frappe de nuit, lame à la main. */
    [ASSASSIN] = {"Night Assailant", 16226786, CADRE_EFFET, ATTR_TENEBRES, 3},
    /* Ombrelame : chevalier spectral à l'épée brisée, tout en ombre. */
    [OMBRELAME] = {"The Phantom Knights of Break Sword", 62709239, CADRE_XYZ,
        ATTR_TENEBRES, 3},
    /* Corsaire : pas de capitaine pirate jouable ; flotte de la Plunder Patroll. */
    [CORSAIRE] = {"Blackeyes, the Plunder Patroll Seaguide", 91642007, CADRE_EFFET,
        ATTR_EAU, 4},
    /* Soutien */
    [PRETRE] = {"St. Joan", 21175632, CADRE_FUSION, ATTR_LUMIERE, 7},
    /* Barde : diva qui chante pour renforcer ses alliés. */
    [BARDE] = {"Aria the Melodious Diva", 40502912, CADRE_EFFET, ATTR_LUMIERE, 4},
    [ALCHIMISTE] = {"Alchemist of Black Spells", 78121572, CADRE_EFFET, ATTR_VENT, 4},
    [INGENIEUR] = {"Ancient Gear Engineer", 1953925, CADRE_EFFET, ATTR_TERRE, 5},
    [GARDIEN] = {"Big Shield Gardna", 65240384, CADRE_EFFET, ATTR_TERRE, 4},
    [MOINE] = {"Master Monk", 49814180, CADRE_EFFET, ATTR_TERRE, 5},
};

static const sfColor CADRES[NB_CADRES][2] = {
    [CADRE_NORMAL] = {{214, 175, 100, 255}, {160, 118, 52, 255}},
    [CADRE_EFFET] = {{206, 110, 52, 255}, {139, 63, 22, 255}},
    [CADRE_RITUEL] = {{98, 146, 220, 255}, {44, 86, 164, 255}},
    [CADRE_FUSION] = {{160, 104, 204, 255}, {88, 50, 142, 255}},
    [CADRE_SYNCHRO] = {{238, 238, 244, 255}, {176, 180, 196, 255}},
    [CADRE_XYZ] = {{48, 48, 56, 255}, {14, 14, 20, 255}},
};

static const char *NOMS_CADRES[NB_CADRES] = {
    "Normal", "Effet", "Rituel", "Fusion", "Synchro", "Xyz"
};

static const sfColor ATTRIBUTS[NB_ATTRIBUTS] = {
    [ATTR_TENEBRES] = {112, 52, 156, 255},
    [ATTR_LUMIERE] = {232, 196, 58, 255},
    [ATTR_TERRE] = {128, 92, 52, 255},
    [ATTR_EAU] = {46, 134, 193, 255},
    [ATTR_FEU] = {214, 56, 30, 255},
    [ATTR_VENT] = {46, 170, 100, 255},
};

static const char *NOMS_ATTRIBUTS[NB_ATTRIBUTS] = {
    "Ténèbres", "Lumière", "Terre", "Eau", "Feu", "Vent"
};

/* Glyphes de la police DejaVu, à la place des idéogrammes des vraies cartes. */
static const char *SYMBOLES[NB_ATTRIBUTS] = {"☾", "☼", "▲", "≈", "✹", "≋"};

const carte_classe_t *carte_de_classe(classe_t c)
{
    return &CARTES[(unsigned)c < NB_CLASSES ? c : 0];
}

sfColor couleur_cadre(cadre_carte_t c, int bas)
{
    return CADRES[(unsigned)c < NB_CADRES ? c : 0][bas ? 1 : 0];
}

/** Couleur du texte sur le cadre : blanc sur le noir des Xyz, noir ailleurs. */
sfColor encre_cadre(cadre_carte_t c)
{
    return c == CADRE_XYZ ? (sfColor){240, 236, 226, 255} : (sfColor){18, 14, 10, 255};
}

/** Fond de la zone du titre : le cadre éclairci, ou gris foncé pour les Xyz. */
sfColor fond_titre(cadre_carte_t c)
{
    if (c == CADRE_XYZ)
        return (sfColor){58, 58, 66, 255};
    return melanger(couleur_cadre(c, 0), C_BLANC, 0.22f);
}

const char *nom_cadre(cadre_carte_t c)
{
    return NOMS_CADRES[(unsigned)c < NB_CADRES ? c : 0];
}

sfColor couleur_attribut(attribut_carte_t a)
{
    return ATTRIBUTS[(unsigned)a < NB_ATTRIBUTS ? a : 0];
}

const char *nom_attribut(attribut_carte_t a)
{
    return NOMS_ATTRIBUTS[(unsigned)a < NB_ATTRIBUTS ? a : 0];
}

const char *symbole_attribut(attribut_carte_t a)
{
    return SYMBOLES[(unsigned)a < NB_ATTRIBUTS ? a : 0];
}
