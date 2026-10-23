/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Événements sonores, packs de sons et réglages audio.
 *
 * Ce header ne dépend pas de SFML : tout ce qu'il déclare se teste sans
 * carte son. Les écrans ne parlent qu'en événements (SON_CARTE_SELECTION…) ;
 * le pack du thème actif dit quel fichier jouer pour chacun.
 */

#ifndef GUI_SONS_H
    #define GUI_SONS_H

    #include <stddef.h>

    #define SON_VARIANTES_MAX 4     /**< fichiers différents pour un même événement */
    #define SON_FICHIER_MAX 64      /**< longueur max d'un nom de fichier du manifeste */
    #define SON_CHEMIN_MAX 512      /**< dossier du pack + nom de fichier */
    #define VOLUME_MAX 100

/** Ce qui s'est passé, du point de vue du joueur. Le nom textuel est dans audio_evenements.c. */
typedef enum {
    /* Interface */
    SON_UI_SURVOL,
    SON_UI_VALIDER,
    SON_UI_DEPLACEMENT,
    SON_UI_OUVERTURE,
    SON_UI_FERMETURE,
    SON_UI_ONGLET,
    SON_UI_SAISIE,
    SON_UI_EFFACEMENT,
    SON_UI_REFUS,
    SON_UI_VALEUR,
    SON_UI_ERREUR,
    SON_UI_SUCCES,
    SON_UI_INFO,
    SON_ECRAN_TRANSITION,
    SON_ECRAN_RETOUR,
    SON_APP_OUVERTURE,
    SON_APP_FERMETURE,
    SON_SECRET_KONAMI,
    /* Cartes et données */
    SON_CARTE_SELECTION,
    SON_CARTE_RETRAIT,
    SON_CARTE_OUVERTURE,
    SON_CARTE_DEFILEMENT,
    SON_CLASSE_CHANGEMENT,
    SON_EQUIPE_VALIDEE,
    SON_PERSONNAGE_CREE,
    SON_RECHERCHE_VIDE,
    SON_SAUVEGARDE_OK,
    SON_CHARGEMENT_OK,
    /* Combat */
    SON_COMBAT_DEBUT,
    SON_COMBAT_TOUR,
    SON_COMBAT_ACTION,
    SON_COMBAT_IMPACT,
    SON_COMBAT_IMPACT_GARDE,
    SON_COMBAT_IMPACT_LOURD,
    SON_COMBAT_SOIN,
    SON_COMBAT_GARDE,
    SON_COMBAT_GARDE_FIN,
    SON_COMBAT_KO,
    SON_COMBAT_VICTOIRE,
    SON_COMBAT_DEFAITE,
    NB_SONS
} son_t;

typedef enum {
    MUSIQUE_MENU,
    MUSIQUE_COMBAT,
    NB_MUSIQUES
} musique_t;

/** Identités sonores livrées. THEME_STANDARD sert aussi de repli. */
typedef enum {
    THEME_STANDARD,
    THEME_YUGIOH,
    NB_THEMES
} theme_son_t;

/** Choix du joueur : AUTO suit le mode visuel (« Yu-Gi-Oh! » → thème Yu-Gi-Oh!). */
typedef enum {
    CHOIX_THEME_AUTO,
    CHOIX_THEME_STANDARD,
    CHOIX_THEME_YUGIOH,
    NB_CHOIX_THEMES
} choix_theme_t;

/** Règles d'un événement, communes à tous les thèmes. */
typedef struct {
    const char *nom;    /**< « carte.selection » : clé du manifeste */
    int priorite;       /**< 1 discret, 2 normal, 3 important (baisse la musique) */
    float delai;        /**< secondes minimum entre deux déclenchements */
    int max;            /**< lectures simultanées au plus */
    float hauteur;      /**< variation aléatoire de hauteur, ± (anti-lassitude) */
} info_son_t;

/** Ce qu'un pack associe à un événement. nb == 0 : non défini (on se replie). */
typedef struct {
    int nb;
    char fichiers[SON_VARIANTES_MAX][SON_FICHIER_MAX];
    char rare[SON_FICHIER_MAX];     /**< variante rare, "" si aucune */
    float proba_rare;
    float volume;                   /**< 0..1 */
    float hauteur;                  /**< < 0 : celle de info_son_t */
} entree_son_t;

typedef struct {
    char fichier[SON_FICHIER_MAX];  /**< "" : non défini */
    float volume;
} entree_musique_t;

/** Contenu d'un manifeste sons.txt. */
typedef struct {
    char dossier[SON_CHEMIN_MAX - SON_FICHIER_MAX - 2];
    entree_son_t sons[NB_SONS];
    entree_musique_t musiques[NB_MUSIQUES];
} pack_t;

/** Réglages du joueur, conservés dans un petit fichier texte. */
typedef struct {
    int general;        /**< 0..100 */
    int musique;
    int effets;
    int coupe;          /**< 1 : silence total */
    choix_theme_t theme;
} reglages_audio_t;

/** État d'une voix de lecture, vu par les règles (sans SFML). */
typedef struct {
    int active;
    int priorite;
    float debut;
} voix_info_t;

/* audio_evenements.c */
const info_son_t *son_info(son_t s);
int son_depuis_nom(const char *nom);
int musique_depuis_nom(const char *nom);
const char *nom_musique(musique_t m);
const char *nom_theme(theme_son_t t);
const char *dossier_theme(theme_son_t t);

/* audio_manifeste.c */
void pack_vider(pack_t *p, const char *dossier);
int pack_lire_ligne(pack_t *p, const char *ligne, char *erreur, size_t taille);
int pack_charger(pack_t *p, const char *dossier, const char *manifeste, char *erreur,
    size_t taille);
int pack_chemin(const pack_t *p, const char *fichier, char *chemin, size_t taille);

/* audio_regles.c */
float hasard(unsigned int *graine);
const entree_son_t *son_resoudre(const pack_t *theme, const pack_t *standard, son_t s,
    const pack_t **source);
int variante_choisir(const entree_son_t *e, int precedente, unsigned int *graine,
    int *rare);
float volume_effectif(const reglages_audio_t *r, int musique, float gain);
int son_autorise(son_t s, float derniere, int actives, float maintenant);
int voix_choisir(const voix_info_t *voix, int n, int priorite);
theme_son_t theme_effectif(choix_theme_t choix, int mode_yugioh);

/* audio_reglages.c */
void reglages_defaut(reglages_audio_t *r);
int reglages_lire(reglages_audio_t *r, const char *chemin);
int reglages_ecrire(const reglages_audio_t *r, const char *chemin);
const char *nom_choix_theme(choix_theme_t c);

#endif /* GUI_SONS_H */
