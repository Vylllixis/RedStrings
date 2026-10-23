/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ wav_vers_ogg.c ─────────────────────────────────────────────────────
** │ But         Convertir les WAV produits par generer_sons.py en OGG
** │             Vorbis, avec l'encodeur intégré à SFML : aucun autre outil
** │             à installer.
** │ Usage       wav_vers_ogg entree1.wav sortie1.ogg [entree2.wav sortie2.ogg…]
** │ Code retour 0 si tout est converti, 1 sinon
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <SFML/Audio.h>

int main(int argc, char **argv)
{
    int erreurs = 0;
    sfSoundBuffer *b = NULL;

    if (argc < 3 || argc % 2 == 0) {
        fprintf(stderr, "usage : %s entree.wav sortie.ogg [...]\n", argv[0]);
        return 1;
    }
    for (int i = 1; i + 1 < argc; i += 2) {
        b = sfSoundBuffer_createFromFile(argv[i]);
        if (b == NULL || !sfSoundBuffer_saveToFile(b, argv[i + 1])) {
            fprintf(stderr, "échec : %s -> %s\n", argv[i], argv[i + 1]);
            erreurs++;
        }
        if (b != NULL)
            sfSoundBuffer_destroy(b);
    }
    return erreurs != 0;
}
