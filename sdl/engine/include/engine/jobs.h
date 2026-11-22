#ifndef ENG_JOBS_H
#define ENG_JOBS_H

/* Système de jobs (M7-02) : pool de threads SDL, file FIFO bornée protégée par un mutex,
 * compteurs d'achèvement (un compteur = « tant de jobs pas encore finis »), dépendance simple : un
 * job peut attendre qu'un compteur tombe à zéro avant de démarrer.
 *
 *   EngJobCounter done = {0};
 *   eng_jobs_submit(&jobs, generate_cell, &cell, NULL, &done);
 *   ...
 *   eng_jobs_wait(&jobs, &done);   // le thread appelant aide en exécutant des jobs
 *
 * Règles : un job ne doit pas bloquer (pas d'attente de mutex du jeu) ; il n'alloue pas avec malloc
 * (mémoire préparée par l'appelant). La file est bornée (ENG_JOBS_QUEUE) : submit échoue si elle
 * est pleine plutôt que de grandir. */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define ENG_JOBS_MAX_WORKERS 16U
#define ENG_JOBS_QUEUE 1024U

typedef void (*EngJobFn)(void *user);

/* Atomiques C11 (et non SDL_AtomicInt) : ThreadSanitizer voit ainsi la synchronisation
 * job -> thread qui attend (SDL est compilé sans instrumentation). */
typedef struct EngJobCounter {
    atomic_int pending;
} EngJobCounter;

/* (Re)met un compteur à `value` (0 : aucun job en attente). */
void eng_job_counter_set(EngJobCounter *counter, int value);

typedef struct EngJob {
    EngJobFn fn;
    void *user;
    EngJobCounter *after; /* attendre que ce compteur soit à 0 (NULL : aucun) */
    EngJobCounter *done;  /* décrémenté à la fin (NULL : aucun) */
} EngJob;

typedef struct EngJobs {
    struct SDL_Mutex *lock;
    struct SDL_Condition *wake;
    struct SDL_Thread *threads[ENG_JOBS_MAX_WORKERS];
    uint32_t worker_count;
    EngJob queue[ENG_JOBS_QUEUE];
    uint32_t head;
    uint32_t count;
    bool quit;
    atomic_uint executed; /* jobs terminés depuis le début */
} EngJobs;

/* `workers` = 0 : nombre de cœurs - 1 (au moins 1), borné à ENG_JOBS_MAX_WORKERS. */
bool eng_jobs_init(EngJobs *jobs, uint32_t workers);
void eng_jobs_shutdown(EngJobs *jobs); /* attend la fin des jobs en file */

bool eng_jobs_submit(EngJobs *jobs, EngJobFn fn, void *user, EngJobCounter *after,
                     EngJobCounter *done);

/* Compteur à zéro ? (sans attendre) */
bool eng_jobs_finished(EngJobCounter *counter);

/* Attend que le compteur tombe à zéro en exécutant des jobs de la file en attendant. */
void eng_jobs_wait(EngJobs *jobs, EngJobCounter *counter);

uint32_t eng_jobs_executed(EngJobs *jobs);

#endif
