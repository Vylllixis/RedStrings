/* Caméra 3e personne (M4-10) : orbite, bornes, zoom, sensibilité, lissages exacts en temps, et
 * fluidité identique à 30, 60 et 144 images par seconde. Valeurs attendues calculées à la main. */
#include <math.h>
#include <string.h>

#include "engine/orbit_camera.h"
#include "utest.h"

#define DT (1.0F / 60.0F)
#define EPS 1e-4F

static const float ORIGIN[3] = {0.0F, 0.0F, 0.0F};

static EngOrbitCamera make(float yaw, float pitch, float distance)
{
    EngOrbitCamera c;
    (void)eng_orbit_camera_init(&c, ORIGIN, yaw, pitch, distance);
    return c;
}

static bool same_camera(const EngOrbitCamera *a, const EngOrbitCamera *b)
{
    for (int i = 0; i < 3; i++) {
        if (a->target[i] != b->target[i]) {
            return false;
        }
    }
    return a->yaw == b->yaw && a->pitch == b->pitch && a->want_yaw == b->want_yaw &&
           a->want_pitch == b->want_pitch && a->distance == b->distance &&
           a->effective == b->effective && a->follow_tau == b->follow_tau &&
           a->look_tau == b->look_tau && a->sensitivity == b->sensitivity &&
           a->invert_y == b->invert_y;
}

static EngInputState look(float x, float y)
{
    EngInputState s;
    memset(&s, 0, sizeof(s));
    s.axis[ENG_AXIS_LOOK] = (EngAxisValue){x, y};
    return s;
}

static float dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return sqrtf((dx * dx) + (dy * dy) + (dz * dz));
}

UTEST(orbit_camera, eye_is_behind_the_pivot_for_yaw_and_pitch_zero)
{
    const EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    float eye[3];
    eng_orbit_camera_eye(&c, eye);
    /* regarde vers -Z : l'oeil est à +Z du pivot */
    EXPECT_NEAR(eye[0], 0.0F, EPS);
    EXPECT_NEAR(eye[1], 0.0F, EPS);
    EXPECT_NEAR(eye[2], 4.0F, EPS);
}

UTEST(orbit_camera, eye_for_yaw_and_pitch_matches_the_view_direction)
{
    EngOrbitCamera c = make(1.0F, 0.5F, 3.0F);
    EXPECT_TRUE(eng_orbit_camera_snap(&c, (float[3]){10.0F, 2.0F, -5.0F}));
    float eye[3];
    eng_orbit_camera_eye(&c, eye);
    EXPECT_NEAR(dist3(eye, c.target), 3.0F, EPS);
    /* pitch > 0 : la caméra regarde vers le haut, donc elle est sous le pivot */
    EXPECT_LT(eye[1], c.target[1]);
    /* la direction de l'oeil vers le pivot est la direction de regard de l'EngCamera appliquée */
    EngCamera view;
    eng_camera_init(&view, ORIGIN, 0.0F, 0.0F);
    eng_orbit_camera_apply(&c, &view);
    float forward[3];
    eng_camera_forward(&view, forward);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(view.position[i] + (forward[i] * 3.0F), c.target[i], EPS);
    }
}

UTEST(orbit_camera, apply_keeps_the_projection_fields_of_the_target_camera)
{
    EngOrbitCamera c = make(0.3F, -0.2F, 5.0F);
    EngCamera view;
    eng_camera_init(&view, ORIGIN, 0.0F, 0.0F);
    view.fov_y = 1.234F;
    view.near_z = 0.5F;
    view.far_z = 77.0F;
    eng_orbit_camera_apply(&c, &view);
    EXPECT_EQ(view.fov_y, 1.234F);
    EXPECT_EQ(view.near_z, 0.5F);
    EXPECT_EQ(view.far_z, 77.0F);
    EXPECT_EQ(view.yaw, c.yaw);
}

