/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <SDL2/SDL.h>
#include <stdio.h>

#define LARGEUR 800
#define HAUTEUR 600
#define TAILLE_CARRE 80
#define VITESSE 5 // pixels par image

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
