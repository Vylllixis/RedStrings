#include "engine/atmosphere.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979323846F
/* sinus de l'élévation sous laquelle le crépuscule est fini (-12°) */
#define TWILIGHT_SIN 0.21F
/* largeur (en y) du raccord ciel / sol */
#define HORIZON_BLEND 0.05F
/* demi-largeur (en sinus de l'élévation) du fondu de visibilité du soleil direct */
#define SUN_FADE 0.02F

void eng_atmosphere_default(EngAtmosphere *a)
{
    *a = (EngAtmosphere){
        .rayleigh = {0.040F, 0.094F, 0.229F},
        .mie = 0.03F,
        .mie_g = 0.70F,
        .irradiance = 50.0F,
        .sun_radiance = 60.0F,
        .sun_radius_deg = 0.8F,
        .path_limit = 8.0F,
        .sun_path_limit = 15.0F,
        .ground_albedo = {0.25F, 0.22F, 0.18F},
        .night = {0.002F, 0.003F, 0.006F},
    };
}

static bool finite_in(float v, float lo, float hi)
{
    return isfinite(v) && v >= lo && v <= hi;
}

bool eng_atmosphere_valid(const EngAtmosphere *a)
{
    if (a == NULL) {
        return false;
    }
    for (int c = 0; c < 3; c++) {
        if (!finite_in(a->rayleigh[c], 1e-4F, 100.0F) ||
            !finite_in(a->ground_albedo[c], 0.0F, 1.0F) || !finite_in(a->night[c], 0.0F, 1e6F)) {
            return false;
        }
    }
    return finite_in(a->mie, 1e-4F, 100.0F) && isfinite(a->mie_g) && a->mie_g > -0.999F &&
           a->mie_g < 0.999F && finite_in(a->irradiance, 0.0F, 1e6F) &&
           finite_in(a->sun_radiance, 0.0F, 1e7F) && isfinite(a->sun_radius_deg) &&
           a->sun_radius_deg > 0.0F && a->sun_radius_deg <= 45.0F &&
           finite_in(a->path_limit, 1.0F, 1000.0F) && finite_in(a->sun_path_limit, 1.0F, 1000.0F);
}

float eng_air_mass(float cos_zenith)
{
    float mu = cos_zenith;
    if (!(mu > 0.0F)) {
        mu = 0.0F;
    }
    if (mu > 1.0F) {
        mu = 1.0F;
    }
    return 1.0F / (mu + (0.025F * expf(-11.0F * mu)));
}

static bool unit3(const float in[3], float out[3])
{
    if (!isfinite(in[0]) || !isfinite(in[1]) || !isfinite(in[2])) {
        return false;
    }
    const float len = sqrtf((in[0] * in[0]) + (in[1] * in[1]) + (in[2] * in[2]));
    if (!(len > 1e-12F)) {
        return false;
    }
    for (int c = 0; c < 3; c++) {
        out[c] = in[c] / len;
    }
    return true;
}

static float min_f(float a, float b)
{
    return a < b ? a : b;
}

static float clamp01(float v)
{
    return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
}

/* ciel au-dessus de l'horizon : `view` peut avoir y < 0, seul le cosinus avec le soleil et la
 * masse d'air (de y borné à 0) comptent */
static void sky_above(const EngAtmosphere *a, const float sun[3], const float view[3], float rgb[3])
{
    const float m_view = min_f(eng_air_mass(view[1]), a->path_limit);
    const float m_sun = min_f(eng_air_mass(sun[1]), a->sun_path_limit);
    const float c = (sun[0] * view[0]) + (sun[1] * view[1]) + (sun[2] * view[2]);
    const float phase_r = (3.0F / (16.0F * PI_F)) * (1.0F + (c * c));
    const float g = a->mie_g;
    const float phase_m =
        (1.0F - (g * g)) / (4.0F * PI_F * powf(1.0F + (g * g) - (2.0F * g * c), 1.5F));
    const float twilight = clamp01((sun[1] + TWILIGHT_SIN) / TWILIGHT_SIN);
    const float lit = twilight * twilight;
    for (int ch = 0; ch < 3; ch++) {
        const float ext = a->rayleigh[ch] + a->mie;
        const float scatter = (a->rayleigh[ch] * phase_r) + (a->mie * phase_m);
        const float transmit_sun = expf(-ext * m_sun);
        const float inscatter = (1.0F - expf(-ext * m_view)) / ext;
        rgb[ch] = (a->irradiance * scatter * inscatter * transmit_sun * lit) + a->night[ch];
    }
}

