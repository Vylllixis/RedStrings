#include "engine/jobs.h"

#include <SDL3/SDL.h>
#include <string.h>

static bool ready(const EngJob *j)
{
    return j->after == NULL || atomic_load(&j->after->pending) <= 0;
}

/* Sous le verrou : retire le premier job prêt (les jobs en attente de dépendance restent en file,
 * dans l'ordre). */
static bool take(EngJobs *jobs, EngJob *out)
{
    for (uint32_t k = 0; k < jobs->count; k++) {
        const uint32_t idx = (jobs->head + k) % ENG_JOBS_QUEUE;
        if (!ready(&jobs->queue[idx])) {
            continue;
        }
        *out = jobs->queue[idx];
        /* décale les éléments précédents d'un cran pour boucher le trou (ordre FIFO conservé) */
        for (uint32_t m = k; m > 0U; m--) {
            jobs->queue[(jobs->head + m) % ENG_JOBS_QUEUE] =
                jobs->queue[(jobs->head + m - 1U) % ENG_JOBS_QUEUE];
        }
        jobs->head = (jobs->head + 1U) % ENG_JOBS_QUEUE;
        jobs->count--;
        return true;
    }
    return false;
}

static void run(EngJobs *jobs, const EngJob *j)
{
    j->fn(j->user);
    (void)atomic_fetch_add(&jobs->executed, 1U);
    if (j->done != NULL) {
        (void)atomic_fetch_sub(&j->done->pending, 1);
    }
    /* un job fini peut débloquer une dépendance : réveiller tout le monde */
    SDL_LockMutex(jobs->lock);
    SDL_BroadcastCondition(jobs->wake);
    SDL_UnlockMutex(jobs->lock);
}

static int SDLCALL worker(void *data)
{
    EngJobs *jobs = data;
    for (;;) {
        EngJob j;
        SDL_LockMutex(jobs->lock);
        while (!take(jobs, &j)) {
            if (jobs->quit && jobs->count == 0U) {
                SDL_UnlockMutex(jobs->lock);
                return 0;
            }
            SDL_WaitConditionTimeout(jobs->wake, jobs->lock, 50);
        }
        SDL_UnlockMutex(jobs->lock);
        run(jobs, &j);
    }
}

bool eng_jobs_init(EngJobs *jobs, uint32_t workers)
{
    if (jobs == NULL) {
        return false;
    }
    memset(jobs, 0, sizeof *jobs);
    atomic_init(&jobs->executed, 0U);
    if (workers == 0U) {
        const int cores = SDL_GetNumLogicalCPUCores();
        workers = cores > 1 ? (uint32_t)(cores - 1) : 1U;
    }
    if (workers > ENG_JOBS_MAX_WORKERS) {
        workers = ENG_JOBS_MAX_WORKERS;
    }
    jobs->lock = SDL_CreateMutex();
    jobs->wake = SDL_CreateCondition();
    if (jobs->lock == NULL || jobs->wake == NULL) {
        eng_jobs_shutdown(jobs);
        return false;
    }
    for (uint32_t i = 0; i < workers; i++) {
        jobs->threads[i] = SDL_CreateThread(worker, "eng_job", jobs);
        if (jobs->threads[i] == NULL) {
            eng_jobs_shutdown(jobs);
            return false;
        }
        jobs->worker_count++;
    }
    return true;
}

void eng_jobs_shutdown(EngJobs *jobs)
{
    if (jobs == NULL) {
        return;
    }
    if (jobs->lock != NULL) {
        SDL_LockMutex(jobs->lock);
        jobs->quit = true;
        if (jobs->wake != NULL) {
            SDL_BroadcastCondition(jobs->wake);
        }
        SDL_UnlockMutex(jobs->lock);
    }
    for (uint32_t i = 0; i < jobs->worker_count; i++) {
        SDL_WaitThread(jobs->threads[i], NULL);
        jobs->threads[i] = NULL;
    }
    jobs->worker_count = 0;
    if (jobs->wake != NULL) {
        SDL_DestroyCondition(jobs->wake);
        jobs->wake = NULL;
    }
    if (jobs->lock != NULL) {
        SDL_DestroyMutex(jobs->lock);
        jobs->lock = NULL;
    }
}

bool eng_jobs_submit(EngJobs *jobs, EngJobFn fn, void *user, EngJobCounter *after,
                     EngJobCounter *done)
{
    if (jobs == NULL || fn == NULL || jobs->lock == NULL) {
        return false;
    }
    SDL_LockMutex(jobs->lock);
    if (jobs->count >= ENG_JOBS_QUEUE || jobs->quit) {
        SDL_UnlockMutex(jobs->lock);
        return false;
    }
    if (done != NULL) {
        (void)atomic_fetch_add(&done->pending, 1);
    }
    const EngJob j = {fn, user, after, done};
    jobs->queue[(jobs->head + jobs->count) % ENG_JOBS_QUEUE] = j;
    jobs->count++;
    SDL_SignalCondition(jobs->wake);
    SDL_UnlockMutex(jobs->lock);
    return true;
}

void eng_job_counter_set(EngJobCounter *counter, int value)
{
    if (counter != NULL) {
        atomic_store(&counter->pending, value);
    }
}

bool eng_jobs_finished(EngJobCounter *counter)
{
    return counter == NULL || atomic_load(&counter->pending) <= 0;
}

void eng_jobs_wait(EngJobs *jobs, EngJobCounter *counter)
{
    if (jobs == NULL || counter == NULL || jobs->lock == NULL) {
        return;
    }
    while (!eng_jobs_finished(counter)) {
        EngJob j;
        SDL_LockMutex(jobs->lock);
        const bool got = take(jobs, &j);
        if (!got && !eng_jobs_finished(counter)) {
            SDL_WaitConditionTimeout(jobs->wake, jobs->lock, 5);
        }
        SDL_UnlockMutex(jobs->lock);
        if (got) {
            run(jobs, &j);
        }
    }
}

uint32_t eng_jobs_executed(EngJobs *jobs)
{
    return jobs != NULL ? atomic_load(&jobs->executed) : 0U;
}