UTEST(orbit_camera, init_clamps_and_rejects_non_finite)
{
    EngOrbitCamera c = make(0.0F, 3.0F, 100.0F);
    EXPECT_EQ(c.pitch, ENG_ORBIT_PITCH_MAX);
    EXPECT_EQ(c.distance, ENG_ORBIT_DISTANCE_MAX);
    c = make(0.0F, -3.0F, 0.0F);
    EXPECT_EQ(c.pitch, ENG_ORBIT_PITCH_MIN);
    EXPECT_EQ(c.distance, ENG_ORBIT_DISTANCE_MIN);
    EngOrbitCamera before = c;
    EXPECT_FALSE(eng_orbit_camera_init(&c, ORIGIN, NAN, 0.0F, 4.0F));
    EXPECT_FALSE(eng_orbit_camera_init(&c, ORIGIN, 0.0F, INFINITY, 4.0F));
    EXPECT_FALSE(eng_orbit_camera_init(&c, ORIGIN, 0.0F, 0.0F, NAN));
    EXPECT_FALSE(eng_orbit_camera_init(&c, (float[3]){0.0F, NAN, 0.0F}, 0.0F, 0.0F, 4.0F));
    EXPECT_FALSE(eng_orbit_camera_init(NULL, ORIGIN, 0.0F, 0.0F, 4.0F));
    EXPECT_FALSE(eng_orbit_camera_init(&c, NULL, 0.0F, 0.0F, 4.0F));
    EXPECT_TRUE(same_camera(&c, &before)); /* inchangée */
}

UTEST(orbit_camera, mouse_right_turns_right_and_up_looks_up_without_smoothing)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, 0.0F, 0.0F));
    const EngInputState in = look(10.0F, 5.0F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_NEAR(c.yaw, -10.0F * ENG_CAMERA_LOOK_UNIT, EPS); /* droite : yaw diminue */
    EXPECT_NEAR(c.pitch, 5.0F * ENG_CAMERA_LOOK_UNIT, EPS); /* haut : pitch augmente */
}

UTEST(orbit_camera, invert_y_and_sensitivity_scale_the_look)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, 0.0F, 0.0F));
    EXPECT_TRUE(eng_orbit_camera_set_sensitivity(&c, 2.0F));
    c.invert_y = true;
    const EngInputState in = look(3.0F, 4.0F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_NEAR(c.yaw, -3.0F * ENG_CAMERA_LOOK_UNIT * 2.0F, EPS);
    EXPECT_NEAR(c.pitch, -4.0F * ENG_CAMERA_LOOK_UNIT * 2.0F, EPS);
}

UTEST(orbit_camera, settings_reject_out_of_range_values)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EXPECT_FALSE(eng_orbit_camera_set_sensitivity(&c, 0.0F));
    EXPECT_FALSE(eng_orbit_camera_set_sensitivity(&c, -1.0F));
    EXPECT_FALSE(eng_orbit_camera_set_sensitivity(&c, ENG_ORBIT_SENSITIVITY_MAX + 0.1F));
    EXPECT_FALSE(eng_orbit_camera_set_sensitivity(&c, NAN));
    EXPECT_FALSE(eng_orbit_camera_set_sensitivity(NULL, 1.0F));
    EXPECT_EQ(c.sensitivity, 1.0F);
    EXPECT_TRUE(eng_orbit_camera_set_sensitivity(&c, ENG_ORBIT_SENSITIVITY_MAX));
    EXPECT_FALSE(eng_orbit_camera_set_smoothing(&c, -0.1F, 0.0F));
    EXPECT_FALSE(eng_orbit_camera_set_smoothing(&c, 0.0F, 2.1F));
    EXPECT_FALSE(eng_orbit_camera_set_smoothing(&c, NAN, 0.0F));
    EXPECT_EQ(c.follow_tau, ENG_ORBIT_FOLLOW_TAU_DEFAULT);
    EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, 2.0F, 0.0F));
}

