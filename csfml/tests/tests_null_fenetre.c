/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_null_fenetre.c ───────────────────────────────────────────────
** │ But         Toutes les fonctions de dessin, d'interface et de scène
** │             reçoivent des NULL à la place de l'application (ou de
** │             leurs autres pointeurs) : elles refusent sans planter.
** │             Aucune fenêtre n'est ouverte ; le fichier est généré
** │             module par module, une fonction de test par fichier source.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>

#include "gui_app.h"
#include "gui_audio.h"
#include "gui_carte.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_emblemes.h"
#include "gui_hud.h"
#include "gui_scenes.h"
#include "gui_ui.h"
#include "gui_yugioh.h"
#include "../src/app/app_interne.h"
#include "../src/audio/audio_interne.h"
#include "../src/rendu/emblemes/emblemes_interne.h"
#include "../src/scenes/arene/arene_interne.h"
#include "../src/scenes/creation/creation_interne.h"
#include "../src/scenes/menu/menu_interne.h"
#include "../src/yugioh/yugioh_interne.h"
#include "lanceur.h"
#include "tests_null.h"

static void test_null_app_affichage(void)
{
    (void)ouvrir_fenetre(NULL, (affichage_t)0);
    appliquer_taille(NULL, 0, 0);
    demander_affichage(NULL, (affichage_t)0);
    appliquer_demande_affichage(NULL);
    basculer_plein_ecran(NULL);
    changer_type_plein_ecran(NULL);
}

static void test_null_app_app(void)
{
    (void)creer_app(NULL, (mode_visuel_t)0);
    detruire_app(NULL);
}

static void test_null_app_boucle(void)
{
    changer_scene(NULL, (scene_t)0);
    maj_app(NULL, 0.f);
}

static void test_null_app_evenements(void)
{
    evenements(NULL);
}

static void test_null_app_ressources(void)
{
    (void)chemin_ressource(NULL, NULL, 0);
    app_init_poussieres(NULL);
    (void)app_creer_ressources(NULL);
}

static void test_null_audio_audio_lecture(void)
{
    son(NULL, (son_t)0);
    son_secondaire(NULL, (son_t)0);
    son_regle(NULL, (son_t)0, 0.f, 0.f);
    audio_couper_voix(NULL);
}

static void test_null_audio_audio_moteur(void)
{
    (void)audio_chemin_pack((theme_son_t)0, NULL, 0);
    (void)tampon_obtenir(NULL, NULL, NULL);
    audio_init(NULL);
    audio_liberer(NULL);
    audio_changer_theme(NULL, (choix_theme_t)0);
    audio_appliquer_reglages(NULL, NULL);
}

static void test_null_audio_audio_musique(void)
{
    musique(NULL, (musique_t)0);
    musique_arreter(NULL, 0.f);
    audio_maj(NULL);
}

static void test_null_audio_audio_panneau(void)
{
    audio_raccourcis(NULL);
    (void)audio_panneau_ouvert(NULL);
    audio_panneau(NULL);
}

static void test_null_audio_audio_suivi(void)
{
    audio_entrer_scene(NULL, (scene_t)0);
    audio_suivre(NULL);
    audio_impact(NULL, NULL, 0);
}

static void test_null_rendu_cible(void)
{
    cible_primitives(NULL, NULL, 0, (sfPrimitiveType)0, NULL);
    cible_convexe(NULL, NULL, NULL);
    cible_cercle(NULL, NULL, NULL);
    cible_sprite(NULL, NULL, NULL);
    cible_texte(NULL, NULL, NULL);
}

static void test_null_rendu_ecran(void)
{
    (void)ecran_visible(NULL);
    (void)ecran_couvrant(NULL);
    (void)decal_gauche(NULL);
    (void)decal_droite(NULL);
    (void)vers_ecran(NULL, 0.f, 0.f);
    zoom_debut(NULL, (sfVector2f){0}, 0.f);
    zoom_fin(NULL);
}

