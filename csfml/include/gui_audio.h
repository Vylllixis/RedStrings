/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Moteur audio de la version graphique (CSFML Audio).
 *
 * Les écrans appellent son(app, SON_…) ou musique(app, MUSIQUE_…) : ils ne
 * connaissent aucun fichier. Le moteur choisit le fichier dans le pack du
 * thème actif (repli : pack standard, puis silence), gère les voix, le rythme
 * des lectures, les fondus de musique et les réglages du joueur.
 * Sans carte son, tout reste silencieux et le jeu fonctionne normalement.
 */

#ifndef GUI_AUDIO_H
    #define GUI_AUDIO_H

    #include <SFML/Audio.h>
    #include "bataille.h"
    #include "gui_sons.h"
    #include "gui_types.h"

    #define NB_VOIX 16              /**< sons joués en même temps, au plus */
    #define MAX_TAMPONS 200         /**< fichiers de bruitage gardés en mémoire */
    #define FICHIER_REGLAGES_AUDIO "reglages_audio.txt"
    #define MANIFESTE_AUDIO "sons.txt"

/** Une voix de lecture : un sfSound réutilisé d'un bruitage à l'autre. */
typedef struct {
    sfSound *son;
    int id;             /**< son_t joué, -1 si la voix n'a jamais servi */
    int priorite;
    float debut;
} voix_t;

/** Un fichier décodé une fois, réutilisé à chaque lecture. */
typedef struct {
    char chemin[SON_CHEMIN_MAX];
    sfSoundBuffer *tampon;  /**< NULL si le chargement a échoué (signalé une fois) */
} tampon_t;

/** Une musique en cours ; `niveau` monte ou descend pour les fondus. */
typedef struct {
    sfMusic *flux;
    int piste;          /**< musique_t, -1 si aucune */
    int theme;
    float niveau;       /**< 0..1 */
    float vitesse;      /**< variation de niveau par seconde (+ entrée, - sortie) */
    float gain;         /**< volume du manifeste */
} piste_t;

/** Ce que les écrans montraient à l'image précédente (audio_suivi.c). */
typedef struct {
    int scene;
    int menu_selection;
    int konami;
    int nb_personnages;
    int sel_etape;
    int sel_nb[NB_EQUIPES];
    int sel_curseur;
    int sel_part;
    int liste_ouvert;
    int liste_trouves;
    int crea_classe;
    int crea_focus;
    int crea_valeurs[NB_STATS];
    int arene_etat;
    int arene_action;
    int arene_cible;
} audio_suivi_t;

/** Panneau des réglages audio (F2). */
typedef struct {
    int ouvert;
    int ligne;              /**< ligne choisie au clavier */
    float t;
    float apparition;       /**< 0..1 */
    glissiere_t glissieres[3];
    bouton_t boutons[NB_CHOIX_THEMES + 2];
    int valeurs[3];         /**< copie éditée des volumes */
} panneau_audio_t;

/** État complet du moteur audio, rangé dans l'application (app->audio). */
typedef struct {
    int pret;                   /**< 0 : pas d'audio, tous les appels sont sans effet */
    int trace;                  /**< REDSTRINGS_AUDIO_TRACE=1 : chaque décision sur stderr */
    reglages_audio_t reglages;
    theme_son_t theme;
    pack_t standard;
    pack_t pack;                /**< pack du thème actif */
    voix_t voix[NB_VOIX];
    tampon_t tampons[MAX_TAMPONS];
    int nb_tampons;
    float derniere[NB_SONS];    /**< instant de la dernière lecture de chaque son */
    int precedente[NB_SONS];    /**< dernière variante jouée */
    unsigned int graine;
    float temps;
    int important;              /**< un son de priorité >= 2 a sonné pendant l'image */
    piste_t pistes[2];          /**< [0] musique courante, [1] musique qui s'éteint */
    unsigned int musiques_signalees;    /**< musiques manquantes déjà signalées (1 bit chacune) */
    float attenuation;          /**< musique baissée pendant un son important */
    float fin_attenuation;
    audio_suivi_t suivi;
    panneau_audio_t panneau;
} audio_t;

/** @name Création, thème et réglages (audio_moteur.c) */
/**@{*/
/**
 * Prépare l'audio une fois, après la fenêtre : réglages du fichier, voix,
 * packs du thème choisi, bruitages décodés d'avance. Sans carte son, l'audio
 * reste désactivé (`pret` = 0) et tous les appels sont sans effet.
 */
void audio_init(app_t *app);
/** Arrête et libère tout ; peut être appelée même si audio_init a échoué. */
void audio_liberer(app_t *app);
/** Change d'identité sonore à chaud (et l'enregistre) ; la musique suit en fondu. */
void audio_changer_theme(app_t *app, choix_theme_t choix);
/** Applique de nouveaux réglages (la coupure est immédiate) et les enregistre. */
void audio_appliquer_reglages(app_t *app, const reglages_audio_t *r);
/**@}*/

/** @name Bruitages (audio_lecture.c) */
/**@{*/
/**
 * Joue l'événement `s` selon le thème actif. Peut ne rien jouer : son coupé,
 * aucun pack ne le définit, délai minimum non écoulé, ou aucune voix libérable.
 */
void son(app_t *app, son_t s);
/** Comme son(), sauf si un son de priorité >= 2 a déjà sonné pendant l'image. */
void son_secondaire(app_t *app, son_t s);
/**
 * Comme son(), avec une hauteur imposée (`hauteur` multiplie la hauteur, 1 =
 * normale) et une position (`pan` de -1 à gauche à 1 à droite, 0 au centre).
 */
void son_regle(app_t *app, son_t s, float hauteur, float pan);
/** Arrête tous les bruitages en cours (la musique n'est pas touchée). */
void audio_couper_voix(app_t *app);
/**@}*/

/** @name Musique (audio_musique.c) */
/**@{*/
/** Fait jouer `m` en boucle, en fondu enchaîné ; rien si elle joue déjà. */
void musique(app_t *app, musique_t m);
/** La musique courante s'éteint en `duree` secondes. */
void musique_arreter(app_t *app, float duree);
/** Une fois par image : horloge, atténuation sous les sons importants, fondus, volumes. */
void audio_maj(app_t *app);
/**@}*/

/** @name Sons des écrans (audio_suivi.c) */
/**@{*/
/** Nouvel écran : la musique suit, et le premier état observé ne fait pas de bruit. */
void audio_entrer_scene(app_t *app, scene_t s);
/** Après le dessin de l'écran : chaque changement d'état devient un événement. */
void audio_suivre(app_t *app);
/** Son de l'impact d'un événement du moteur, du côté de la cible. */
void audio_impact(app_t *app, const evenement_t *ev, int pv_max);
/**@}*/

/** @name Réglages à l'écran (audio_panneau.c) */
/**@{*/
/** F2 ouvre ou ferme le panneau, F3 coupe ou rétablit le son ; ces touches n'atteignent pas l'écran. */
void audio_raccourcis(app_t *app);
int audio_panneau_ouvert(const app_t *app);
/** Dessine le panneau par-dessus l'écran (vue fixe) et applique les réglages. */
void audio_panneau(app_t *app);
/**@}*/

#endif /* GUI_AUDIO_H */