UTEST(orbit_camera, pitch_is_limited_both_ways_even_with_huge_input)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, 0.0F, 0.0F));
    EngInputState in = look(0.0F, 1e6F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_EQ(c.pitch, ENG_ORBIT_PITCH_MAX);
    in = look(0.0F, -1e6F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_EQ(c.pitch, ENG_ORBIT_PITCH_MIN);
    /* revenir de la butée est immédiat : aucune « dette » de rotation accumulée au-delà */
    in = look(0.0F, 1.0F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_NEAR(c.pitch, ENG_ORBIT_PITCH_MIN + ENG_CAMERA_LOOK_UNIT, EPS);
}

UTEST(orbit_camera, wheel_zooms_within_limits)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EngInputState in;
    memset(&in, 0, sizeof(in));
    in.wheel = 1.0F; /* vers l'avant : plus près */
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_NEAR(c.distance, 4.0F / ENG_ORBIT_ZOOM_STEP, EPS);
    in.wheel = -2.0F;
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_NEAR(c.distance, 4.0F / ENG_ORBIT_ZOOM_STEP * ENG_ORBIT_ZOOM_STEP * ENG_ORBIT_ZOOM_STEP,
                EPS);
    in.wheel = 1000.0F;
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_EQ(c.distance, ENG_ORBIT_DISTANCE_MIN);
    in.wheel = -1000.0F;
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_EQ(c.distance, ENG_ORBIT_DISTANCE_MAX);
    EXPECT_EQ(c.effective, c.distance);
}

UTEST(orbit_camera, update_resets_the_effective_distance)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    c.effective = 1.5F; /* réduit par une collision au pas précédent */
    const EngInputState in = look(0.0F, 0.0F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_EQ(c.effective, 4.0F);
}

UTEST(orbit_camera, invalid_update_arguments_change_nothing)
{
    EngOrbitCamera c = make(0.4F, 0.1F, 4.0F);
    const EngOrbitCamera before = c;
    const EngInputState in = look(5.0F, 5.0F);
    const float nanp[3] = {NAN, 0.0F, 0.0F};
    EXPECT_FALSE(eng_orbit_camera_update(NULL, &in, DT, ORIGIN));
    EXPECT_FALSE(eng_orbit_camera_update(&c, NULL, DT, ORIGIN));
    EXPECT_FALSE(eng_orbit_camera_update(&c, &in, DT, NULL));
    EXPECT_FALSE(eng_orbit_camera_update(&c, &in, DT, nanp));
    EXPECT_FALSE(eng_orbit_camera_update(&c, &in, 0.0F, ORIGIN));
    EXPECT_FALSE(eng_orbit_camera_update(&c, &in, -DT, ORIGIN));
    EXPECT_FALSE(eng_orbit_camera_update(&c, &in, 0.2F, ORIGIN));
    EXPECT_FALSE(eng_orbit_camera_update(&c, &in, NAN, ORIGIN));
    EXPECT_FALSE(eng_orbit_camera_snap(&c, nanp));
    EXPECT_TRUE(same_camera(&c, &before));
    /* une entrée de regard non finie est ignorée, la mise à jour a lieu */
    const EngInputState bad = look(NAN, 1.0F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &bad, DT, ORIGIN));
    EXPECT_EQ(c.want_yaw, before.want_yaw);
}

/* Filtre exponentiel exact : un échelon est comblé de 1 - exp(-t/tau) quel que soit le pas, et un
 * pivot qui avance à vitesse constante est suivi avec un retard v x tau. */
UTEST(orbit_camera, follow_is_an_exact_exponential_filter_with_lag_v_tau)
{
    const float tau = 0.2F;
    const float speed = 5.0F;
    float lag[2];
    const float steps[2] = {1.0F / 30.0F, 1.0F / 120.0F};
    for (int s = 0; s < 2; s++) {
        EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
        EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, tau, 0.0F));
        const EngInputState in = look(0.0F, 0.0F);
        const int n = (int)lroundf(4.0F / steps[s]);
        float pivot[3] = {0.0F, 0.0F, 0.0F};
        for (int i = 0; i < n; i++) {
            pivot[0] += speed * steps[s];
            EXPECT_TRUE(eng_orbit_camera_update(&c, &in, steps[s], pivot));
        }
        lag[s] = pivot[0] - c.target[0];
    }
    /* retard exact du filtre échantillonné : v x dt x e^(-dt/tau) / (1 - e^(-dt/tau)), soit
     * v x tau - v x dt / 2 à peu près */
    for (int s = 0; s < 2; s++) {
        const float e = expf(-steps[s] / tau);
        EXPECT_NEAR(lag[s], speed * steps[s] * e / (1.0F - e), 2e-3F);
    }

    /* échelon : après t = tau, 63,2 % de l'écart est comblé, que le pas soit 1/30 ou 1/120 */
    for (int s = 0; s < 2; s++) {
        EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
        EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, tau, 0.0F));
        const EngInputState in = look(0.0F, 0.0F);
        const float pivot[3] = {1.0F, 0.0F, 0.0F};
        const int n = (int)lroundf(tau / steps[s]);
        for (int i = 0; i < n; i++) {
            EXPECT_TRUE(eng_orbit_camera_update(&c, &in, steps[s], pivot));
        }
        EXPECT_NEAR(c.target[0], 1.0F - expf(-1.0F), 1e-4F);
    }
}