bool eng_atmosphere_sky(const EngAtmosphere *a, const float sun_in[3], const float view_in[3],
                        float rgb[3])
{
    float sun[3];
    float view[3];
    memset(rgb, 0, 3 * sizeof(float));
    if (!eng_atmosphere_valid(a) || !unit3(sun_in, sun) || !unit3(view_in, view)) {
        return false;
    }
    sky_above(a, sun, view, rgb);
    if (view[1] < 0.0F) {
        /* sol : albedo x ciel à l'horizon dans le même azimut (ou le ciel au zénith si la vue est
         * à la verticale) */
        float flat[3] = {view[0], 0.0F, view[2]};
        float horizon[3];
        if (!unit3(flat, flat)) {
            flat[0] = 1.0F;
            flat[2] = 0.0F;
        }
        sky_above(a, sun, flat, horizon);
        const float t = clamp01(-view[1] / HORIZON_BLEND);
        for (int c = 0; c < 3; c++) {
            const float ground = a->ground_albedo[c] * horizon[c];
            rgb[c] = horizon[c] + ((ground - horizon[c]) * t);
        }
    }
    return true;
}

bool eng_atmosphere_horizon(const EngAtmosphere *a, const float sun_in[3], const float direction[3],
                            float rgb[3])
{
    float sun[3];
    memset(rgb, 0, 3 * sizeof(float));
    if (!eng_atmosphere_valid(a) || !unit3(sun_in, sun) || !isfinite(direction[0]) ||
        !isfinite(direction[2])) {
        return false;
    }
    float flat[3] = {direction[0], 0.0F, direction[2]};
    if (!unit3(flat, flat)) {
        flat[0] = 1.0F;
        flat[2] = 0.0F;
    }
    sky_above(a, sun, flat, rgb);
    return true;
}

/* transmittance du soleil à travers toute l'atmosphère, par canal */
static void sun_transmittance(const EngAtmosphere *a, float sun_y, float out[3])
{
    const float m = eng_air_mass(sun_y);
    for (int c = 0; c < 3; c++) {
        out[c] = expf(-(a->rayleigh[c] + a->mie) * m);
    }
}

bool eng_atmosphere_radiance(const EngAtmosphere *a, const float sun_in[3], const float view_in[3],
                             float rgb[3])
{
    if (!eng_atmosphere_sky(a, sun_in, view_in, rgb)) {
        return false;
    }
    float sun[3];
    float view[3];
    (void)unit3(sun_in, sun);
    (void)unit3(view_in, view);
    const float c = (sun[0] * view[0]) + (sun[1] * view[1]) + (sun[2] * view[2]);
    const float radius = a->sun_radius_deg * (PI_F / 180.0F);
    const float cos_edge = cosf(radius);
    const float cos_core = cosf(radius * 0.8F);
    if (view[1] >= 0.0F && c > cos_edge) {
        const float limb = clamp01((c - cos_edge) / (cos_core - cos_edge));
        float transmit[3];
        sun_transmittance(a, sun[1], transmit);
        for (int ch = 0; ch < 3; ch++) {
            rgb[ch] += a->sun_radiance * transmit[ch] * limb;
        }
    }
    return true;
}

