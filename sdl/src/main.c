/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ main.c ─────────────────────────────────────────────────────────────
** │ But         Premier pas avec SDL2 : ouvre une fenêtre et y affiche un
** │             carré rouge que l'on déplace avec les flèches.
** │             Échap ou la croix de la fenêtre pour quitter.
** │ Contenu     main
** │ Code retour 0 si tout va bien, 84 en cas d'erreur
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <SDL2/SDL.h>
#include <stdio.h>

#define LARGEUR 800
#define HAUTEUR 600
#define TAILLE_CARRE 80
#define VITESSE 5 // pixels par image

/*
** ╭─ main ───────────────────────────────────────────────────────────────
** │ 1. SDL_Init : démarre SDL (partie vidéo)
** │ 2. SDL_CreateWindow : la fenêtre ; SDL_CreateRenderer : le « pinceau »
** │    qui dessine dedans
** │ 3. boucle de jeu, une fois par image :
** │      a. SDL_PollEvent : lit les événements (fermeture, touche Échap)
** │      b. SDL_GetKeyboardState : flèches maintenues = on bouge le carré
** │      c. on efface en gris foncé, on dessine le carré, SDL_RenderPresent
** │         montre le résultat à l'écran
** │      d. SDL_Delay(16) : environ 60 images par seconde
** │ 4. on détruit tout dans l'ordre inverse de la création
** ╰──────────────────────────────────────────────────────────────────────
*/
int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
        return 84;
    }

    SDL_Window *fenetre = SDL_CreateWindow("redstrings - SDL",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        LARGEUR, HAUTEUR, SDL_WINDOW_SHOWN);
    if (fenetre == NULL) {
        fprintf(stderr, "SDL_CreateWindow : %s\n", SDL_GetError());
        SDL_Quit();
        return 84;
    }

    SDL_Renderer *rendu = SDL_CreateRenderer(fenetre, -1,
        SDL_RENDERER_PRESENTVSYNC);
    if (rendu == NULL) {
        fprintf(stderr, "SDL_CreateRenderer : %s\n", SDL_GetError());
        SDL_DestroyWindow(fenetre);
        SDL_Quit();
        return 84;
    }

    SDL_Rect carre = {(LARGEUR - TAILLE_CARRE) / 2,
        (HAUTEUR - TAILLE_CARRE) / 2, TAILLE_CARRE, TAILLE_CARRE};
    int ouvert = 1;

    while (ouvert) {
        SDL_Event evenement;

        while (SDL_PollEvent(&evenement)) {
            if (evenement.type == SDL_QUIT)
                ouvert = 0;
            if (evenement.type == SDL_KEYDOWN
                && evenement.key.keysym.sym == SDLK_ESCAPE)
                ouvert = 0;
        }

        const Uint8 *touches = SDL_GetKeyboardState(NULL);

        if (touches[SDL_SCANCODE_LEFT])
            carre.x -= VITESSE;
        if (touches[SDL_SCANCODE_RIGHT])
            carre.x += VITESSE;
        if (touches[SDL_SCANCODE_UP])
            carre.y -= VITESSE;
        if (touches[SDL_SCANCODE_DOWN])
            carre.y += VITESSE;

        // le carré reste dans la fenêtre
        if (carre.x < 0)
            carre.x = 0;
        if (carre.y < 0)
            carre.y = 0;
        if (carre.x > LARGEUR - TAILLE_CARRE)
            carre.x = LARGEUR - TAILLE_CARRE;
        if (carre.y > HAUTEUR - TAILLE_CARRE)
            carre.y = HAUTEUR - TAILLE_CARRE;

        SDL_SetRenderDrawColor(rendu, 30, 30, 36, 255);
        SDL_RenderClear(rendu);
        SDL_SetRenderDrawColor(rendu, 200, 40, 40, 255);
        SDL_RenderFillRect(rendu, &carre);
        SDL_RenderPresent(rendu);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(rendu);
    SDL_DestroyWindow(fenetre);
    SDL_Quit();
    return 0;
}
