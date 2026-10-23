/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ lanceur_analyse.c ──────────────────────────────────────────────────
** │ But         Comprendre pourquoi un test est mort : signal reçu, code
** │             de sortie, rapport des sanitizers. Donne un titre, une
** │             cause en français, les lignes utiles du rapport et la
** │             pile d'appels réduite au code du projet.
** │ Contenu     signal_titre, cause_probable, lire_rapport,
** │             lanceur_analyser_mort
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include "lanceur_interne.h"

#define LIGNE_MAX 512

static const char *signal_titre(int sig)
{
    if (sig == SIGSEGV)
        return "Plantage : accès mémoire invalide (SIGSEGV)";
    if (sig == SIGABRT)
        return "Arrêt brutal du programme (SIGABRT)";
    if (sig == SIGFPE)
        return "Opération arithmétique impossible (SIGFPE)";
    if (sig == SIGBUS)
        return "Plantage : accès mémoire mal aligné ou hors fichier (SIGBUS)";
    if (sig == SIGILL)
        return "Instruction illégale (SIGILL)";
    if (sig == SIGALRM)
        return "Test interrompu : il ne se termine pas";
    if (sig == SIGKILL)
        return "Test tué (mémoire épuisée ?) (SIGKILL)";
    return "Test terminé par un signal";
}

/* Explication en français, à partir de ce que disent le signal et les sanitizers. */
static const char *cause_probable(int sig, const char *rapport)
{
    static const struct { const char *motif; const char *cause; } causes[] = {
        {"member access within null pointer", "un pointeur NULL est déréférencé (->) : la fonction ne vérifie pas son paramètre"},
        {"load of null pointer", "lecture à travers un pointeur NULL : un paramètre NULL n'est pas géré"},
        {"store to null pointer", "écriture à travers un pointeur NULL : un paramètre NULL n'est pas géré"},
        {"null pointer passed as argument", "un NULL est passé à une fonction de la libc qui l'interdit (strlen, memcpy…)"},
        {"out of bounds for type", "indice hors du tableau : une valeur n'est pas vérifiée avant d'indexer"},
        {"signed integer overflow", "dépassement d'un entier signé (comportement indéfini)"},
        {"division by zero", "division par zéro"},
        {"heap-buffer-overflow", "lecture ou écriture au-delà d'un tableau alloué (malloc) : taille mal calculée"},
        {"stack-buffer-overflow", "lecture ou écriture au-delà d'un tableau local : tampon trop petit"},
        {"global-buffer-overflow", "lecture ou écriture au-delà d'un tableau global ou d'une chaîne constante"},
        {"heap-use-after-free", "utilisation d'une zone mémoire déjà libérée (free)"},
        {"double-free", "la même zone mémoire est libérée deux fois"},
        {"stack-overflow", "récursion infinie ou trop profonde"},
        {"detected memory leaks", "fuite mémoire : une zone allouée n'est jamais libérée"},
        {"SEGV on unknown address 0x000000000000", "accès à l'adresse 0 : pointeur NULL déréférencé"},
        {"Assertion", "une assertion du programme a échoué"},
    };

    for (size_t i = 0; i < sizeof(causes) / sizeof(causes[0]); i++) {
        if (strstr(rapport, causes[i].motif) != NULL)
            return causes[i].cause;
    }
    if (sig == SIGALRM)
        return "boucle infinie probable, ou attente d'une entrée qui n'arrive jamais";
    if (sig == SIGSEGV)
        return "adresse invalide : pointeur NULL, non initialisé ou déjà libéré";
    if (sig == SIGFPE)
        return "division ou modulo par zéro";
    return "cause non identifiée : voir les lignes du rapport ci-dessous";
}

/* Une pile d'appels utile : du code du projet, pas de la libc ni des sanitizers. */
static int frame_utile(const char *ligne)
{
    return strstr(ligne, "libsanitizer") == NULL && strstr(ligne, "sysdeps") == NULL
        && strstr(ligne, "/csu/") == NULL && strstr(ligne, "lanceur") == NULL
        && strstr(ligne, " _start ") == NULL && strstr(ligne, "(/lib") == NULL
        && strstr(ligne, "(/usr") == NULL && strstr(ligne, "(BuildId") == NULL;
}