bool eng_atmosphere_sun_light(const EngAtmosphere *a, const float sun_in[3], float color[3],
                              float *visibility)
{
    float sun[3];
    color[0] = color[1] = color[2] = 0.0F;
    *visibility = 0.0F;
    if (!eng_atmosphere_valid(a) || !unit3(sun_in, sun)) {
        return false;
    }
    float t[3];
    sun_transmittance(a, sun[1], t);
    for (int c = 0; c < 3; c++) {
        color[c] = t[c] / expf(-(a->rayleigh[c] + a->mie) * eng_air_mass(1.0F));
    }
    *visibility = clamp01((sun[1] + SUN_FADE) / (2.0F * SUN_FADE));
    return true;
}

/* Moyenne du ciel sur 9 directions : le zénith et 8 directions à 45° de hauteur, équiréparties en
 * azimut. Indépendante de l'azimut du soleil à une symétrie près, et sans le halo de Mie qui
 * ferait varier le zénith d'un facteur 5 selon que le soleil est au-dessus ou non. */
static void hemisphere_mean(const EngAtmosphere *a, const float sun[3], float out[3])
{
    const float h = 0.70710678F;
    out[0] = out[1] = out[2] = 0.0F;
    for (int i = 0; i < 9; i++) {
        float d[3] = {0.0F, 1.0F, 0.0F};
        if (i > 0) {
            const float az = (float)(i - 1) * (PI_F / 4.0F);
            d[0] = h * sinf(az);
            d[1] = h;
            d[2] = h * cosf(az);
        }
        float rgb[3];
        sky_above(a, sun, d, rgb);
        for (int c = 0; c < 3; c++) {
            out[c] += rgb[c] / 9.0F;
        }
    }
}

bool eng_atmosphere_ambient(const EngAtmosphere *a, const float sun_in[3], float rgb[3])
{
    float sun[3];
    rgb[0] = rgb[1] = rgb[2] = 0.0F;
    if (!eng_atmosphere_valid(a) || !unit3(sun_in, sun)) {
        return false;
    }
    const float up[3] = {0.0F, 1.0F, 0.0F};
    float now[3];
    float noon[3];
    hemisphere_mean(a, sun, now);
    hemisphere_mean(a, up, noon);
    for (int c = 0; c < 3; c++) {
        rgb[c] = now[c] / noon[c];
    }
    return true;
}

bool eng_atmosphere_to_sky_desc(const EngAtmosphere *a, const float sun_in[3], EngSkyDesc *out)
{
    float sun[3];
    if (!eng_atmosphere_valid(a) || !unit3(sun_in, sun)) {
        return false;
    }
    const float up[3] = {0.0F, 1.0F, 0.0F};
    sky_above(a, sun, up, out->zenith);
    /* horizon : direction horizontale perpendiculaire au soleil (ni dans son halo ni en face) */
    float side[3] = {-sun[2], 0.0F, sun[0]};
    if (!unit3(side, side)) {
        side[0] = 1.0F;
        side[2] = 0.0F;
    }
    sky_above(a, sun, side, out->horizon);
    for (int c = 0; c < 3; c++) {
        out->ground[c] = a->ground_albedo[c] * out->horizon[c];
        out->sun_direction[c] = sun[c];
    }
    float transmit[3];
    sun_transmittance(a, sun[1], transmit);
    for (int c = 0; c < 3; c++) {
        out->sun_radiance[c] = a->sun_radiance * transmit[c];
    }
    out->sun_angular_radius_deg = a->sun_radius_deg;
    return true;
}

void eng_atmosphere_pipeline_desc(EngPipelineDesc *desc)
{
    *desc = (EngPipelineDesc){
        .name = "atmosphere",
        .vertex_shader = "sky.vert",
        .fragment_shader = "atmosphere.frag",
        .cull_mode = ENG_CULL_NONE,
        .depth_test = false,
        .depth_write = false,
    };
}

typedef struct SkyVertexUniforms {
    float inv_view_proj[16];
    float camera[4];
} SkyVertexUniforms;

