/* Système de jobs (M7-02). Aussi compilé sous ThreadSanitizer : tools/scripts/tsan-jobs.sh. */
#include <SDL3/SDL.h>
#include <string.h>

#include "engine/jobs.h"
#include "utest.h"

typedef struct Work {
    uint64_t sum;
    uint32_t n;
} Work;

static void add_range(void *user)
{
    Work *w = user;
    uint64_t s = 0;
    for (uint32_t i = 1; i <= w->n; i++) {
        s += i;
    }
    w->sum = s;
}

UTEST(jobs, a_thousand_jobs_all_run_once_and_the_counter_reaches_zero)
{
    static EngJobs jobs;
    ASSERT_TRUE(eng_jobs_init(&jobs, 4));
    EXPECT_EQ(jobs.worker_count, 4U);
    static Work work[1000];
    EngJobCounter done;
    eng_job_counter_set(&done, 0);
    for (uint32_t i = 0; i < 1000U; i++) {
        work[i].n = i;
        work[i].sum = 0;
        ASSERT_TRUE(eng_jobs_submit(&jobs, add_range, &work[i], NULL, &done));
        if (i % 500U == 499U) {
            eng_jobs_wait(&jobs, &done); /* la file (1024) ne déborde jamais */
        }
    }
    eng_jobs_wait(&jobs, &done);
    EXPECT_TRUE(eng_jobs_finished(&done));
    for (uint32_t i = 0; i < 1000U; i++) {
        EXPECT_EQ(work[i].sum, (uint64_t)i * (i + 1U) / 2U);
    }
    EXPECT_EQ(eng_jobs_executed(&jobs), 1000U);
    eng_jobs_shutdown(&jobs);
}

typedef struct Chain {
    atomic_int stage;
    int seen_first;
} Chain;

static void first(void *user)
{
    Chain *c = user;
    SDL_Delay(20);
    atomic_store(&c->stage, 1);
}

static void second(void *user)
{
    Chain *c = user;
    c->seen_first = atomic_load(&c->stage);
}

UTEST(jobs, a_dependent_job_starts_only_after_its_dependency_finished)
{
    static EngJobs jobs;
    ASSERT_TRUE(eng_jobs_init(&jobs, 3));
    static Chain c;
    atomic_store(&c.stage, 0);
    c.seen_first = -1;
    EngJobCounter a;
    EngJobCounter b;
    eng_job_counter_set(&a, 0);
    eng_job_counter_set(&b, 0);
    ASSERT_TRUE(eng_jobs_submit(&jobs, first, &c, NULL, &a));
    ASSERT_TRUE(eng_jobs_submit(&jobs, second, &c, &a, &b)); /* attend `a` */
    eng_jobs_wait(&jobs, &b);
    EXPECT_EQ(c.seen_first, 1);
    eng_jobs_shutdown(&jobs);
}

UTEST(jobs, invalid_uses_are_refused_and_the_queue_is_bounded)
{
    static EngJobs jobs;
    EXPECT_FALSE(eng_jobs_init(NULL, 2));
    ASSERT_TRUE(eng_jobs_init(&jobs, 1));
    EXPECT_FALSE(eng_jobs_submit(&jobs, NULL, NULL, NULL, NULL));
    EXPECT_FALSE(eng_jobs_submit(NULL, add_range, NULL, NULL, NULL));
    /* un job bloqué par une dépendance jamais satisfaite remplit la file sans tourner */
    EngJobCounter never;
    eng_job_counter_set(&never, 1);
    static Work w;
    w.n = 3;
    uint32_t accepted = 0;
    for (uint32_t i = 0; i < ENG_JOBS_QUEUE + 10U; i++) {
        accepted += eng_jobs_submit(&jobs, add_range, &w, &never, NULL) ? 1U : 0U;
    }
    EXPECT_EQ(accepted, ENG_JOBS_QUEUE);
    eng_job_counter_set(&never, 0); /* débloque : tout tourne avant l'arrêt */
    eng_jobs_shutdown(&jobs);
    EXPECT_EQ(eng_jobs_executed(&jobs), ENG_JOBS_QUEUE);
    EXPECT_EQ(w.sum, 6U);
    EXPECT_TRUE(eng_jobs_finished(NULL));
    eng_jobs_wait(NULL, NULL);
}

UTEST(jobs, default_worker_count_is_at_least_one)
{
    static EngJobs jobs;
    ASSERT_TRUE(eng_jobs_init(&jobs, 0));
    EXPECT_GE(jobs.worker_count, 1U);
    EXPECT_LE(jobs.worker_count, ENG_JOBS_MAX_WORKERS);
    eng_jobs_shutdown(&jobs);
}