static void test_null_rendu_armes(void)
{
    emb_d_epees(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_lame(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_bouclier(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_hache(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_lance(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_arc(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_cible(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
}

static void test_null_rendu_divers(void)
{
    emb_d_ancre(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_croix(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_note(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_fiole(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_engrenage(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
}

static void test_null_rendu_emblemes(void)
{
    embleme(NULL, (classe_t)0, (sfVector2f){0}, 0.f, (sfColor){0});
}

static void test_null_rendu_magie(void)
{
    emb_d_etoile(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_flamme(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_crane(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_portail(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_feuille(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_lune(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_sablier(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    emb_d_orbe(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
}

static void test_null_rendu_traits(void)
{
    emb_trait(NULL, (sfVector2f){0}, 0.f, NULL, 0.f, (sfColor){0});
    emb_epee(NULL, (sfVector2f){0}, 0.f, 0.f, (sfColor){0});
    emb_etoile(NULL, (sfVector2f){0}, 0.f, 0.f, (sfColor){0});
    emb_flamme(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
}

static void test_null_rendu_fond(void)
{
    dessiner_fond(NULL);
}

static void test_null_rendu_formes(void)
{
    rect_arrondi(NULL, (sfFloatRect){0}, 0.f, (sfColor){0});
    rect_arrondi_bord(NULL, (sfFloatRect){0}, 0.f, (sfColor){0}, (sfColor){0});
    ombre(NULL, (sfFloatRect){0}, 0.f, 0.f);
    lueur(NULL, (sfVector2f){0}, (sfVector2f){0}, (sfColor){0});
    cercle(NULL, (sfVector2f){0}, 0.f, (sfColor){0}, (sfColor){0}, 0.f);
    polygone(NULL, NULL, 0, (sfVector2f){0}, 0.f, 0.f, (sfColor){0});
    ligne_epaisse(NULL, NULL, 0, 0.f, (sfColor){0}, 0);
}

static void test_null_rendu_hud(void)
{
    panneau(NULL, (sfFloatRect){0}, 0.f, (sfColor){0}, (sfColor){0}, (sfColor){0});
    ombre_biseau(NULL, (sfFloatRect){0}, 0.f, 0.f);
    crochets(NULL, (sfFloatRect){0}, 0.f, (sfColor){0});
    losange(NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    hexagone(NULL, (sfVector2f){0}, 0.f, 0.f, (sfColor){0}, (sfColor){0});
    titre_section(NULL, NULL, NULL, (sfVector2f){0}, 0.f, (sfColor){0});
    (void)largeur_touche(NULL, NULL);
    (void)touche_clavier(NULL, NULL, (sfVector2f){0}, (sfColor){0}, 0.f);
}

static void test_null_rendu_jauges(void)
{
    arc_cercle(NULL, (sfVector2f){0}, 0.f, 0.f, (sfVector2f){0}, (sfColor){0}, 0);
    graduations(NULL, (sfVector2f){0}, 0.f, 0, 0.f, 0.f, (sfColor){0});
    ellipse(NULL, (sfVector2f){0}, (sfVector2f){0}, 0.f, (sfColor){0});
    radar_stats(NULL, (sfVector2f){0}, 0.f, NULL, (sfColor){0});
}

static void test_null_rendu_texte(void)
{
    (void)largeur_texte(NULL, (police_t)0, NULL, 0);
    (void)position_caractere(NULL, (police_t)0, NULL, 0, 0);
    (void)texte_echelle(NULL, (police_t)0, NULL, 0, (sfVector2f){0}, (sfColor){0}, (alignement_t)0, 0.f);
    (void)texte(NULL, (police_t)0, NULL, 0, (sfVector2f){0}, (sfColor){0}, (alignement_t)0);
    tronquer(NULL, (police_t)0, 0, NULL, 0.f, NULL, 0);
    (void)etiquette(NULL, NULL, (sfVector2f){0}, (sfColor){0}, (alignement_t)0);
}

static void test_null_scenes_arene_cartes(void)
{
    arene_cartes_invocation(NULL, NULL);
    arene_carte_action(NULL, NULL);
    arene_cartes_fin(NULL, NULL, 0.f);
}

static void test_null_scenes_arene_combattants(void)
{
    arene_placer(NULL, NULL);
    (void)arene_ordre_dessin(NULL, NULL);
    arene_barre_pv(NULL, (sfFloatRect){0}, NULL, 0);
    arene_dessiner_combattant(NULL, NULL, (ref_t){0}, 0);
    arene_plaque(NULL, NULL, (ref_t){0}, 0);
    arene_reticules(NULL, NULL);
}

static void test_null_scenes_arene_commandes(void)
{
    arene_clavier(NULL, NULL);
    arene_souris_cibles(NULL, NULL);
    arene_panneau_commandes(NULL, NULL, 0.f);
}

static void test_null_scenes_arene_decor(void)
{
    arene_disque(NULL, (sfVector2f){0}, (sfVector2f){0}, (sfColor){0}, (sfColor){0});
    arene_voile(NULL);
    arene_dessiner_arene(NULL, NULL);
}

static void test_null_scenes_arene_effets(void)
{
    arene_dessiner_effet(NULL, NULL);
}

static void test_null_scenes_arene_evenements(void)
{
    arene_jouer_evenements(NULL, NULL);
    arene_maj_visuels(NULL, NULL);
}

static void test_null_scenes_arene_infos(void)
{
    arene_dessiner_popups(NULL, NULL);
    arene_panneau_equipe(NULL, NULL, 0, (sfFloatRect){0});
    arene_ordre_tours(NULL, NULL);
    arene_journal(NULL, NULL, (sfVector2f){0});
    arene_banniere(NULL, NULL);
    arene_ecran_fin(NULL, NULL);
}

static void test_null_scenes_scene_arene(void)
{
    (void)arene_vis(NULL, (ref_t){0});
    arene_demarrer(NULL, NULL);
    entrer_arene(NULL);
    arene_debut_tour(NULL);
    arene_lancer(NULL, NULL, (action_t)0, (ref_t){0});
    (void)arene_echelle_monde(NULL);
    scene_arene(NULL);
}

static void test_null_scenes_creation_apercu(void)
{
    creation_apercu(NULL, NULL, 0.f);
}

static void test_null_scenes_creation_cartes(void)
{
    creation_main_cartes(NULL, NULL, 0.f);
    creation_carte_apercu(NULL, NULL);
}

static void test_null_scenes_creation_classes(void)
{
    creation_onglets(NULL, NULL, 0.f);
    creation_grille_classes(NULL, NULL, 0.f);
}

static void test_null_scenes_creation_hologramme(void)
{
    creation_suivre_classe(NULL, NULL);
    creation_hologramme(NULL, NULL);
}

static void test_null_scenes_creation_logique(void)
{
    (void)creation_raison_invalide(NULL, NULL);
    creation_creer(NULL, NULL);
    creation_clavier(NULL, NULL);
}

static void test_null_scenes_scene_creation(void)
{
    entrer_creation(NULL);
    creation_geometrie(NULL, NULL);
    scene_creation(NULL);
}

static void test_null_scenes_scene_liste(void)
{
    entrer_liste(NULL);
    scene_liste(NULL);
}

static void test_null_scenes_menu_commandes(void)
{
    (void)menu_option_principale(NULL);
    menu_commandes(NULL, 0.f);
}

static void test_null_scenes_menu_equipe(void)
{
    menu_panneau_equipe(NULL, 0.f);
}

static void test_null_scenes_menu_titre(void)
{
    menu_fil_rouge(NULL, 0.f, 0.f, 0.f);
    menu_titre(NULL, 0.f);
    menu_aide_menu(NULL, 0.f);
    menu_credit_claude(NULL, 0.f);
}

static void test_null_scenes_scene_menu(void)
{
    entrer_menu(NULL);
    menu_action(NULL, 0);
    (void)menu_touche_menu(NULL);
    (void)menu_nb_ko(NULL);
    scene_menu(NULL);
    scene_au_revoir(NULL);
}

static void test_null_scenes_scene_selection(void)
{
    entrer_selection(NULL);
    scene_selection(NULL);
}

static void test_null_ui_bouton(void)
{
    (void)survole(NULL, (sfFloatRect){0});
    (void)bouton(NULL, NULL, (sfFloatRect){0}, NULL, NULL, NULL, (sfColor){0}, 0);
    (void)bouton_plein(NULL, NULL, (sfFloatRect){0}, NULL, NULL, (sfColor){0}, 0);
    (void)bouton_retour(NULL, NULL, (sfVector2f){0});
    (void)bouton_rond(NULL, NULL, (sfVector2f){0}, NULL);
}

static void test_null_ui_carte(void)
{
    avatar(NULL, NULL, (sfVector2f){0}, 0.f, 0.f);
    barre_segments(NULL, (sfFloatRect){0}, 0.f, (sfColor){0});
    fiche(NULL, NULL, (sfFloatRect){0}, NULL);
    mini_carte(NULL, NULL, (sfFloatRect){0}, 0.f, NULL);
}

static void test_null_ui_champ(void)
{
    (void)champ_texte(NULL, NULL, (sfFloatRect){0}, NULL, 0);
}

static void test_null_ui_entete(void)
{
    en_tete_surtitre(NULL, NULL, NULL, NULL, NULL);
    en_tete(NULL, NULL, NULL, NULL);
}

static void test_null_ui_glissiere(void)
{
    (void)glissiere_bornee(NULL, NULL, (sfFloatRect){0}, NULL, NULL, NULL, (sfColor){0}, 0);
    (void)glissiere(NULL, NULL, (sfFloatRect){0}, NULL, NULL, (sfColor){0}, 0);
}

static void test_null_ui_grille(void)
{
    (void)grille_vers_ecran(NULL, (sfVector2f){0});
    (void)grille_cartes(NULL, NULL, NULL, 0, 0.f, NULL, 0);
}

static void test_null_ui_notification(void)
{
    (void)pastille(NULL, NULL, (sfVector2f){0}, (sfColor){0}, 0);
    notifier(NULL, NULL, NULL, (sfColor){0});
    dessiner_toasts(NULL);
}

static void test_null_yugioh_carte3d(void)
{
    (void)carte3d_point(NULL, 0.f, 0.f);
    (void)carte3d_face_visible(NULL);
    carte3d(NULL, (classe_t)0, NULL);
}

static void test_null_yugioh_carte3d_finitions(void)
{
    carte3d_bord_lumineux(NULL, NULL, 0.f);
    carte3d_titre(NULL, (classe_t)0, NULL);
}

static void test_null_yugioh_dos(void)
{
    dessiner_dos(NULL);
}

static void test_null_yugioh_face(void)
{
    yugioh_degrade(NULL, (sfFloatRect){0}, (sfColor){0}, (sfColor){0});
    dessiner_face(NULL, (classe_t)0);
}

static void test_null_yugioh_portrait(void)
{
    (void)yugioh_portrait(NULL, (classe_t)0, (sfVector2f){0}, 0.f);
}

static void test_null_yugioh_ressources(void)
{
    (void)yugioh_actif(NULL);
    (void)yugioh_charger(NULL);
    yugioh_liberer(NULL);
}

void tests_null_fenetre(void)
{
    LANCER(test_null_app_affichage);
    LANCER(test_null_app_app);
    LANCER(test_null_app_boucle);
    LANCER(test_null_app_evenements);
    LANCER(test_null_app_ressources);
    LANCER(test_null_audio_audio_lecture);
    LANCER(test_null_audio_audio_moteur);
    LANCER(test_null_audio_audio_musique);
    LANCER(test_null_audio_audio_panneau);
    LANCER(test_null_audio_audio_suivi);
    LANCER(test_null_rendu_cible);
    LANCER(test_null_rendu_ecran);
    LANCER(test_null_rendu_armes);
    LANCER(test_null_rendu_divers);
    LANCER(test_null_rendu_emblemes);
    LANCER(test_null_rendu_magie);
    LANCER(test_null_rendu_traits);
    LANCER(test_null_rendu_fond);
    LANCER(test_null_rendu_formes);
    LANCER(test_null_rendu_hud);
    LANCER(test_null_rendu_jauges);
    LANCER(test_null_rendu_texte);
    LANCER(test_null_scenes_arene_cartes);
    LANCER(test_null_scenes_arene_combattants);
    LANCER(test_null_scenes_arene_commandes);
    LANCER(test_null_scenes_arene_decor);
    LANCER(test_null_scenes_arene_effets);
    LANCER(test_null_scenes_arene_evenements);
    LANCER(test_null_scenes_arene_infos);
    LANCER(test_null_scenes_scene_arene);
    LANCER(test_null_scenes_creation_apercu);
    LANCER(test_null_scenes_creation_cartes);
    LANCER(test_null_scenes_creation_classes);
    LANCER(test_null_scenes_creation_hologramme);
    LANCER(test_null_scenes_creation_logique);
    LANCER(test_null_scenes_scene_creation);
    LANCER(test_null_scenes_scene_liste);
    LANCER(test_null_scenes_menu_commandes);
    LANCER(test_null_scenes_menu_equipe);
    LANCER(test_null_scenes_menu_titre);
    LANCER(test_null_scenes_scene_menu);
    LANCER(test_null_scenes_scene_selection);
    LANCER(test_null_ui_bouton);
    LANCER(test_null_ui_carte);
    LANCER(test_null_ui_champ);
    LANCER(test_null_ui_entete);
    LANCER(test_null_ui_glissiere);
    LANCER(test_null_ui_grille);
    LANCER(test_null_ui_notification);
    LANCER(test_null_yugioh_carte3d);
    LANCER(test_null_yugioh_carte3d_finitions);
    LANCER(test_null_yugioh_dos);
    LANCER(test_null_yugioh_face);
    LANCER(test_null_yugioh_portrait);
    LANCER(test_null_yugioh_ressources);
}
