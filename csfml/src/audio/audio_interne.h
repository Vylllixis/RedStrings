/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Fonctions partagées entre les fichiers du moteur audio (src/audio/).
 */

#ifndef AUDIO_INTERNE_H
    #define AUDIO_INTERNE_H

    #include <stddef.h>
    #include <SFML/Audio.h>
    #include "gui_audio.h"

/* audio_moteur.c */
sfSoundBuffer *tampon_obtenir(audio_t *a, const pack_t *p, const char *fichier);
int audio_chemin_pack(theme_son_t t, char *dossier, size_t taille);

#endif /* AUDIO_INTERNE_H */
