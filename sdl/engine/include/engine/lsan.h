#pragma once

/* Fuite connue d'une dépendance, masquée pour LeakSanitizer (Debug avec ASan).
 *
 * SDL_shadercross (commit 1ff05be, SDL_ShaderCross_INTERNAL_CompileUsingDXC) ne libère pas tous
 * les objets DXC qu'il crée à chaque compilation HLSL (gestionnaire d'inclusion, blobs) : environ
 * 0,5 Ko perdu par recompilation, sans conséquence pour une session de développement, mais
 * LeakSanitizer fait échouer le programme à la sortie. On masque les allocations dont la pile
 * traverse libdxcompiler (que seul shadercross appelle) ; les fuites de notre code, elles, restent
 * signalées.
 *
 * Même traitement pour libdbus : SDL_Init (SDL_DBus_Init, inhibition de l'écran de veille) et
 * certains pilotes graphiques ouvrent une connexion D-Bus qui n'est jamais fermée. Ces fuites
 * n'apparaissent que sur une machine avec une session D-Bus (pas dans un conteneur).
 *
 * À placer UNE fois, hors de toute fonction, dans le fichier de l'exécutable (launcher, tests) :
 * la fonction doit être liée à l'exécutable lui-même, pas à une bibliothèque statique. */

#if defined(__SANITIZE_ADDRESS__)
#define ENG_LSAN_SUPPRESSIONS_ENABLED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ENG_LSAN_SUPPRESSIONS_ENABLED 1
#endif
#endif

/* Le point-virgule de l'appel est consommé par une déclaration finale (-Wpedantic refuse un
 * « ; » seul hors d'une fonction). */
#ifdef ENG_LSAN_SUPPRESSIONS_ENABLED
#define ENG_LSAN_DEFAULT_SUPPRESSIONS()                                                            \
    const char *__lsan_default_suppressions(void);                                                 \
    const char *__lsan_default_suppressions(void)                                                  \
    {                                                                                              \
        return "leak:libdxcompiler\nleak:libdbus\n";                                               \
    }                                                                                              \
    typedef int eng_lsan_suppressions_declared
#else
#define ENG_LSAN_DEFAULT_SUPPRESSIONS() typedef int eng_lsan_suppressions_declared
#endif