/* « #3 0x55e in nom_fonction chemin/fichier.c:42:7 » → « nom_fonction (chemin/fichier.c:42) » */
static int lire_frame(const char *ligne, char *dst, size_t taille)
{
    char fonction[128];
    char lieu[256];
    char *deux_points = NULL;

    if (sscanf(ligne, " #%*d 0x%*x in %127s %255s", fonction, lieu) != 2 || !frame_utile(ligne))
        return 0;
    deux_points = strchr(lieu, ':');
    if (deux_points != NULL && strchr(deux_points + 1, ':') != NULL)
        *strchr(deux_points + 1, ':') = '\0';                  // enlève la colonne
    snprintf(dst, taille, "%s  (%s)", fonction, lieu);
    return 1;
}

static void ajouter_detail(mort_t *mort, const char *ligne)
{
    size_t n = strlen(ligne);

    if (mort->nb_detail >= 4)
        return;
    snprintf(mort->detail[mort->nb_detail], TEXTE_LONG, "%.*s", (int)(n > 0 && ligne[n - 1] == '\n' ? n - 1 : n), ligne);
    mort->nb_detail++;
}

/* Lit la sortie d'erreur du fils : lignes d'erreur des sanitizers et pile d'appels. */
static void lire_rapport(FILE *erreurs, mort_t *mort, char *texte, size_t taille)
{
    char ligne[LIGNE_MAX];
    size_t n = 0;
    int pile_finie = 0;

    rewind(erreurs);
    while (fgets(ligne, sizeof(ligne), erreurs) != NULL) {
        if (n + strlen(ligne) < taille) {
            memcpy(texte + n, ligne, strlen(ligne));
            n += strlen(ligne);
        }
        if (strstr(ligne, "runtime error:") != NULL || strstr(ligne, "ERROR: ") != NULL
            || strstr(ligne, "Assertion") != NULL || strstr(ligne, "READ of size") != NULL
            || strstr(ligne, "WRITE of size") != NULL || strstr(ligne, "Direct leak") != NULL
            || strstr(ligne, "is located") != NULL)
            ajouter_detail(mort, ligne);
        else if (strstr(ligne, "allocated by") != NULL || strstr(ligne, "freed by") != NULL)
            pile_finie = 1;                    // la pile suivante est celle de l'allocation
        else if (!pile_finie && mort->nb_frames < 6 && lire_frame(ligne, mort->frames[mort->nb_frames], TEXTE_LONG))
            mort->nb_frames++;
    }
    texte[n] = '\0';
}

void lanceur_analyser_mort(int statut, int duree_ms, FILE *erreurs, mort_t *mort)
{
    char rapport[8192] = "";
    int sig = WIFSIGNALED(statut) ? WTERMSIG(statut) : 0;

    memset(mort, 0, sizeof(*mort));
    if (erreurs != NULL)
        lire_rapport(erreurs, mort, rapport, sizeof(rapport));
    if (statut < 0)
        snprintf(mort->titre, TEXTE_LONG, "Impossible de lancer le test (fork ou mémoire partagée)");
    else if (sig != 0)
        snprintf(mort->titre, TEXTE_LONG, "%s", signal_titre(sig));
    else if (strstr(rapport, "LeakSanitizer") != NULL)
        snprintf(mort->titre, TEXTE_LONG, "Fuite mémoire détectée en fin de test");
    else if (mort->nb_detail > 0)
        snprintf(mort->titre, TEXTE_LONG, "Erreur détectée par les sanitizers");
    else
        snprintf(mort->titre, TEXTE_LONG, "Le test s'est arrêté avec le code %d", WEXITSTATUS(statut));
    snprintf(mort->cause, TEXTE_LONG, "%s", cause_probable(sig, rapport));
    if (sig == SIGALRM)
        snprintf(mort->cause, TEXTE_LONG, "%s (après %d ms)", cause_probable(sig, rapport), duree_ms);
}
