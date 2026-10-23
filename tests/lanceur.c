/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ lanceur.c ──────────────────────────────────────────────────────────
** │ But         Lance chaque test dans un processus fils : un plantage,
** │             une boucle infinie ou une erreur des sanitizers n'arrête
** │             pas les autres tests, et leur cause est expliquée.
** │ Principe    le fils écrit ses vérifications dans une mémoire partagée
** │             (rapport_t) que le père lit après waitpid()
** │ Utilisé par make tests et make csfml-tests
** ╰──────────────────────────────────────────────────────────────────────
*/

#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "lanceur_interne.h"

#define DELAI_MAX_S 10          /* un test plus long est considéré comme bloqué */

static int g_total;
static int g_reussis;
static int g_verifs;
static double g_ms;
static resume_echec_t g_liste[ECHECS_RESUME_MAX];
static int g_nb_liste;
static char g_groupe[TEXTE_COURT];
static int g_groupe_total;
static int g_groupe_reussis;

static double maintenant_ms(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

/* Un nouveau fichier de tests : on ferme le groupe précédent et on ouvre le suivant. */
static void changer_groupe(const char *fichier)
{
    if (strcmp(g_groupe, fichier) == 0)
        return;
    if (g_groupe[0] != '\0')
        rapport_pied_groupe(g_groupe_reussis, g_groupe_total);
    snprintf(g_groupe, sizeof(g_groupe), "%s", fichier);
    g_groupe_total = 0;
    g_groupe_reussis = 0;
    rapport_entete_groupe(fichier);
}

static rapport_t *rapport_partage(void)
{
    void *m = mmap(NULL, sizeof(rapport_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    return m == MAP_FAILED ? NULL : m;
}

/* Fournie par LeakSanitizer quand le test est compilé avec -fsanitize=address ; sinon NULL. */
extern int __lsan_do_recoverable_leak_check(void) __attribute__((weak));

/* Le fils : rapport partagé, erreurs vers un fichier temporaire, délai maximum. */
static void executer_fils(void (*test)(void), rapport_t *r, FILE *erreurs)
{
    g_rapport = r;
    dup2(fileno(erreurs), STDERR_FILENO);
    alarm(DELAI_MAX_S);
    test();
    fflush(stdout);
    if (r->nb_echecs == 0 && __lsan_do_recoverable_leak_check != NULL
        && __lsan_do_recoverable_leak_check() != 0)
        _exit(23);                              // fuite mémoire : le rapport est sur stderr
    _exit(r->nb_echecs > 0 ? 1 : 0);
}

static void retenir_echec(const char *nom, const char *lieu, const rapport_t *r, const mort_t *mort)
{
    resume_echec_t *e = NULL;

    if (g_nb_liste >= ECHECS_RESUME_MAX)
        return;
    e = &g_liste[g_nb_liste++];
    snprintf(e->nom, sizeof(e->nom), "%s", nom);
    snprintf(e->lieu, sizeof(e->lieu), "%s", lieu);
    if (r->nb_echecs > 0 && mort->titre[0] == '\0')
        snprintf(e->resume, sizeof(e->resume), "%.60s:%d  %.70s  → obtenu %.50s", r->echecs[0].fichier,
            r->echecs[0].ligne, r->echecs[0].expr, r->echecs[0].obtenu);
    else
        snprintf(e->resume, sizeof(e->resume), "%.90s : %.100s", mort->titre, mort->cause);
}

void lanceur_test(const char *nom, void (*test)(void), const char *fichier, int ligne)
{
    static rapport_t secours;
    rapport_t *r = rapport_partage();
    FILE *erreurs = tmpfile();
    const char *court = strncmp(nom, "test_", 5) == 0 ? nom + 5 : nom;
    char lieu[TEXTE_LONG];
    mort_t mort;
    double debut = 0.0;
    double duree = 0.0;
    int statut = -1;
    pid_t pid = -1;
    int def = rapport_trouver_definition(fichier, nom, ligne);
    int soft = 0;

    if (r == NULL) {
        memset(&secours, 0, sizeof(secours));
        r = &secours;
    }
    changer_groupe(fichier);
    setenv("UBSAN_OPTIONS", "print_stacktrace=1", 1);
    fflush(stdout);
    fflush(stderr);
    debut = maintenant_ms();
    pid = r != &secours && erreurs != NULL ? fork() : -1;
    if (pid == 0)
        executer_fils(test, r, erreurs);
    if (pid > 0 && waitpid(pid, &statut, 0) < 0)
        statut = -1;
    duree = maintenant_ms() - debut;
    memset(&mort, 0, sizeof(mort));
    soft = WIFEXITED(statut) && WEXITSTATUS(statut) == 1 && r->nb_echecs > 0;
    if (statut != 0 && !soft)
        lanceur_analyser_mort(statut, (int)duree, erreurs, &mort);
    snprintf(lieu, sizeof(lieu), "%s:%d", fichier, def);
    g_total++;
    g_groupe_total++;
    g_verifs += r->nb_verif;
    g_ms += duree;
    if (statut == 0 && r->nb_echecs == 0) {
        g_reussis++;
        g_groupe_reussis++;
        rapport_ligne_test(court, fichier, def, 1, r->nb_verif, duree);
    } else {
        rapport_ligne_test(court, fichier, def, 0, r->nb_verif, duree);
        rapport_echec(r, &mort, fichier, def);
        retenir_echec(court, lieu, r, &mort);
    }
    if (erreurs != NULL)
        fclose(erreurs);
    if (r != &secours)
        munmap(r, sizeof(rapport_t));
}

int lanceur_bilan(void)
{
    if (g_groupe[0] != '\0')
        rapport_pied_groupe(g_groupe_reussis, g_groupe_total);
    rapport_bilan(g_total, g_reussis, g_verifs, g_ms, g_liste, g_nb_liste);
    return g_reussis == g_total ? 0 : 1;
}
