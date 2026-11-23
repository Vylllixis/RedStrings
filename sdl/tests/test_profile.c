#include <utest.h>

#include "engine/profile.h"

/* Les macros doivent compiler et s'exécuter dans les deux modes (OW_PROFILE=ON/OFF) :
 * c'est ce qui garantit que le code instrumenté ne casse pas la build ordinaire. */

UTEST(eng_profile, macros_follow_the_build_option)
{
#if defined(OW_PROFILE)
    EXPECT_EQ(ENG_PROF_ENABLED, 1);
#else
    EXPECT_EQ(ENG_PROF_ENABLED, 0);
#endif
}

UTEST(eng_profile, nested_zones_frames_and_plots_run_without_a_viewer)
{
    ENG_PROF_THREAD_NAME("test");
    int work = 0;
    for (int frame = 0; frame < 100; frame++) {
        ENG_PROF_ZONE(outer, "outer");
        for (int i = 0; i < 10; i++) {
            ENG_PROF_ZONE(inner, "inner");
            work += i;
            ENG_PROF_ZONE_END(inner);
        }
        ENG_PROF_PLOT("work", work);
        ENG_PROF_ZONE_END(outer);
        ENG_PROF_FRAME();
    }
    EXPECT_EQ(work, 100 * 45);
}
