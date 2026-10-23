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

/* audio_moteur.c : création, thème et réglages */
void audio_init(app_t *app);
void audio_liberer(app_t *app);
void audio_changer_theme(app_t *app, choix_theme_t choix);
void audio_appliquer_reglages(app_t *app, const reglages_audio_t *r);

/* audio_lecture.c : bruitages */
void son(app_t *app, son_t s);
void son_secondaire(app_t *app, son_t s);
void son_regle(app_t *app, son_t s, float hauteur, float pan);
void audio_couper_voix(app_t *app);

/* audio_musique.c : musique et fondus */
void musique(app_t *app, musique_t m);
void musique_arreter(app_t *app, float duree);
void audio_maj(app_t *app);

/* audio_suivi.c : les changements d'état des écrans deviennent des sons */
void audio_entrer_scene(app_t *app, scene_t s);
void audio_suivre(app_t *app);
void audio_impact(app_t *app, const evenement_t *ev, int pv_max);

/* audio_panneau.c : réglages à l'écran (F2) et coupure rapide (F3) */
void audio_raccourcis(app_t *app);
int audio_panneau_ouvert(const app_t *app);
void audio_panneau(app_t *app);

#endif /* GUI_AUDIO_H */