typedef struct AtmosphereFragmentUniforms {
    float sun[4];      /* xyz : vers le soleil ; w : cos du rayon du disque */
    float rayleigh[4]; /* xyz : Rayleigh ; w : Mie */
    float ground[4];   /* xyz : albédo du sol ; w : g de Mie */
    float night[4];    /* xyz : plancher nocturne ; w : irradiance E */
    float limits[4];   /* x : radiance du disque ; y : path_limit ; z : sun_path_limit ;
                          w : cos du coeur du disque */
    float exposure[4]; /* x : échelle d'exposition ; y : étoiles [0 ; 1] ; z : lune [0 ; 1] */
    float moon[4];     /* xyz : vers la lune (M8-07) */
} AtmosphereFragmentUniforms;

bool eng_atmosphere_draw(const EngAtmosphere *a, EngRenderQueue *queue, EngPipelineHandle pipeline,
                         EngBufferHandle dummy_vertex, mat4 view_proj, const float camera[3],
                         const float sun_in[3], float exposure_scale)
{
    return eng_atmosphere_draw_ex(a, queue, pipeline, dummy_vertex, view_proj, camera, sun_in,
                                  exposure_scale, NULL);
}

bool eng_atmosphere_draw_ex(const EngAtmosphere *a, EngRenderQueue *queue,
                            EngPipelineHandle pipeline, EngBufferHandle dummy_vertex,
                            mat4 view_proj, const float camera[3], const float sun_in[3],
                            float exposure_scale, const EngNightSky *night)
{
    if (!eng_atmosphere_valid(a)) {
        return false;
    }
    float sun[3] = {0.0F, 1.0F, 0.0F};
    if (!unit3(sun_in, sun)) {
        sun[0] = 0.0F;
        sun[1] = 1.0F;
        sun[2] = 0.0F;
    }
    SkyVertexUniforms vertex;
    mat4 inverse;
    glm_mat4_inv(view_proj, inverse);
    memcpy(vertex.inv_view_proj, inverse, sizeof vertex.inv_view_proj);
    vertex.camera[0] = camera[0];
    vertex.camera[1] = camera[1];
    vertex.camera[2] = camera[2];
    vertex.camera[3] = 0.0F;
    const float radius = a->sun_radius_deg * (PI_F / 180.0F);
    AtmosphereFragmentUniforms fragment = {
        .sun = {sun[0], sun[1], sun[2], cosf(radius)},
        .rayleigh = {a->rayleigh[0], a->rayleigh[1], a->rayleigh[2], a->mie},
        .ground = {a->ground_albedo[0], a->ground_albedo[1], a->ground_albedo[2], a->mie_g},
        .night = {a->night[0], a->night[1], a->night[2], a->irradiance},
        .limits = {a->sun_radiance, a->path_limit, a->sun_path_limit, cosf(radius * 0.8F)},
        .exposure = {exposure_scale >= 0.0F ? exposure_scale : 0.0F, 0.0F, 0.0F, 0.0F},
        .moon = {0.0F, 1.0F, 0.0F, 0.0F},
    };
    float moon_dir[3] = {0.0F, 1.0F, 0.0F};
    if (night != NULL && unit3(night->moon_dir, moon_dir)) {
        fragment.exposure[1] = isfinite(night->stars) ? clamp01(night->stars) : 0.0F;
        fragment.exposure[2] = isfinite(night->moon) ? clamp01(night->moon) : 0.0F;
        fragment.moon[0] = moon_dir[0];
        fragment.moon[1] = moon_dir[1];
        fragment.moon[2] = moon_dir[2];
    }
    const EngDrawCmd cmd = {.pipeline = pipeline, .vertex_buffer = dummy_vertex, .count = 3};
    return eng_render_draw_uniforms2(queue, &cmd, &vertex, sizeof vertex, &fragment,
                                     sizeof fragment);
}