UTEST(orbit_camera, snap_jumps_to_the_pivot_without_smoothing)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    const float p[3] = {100.0F, 5.0F, -40.0F};
    EXPECT_TRUE(eng_orbit_camera_snap(&c, p));
    EXPECT_EQ(c.target[0], 100.0F);
    EXPECT_EQ(c.target[2], -40.0F);
}

UTEST(orbit_camera, flat_directions_are_horizontal_unit_and_orthogonal)
{
    const float yaws[4] = {0.0F, 0.7F, -2.0F, 5.0F};
    for (int i = 0; i < 4; i++) {
        const EngOrbitCamera c = make(yaws[i], 0.5F, 4.0F);
        float f[3];
        float r[3];
        eng_orbit_camera_flat_forward(&c, f);
        eng_orbit_camera_flat_right(&c, r);
        EXPECT_EQ(f[1], 0.0F);
        EXPECT_EQ(r[1], 0.0F);
        EXPECT_NEAR((f[0] * f[0]) + (f[2] * f[2]), 1.0F, EPS);
        EXPECT_NEAR((r[0] * r[0]) + (r[2] * r[2]), 1.0F, EPS);
        EXPECT_NEAR((f[0] * r[0]) + (f[2] * r[2]), 0.0F, EPS);
    }
    /* yaw 0 : avant = -Z, droite = +X (comme la caméra libre) */
    const EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    float f[3];
    float r[3];
    eng_orbit_camera_flat_forward(&c, f);
    eng_orbit_camera_flat_right(&c, r);
    EXPECT_NEAR(f[2], -1.0F, EPS);
    EXPECT_NEAR(r[0], 1.0F, EPS);
    /* la vue regarde bien le long de `forward` à plat quand le pitch est nul */
    EngCamera view;
    eng_camera_init(&view, ORIGIN, 0.0F, 0.0F);
    eng_orbit_camera_apply(&c, &view);
    float look_dir[3];
    eng_camera_forward(&view, look_dir);
    EXPECT_NEAR(look_dir[2], f[2], EPS);
}

/* ── Fluidité à 30, 60 et 144 images par seconde ──
 * La caméra est simulée à 60 Hz (50 pas, pivot qui avance, regard qui tourne), puis « filmée » à
 * chaque cadence en interpolant les deux derniers pas comme la boucle du moteur. À un même
 * instant, les trois cadences doivent voir la même caméra ; et d'une image à la suivante la
 * caméra ne doit jamais aller plus vite que son pas de simulation le plus rapide. */
#define SIM_STEPS 60

static void simulate(EngCamera out[SIM_STEPS + 1])
{
    EngOrbitCamera c = make(0.0F, -0.2F, 4.0F);
    EngCamera view;
    eng_camera_init(&view, ORIGIN, 0.0F, 0.0F);
    eng_orbit_camera_apply(&c, &view);
    out[0] = view;
    float pivot[3] = {0.0F, 1.6F, 0.0F};
    for (int i = 1; i <= SIM_STEPS; i++) {
        pivot[0] += 6.0F * DT;
        pivot[2] -= 2.0F * DT;
        const EngInputState in = look(4.0F, (i % 20 < 10) ? 2.0F : -2.0F);
        (void)eng_orbit_camera_update(&c, &in, DT, pivot);
        eng_orbit_camera_apply(&c, &view);
        out[i] = view;
    }
}

/* Caméra affichée à l'instant t (s), les pas valant des états à t = i / 60. */
static EngCamera shown_at(const EngCamera *steps, double t)
{
    double pos = t * 60.0;
    int i = (int)pos;
    if (i >= SIM_STEPS) {
        return steps[SIM_STEPS];
    }
    EngCamera out;
    eng_camera_lerp(&steps[i], &steps[i + 1], (float)(pos - (double)i), &out);
    return out;
}

UTEST(orbit_camera, same_picture_at_30_60_and_144_fps_and_no_jerk)
{
    EngCamera steps[SIM_STEPS + 1];
    simulate(steps);

    /* vitesse maximale de l'oeil entre deux pas, m/s */
    float vmax = 0.0F;
    for (int i = 0; i < SIM_STEPS; i++) {
        vmax = fmaxf(vmax, dist3(steps[i].position, steps[i + 1].position) / DT);
    }
    ASSERT_GT(vmax, 1.0F); /* le scénario bouge vraiment */

    const double rates[3] = {30.0, 60.0, 144.0};
    for (int r = 0; r < 3; r++) {
        EngCamera prev = shown_at(steps, 0.0);
        const int frames = (int)(0.9 * rates[r]);
        for (int f = 1; f <= frames; f++) {
            const double t = (double)f / rates[r];
            const EngCamera now = shown_at(steps, t);
            const float v = dist3(prev.position, now.position) * (float)rates[r];
            EXPECT_LE(v, vmax * 1.001F + 1e-3F); /* jamais plus vite que le pas le plus rapide */
            prev = now;
        }
    }
    /* mêmes instants : t = 0,3 s est une image à 30, 60 et 144 ips */
    const EngCamera a = shown_at(steps, 9.0 / 30.0);
    const EngCamera b = shown_at(steps, 18.0 / 60.0);
    const EngCamera d = shown_at(steps, 43.2 / 144.0);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(a.position[i], b.position[i], 1e-5F);
        EXPECT_NEAR(a.position[i], d.position[i], 1e-4F);
    }
    EXPECT_NEAR(a.yaw, b.yaw, 1e-6F);
}

UTEST(orbit_camera, accepts_the_documented_limits_and_ignores_a_non_finite_wheel)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, 0.0F, 2.0F)); /* borne incluse du regard */
    EngInputState in = look(0.0F, 0.0F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, 0.1F, ORIGIN)); /* dt maximal accepté */
    in.wheel = NAN;
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, ORIGIN));
    EXPECT_EQ(c.distance, 4.0F);
}

/* Lissage du regard : un échelon de souris est comblé de 63,2 % après tau, à tout pas de temps. */
UTEST(orbit_camera, look_smoothing_is_an_exponential_filter)
{
    const float tau = 0.1F;
    const float steps[2] = {1.0F / 30.0F, 1.0F / 120.0F};
    for (int s = 0; s < 2; s++) {
        EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
        EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, 0.0F, tau));
        EngInputState in = look(10.0F, 10.0F);
        EXPECT_TRUE(eng_orbit_camera_update(&c, &in, steps[s], ORIGIN)); /* l'échelon */
        in = look(0.0F, 0.0F);
        const int n = (int)lroundf(tau / steps[s]);
        for (int i = 1; i < n; i++) {
            EXPECT_TRUE(eng_orbit_camera_update(&c, &in, steps[s], ORIGIN));
        }
        const float want = 10.0F * ENG_CAMERA_LOOK_UNIT;
        EXPECT_NEAR(c.yaw, -want * (1.0F - expf(-1.0F)), 1e-4F);
        EXPECT_NEAR(c.pitch, want * (1.0F - expf(-1.0F)), 1e-4F);
        EXPECT_NEAR(c.want_yaw, -want, 1e-6F); /* la demande, elle, est immédiate */
    }
}

UTEST(orbit_camera, without_smoothing_the_pivot_is_taken_exactly)
{
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EXPECT_TRUE(eng_orbit_camera_set_smoothing(&c, 0.0F, 0.0F));
    const float p[3] = {0.1F, 3.3F, -7.7F};
    const EngInputState in = look(0.0F, 0.0F);
    EXPECT_TRUE(eng_orbit_camera_update(&c, &in, DT, p));
    EXPECT_EQ(c.target[0], p[0]);
    EXPECT_EQ(c.target[1], p[1]);
    EXPECT_EQ(c.target[2], p[2]);
}
