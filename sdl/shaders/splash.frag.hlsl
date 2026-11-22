// Écran de chargement (2026-11-22) : une scène animée entièrement calculée ici, sans autre image que
// le titre (champ de distance, engine/text_sdf.h). Le thème du jeu : l'aube sur la carte du monde
// (montagnes enneigées à gauche, volcan à droite, château sur sa colline au centre), le chevalier sur
// sa falaise au premier plan, et LE FIL ROUGE : il part de son écharpe, rejoint le mât du château
// puis le cratère du volcan (l'antre du dragon), le chemin de l'aventure, en soulignant le titre.
//
// Temps : fondu depuis le noir, le fil se déroule, le titre apparaît derrière l'aiguille, puis la
// scène vit en boucle (nuages, fumée, braises, dragon, herbe) tant que le chargement dure ; la
// corde du bas se remplit avec la progression ; à la fin, éclat du soleil puis fondu au noir.
// Repère : p.x vers la droite, p.y vers le HAUT, hauteur de l'écran = 1 (p.y de -0,5 à 0,5).
Texture2D<float4> text_tex : register(t0, space2);
SamplerState text_smp : register(s0, space2);

cbuffer Splash : register(b0, space3)
{
    float4 screen;     // xy : taille (pixels) ; z : temps (s) ; w : progression lissée [0, 1]
    float4 anim;       // x : sortie [0, 1] ; y : apparition du titre [0, 1] ; z : graine ; w : libre
    float4 title_rect; // uv du titre dans l'image (u0, v0, u1, v1)
    float4 label_rect; // uv de la légende
    float4 text_info;  // x : largeur / hauteur du titre ; y : largeur / hauteur de la légende
    // Écran-titre (2026-11-22) : la même scène derrière le menu. Écran de chargement : menu =
    // (0, 0, 1, 0), view = (0, 0, 1, 0), title.w = 0 (rien ne change).
    float4 menu;  // x : menu [0, 1] (la corde de chargement s'efface) ; y : assombrissement [0, 1] ;
                  // z : fondu (1 : visible) ; w : libre
    float4 view;  // xy : cadrage (déplacement du premier plan ; les plans lointains bougent moins) ;
                  // z : zoom autour du centre ; w : libre
    float4 title; // xy : centre du titre ; z : sa demi-largeur ; w : 1 pour les utiliser
};

struct Input {
    float2 uv : TEXCOORD0;
};

static const float PI = 3.14159265f;
static const float3 THREAD = float3(1.0f, 0.07f, 0.1f); // le rouge du fil

float hash11(float n)
{
    return frac(sin(n * 127.1f) * 43758.5453f);
}

float hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float noise1(float x)
{
    float i = floor(x);
    float f = frac(x);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(hash11(i), hash11(i + 1.0f), f);
}

float noise2(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = hash21(i);
    float b = hash21(i + float2(1.0f, 0.0f));
    float c = hash21(i + float2(0.0f, 1.0f));
    float d = hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm1(float x)
{
    float v = 0.0f;
    float a = 0.5f;
    for (int i = 0; i < 5; i++) {
        v += a * noise1(x);
        x = x * 2.03f + 17.0f;
        a *= 0.5f;
    }
    return v;
}

float fbm2(float2 p)
{
    float v = 0.0f;
    float a = 0.5f;
    for (int i = 0; i < 5; i++) {
        v += a * noise2(p);
        p = p * 2.02f + float2(13.1f, 7.7f);
        a *= 0.5f;
    }
    return v;
}

float sd_box(float2 p, float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}

float sd_segment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h);
}

// toit pointu : sommet en (0, h), base de demi-largeur w en y = 0
float sd_roof(float2 p, float w, float h)
{
    return max(abs(p.x) * (h / w) + p.y - h, -p.y) * 0.7f;
}

// triangle (distance exacte, signée)
float sd_triangle(float2 p, float2 a, float2 b, float2 c)
{
    float2 e0 = b - a;
    float2 e1 = c - b;
    float2 e2 = a - c;
    float2 v0 = p - a;
    float2 v1 = p - b;
    float2 v2 = p - c;
    float2 pq0 = v0 - e0 * saturate(dot(v0, e0) / dot(e0, e0));
    float2 pq1 = v1 - e1 * saturate(dot(v1, e1) / dot(e1, e1));
    float2 pq2 = v2 - e2 * saturate(dot(v2, e2) / dot(e2, e2));
    float sgn = sign(e0.x * e2.y - e0.y * e2.x);
    float2 d = min(min(float2(dot(pq0, pq0), sgn * (v0.x * e0.y - v0.y * e0.x)),
                       float2(dot(pq1, pq1), sgn * (v1.x * e1.y - v1.y * e1.x))),
                   float2(dot(pq2, pq2), sgn * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

float ease(float x)
{
    x = saturate(x);
    return x * x * (3.0f - 2.0f * x);
}

// ── Le fil rouge : y = f(x) entre deux points d'attache, ondulant au vent ──
static const float2 KNIGHT_AT = float2(-0.62f, -0.2f);       // les pieds du chevalier
static const float2 SCARF = float2(-0.6f, -0.118f);          // d'où part le fil
static const float2 KEEP_AT = float2(0.0f, -0.05f);          // le pied du donjon
static const float2 MAST = float2(0.0f, 0.116f);             // le haut de son mât
static const float2 CRATER = float2(0.62f, 0.095f);          // le cratère du volcan

float thread_y(float x, float t, float strand, float2 from, float2 to, float arch)
{
    float u = saturate((x - from.x) / (to.x - from.x));
    float pinned = sin(u * PI); // les deux bouts ne bougent pas
    // de longues ondulations lentes : un ruban au vent, pas une corde qui vibre
    float wave = sin(u * 5.2f - t * 1.1f + strand * 2.1f) * 0.017f +
                 sin(u * 9.5f + t * 0.7f + strand * 4.7f) * 0.006f;
    return lerp(from.y, to.y, u) + pinned * arch + sin(u * PI * 2.0f) * arch * 0.22f +
           wave * pinned * (1.0f + fmod(strand, 3.0f) * 0.45f);
}

// lueur d'un brin : cœur net et halo ; `reveal` : part déjà déroulée (0 à 1)
float thread_glow(float2 p, float t, float strand, float2 from, float2 to, float arch, float reveal,
                  float width, float aa)
{
    float u = (p.x - from.x) / (to.x - from.x);
    float x = clamp(p.x, from.x, to.x);
    float y = thread_y(x, t, strand, from, to, arch);
    float slope = (thread_y(x + 0.004f, t, strand, from, to, arch) - y) / 0.004f;
    // entre les attaches : distance à la courbe ; au-delà : distance au bout (pas de bord franc)
    float d = u < 0.0f || u > 1.0f ? length(p - float2(x, y))
                                   : abs(p.y - y) / sqrt(1.0f + slope * slope);
    float shown = smoothstep(reveal, reveal - 0.05f, u) * step(0.0005f, reveal);
    float core = smoothstep(width + aa, width - aa, d);
    float halo = exp(-d * 120.0f) * 0.55f + exp(-d * 30.0f) * 0.14f;
    // les brins se rejoignent aux attaches : leur halo ne s'y additionne pas en une tache
    float knot = 0.45f + 0.55f * sin(saturate(u) * PI);
    return (core + halo * knot) * shown;
}

// ── Silhouettes ──

// le château : donjon, deux tours à toit pointu, remparts crénelés ; `c` : pied du donjon
float castle(float2 p, float2 c)
{
    float2 q = (p - c) / 1.35f; // agrandi : la distance renvoyée est à cette échelle
    float d = sd_box(q - float2(0.0f, 0.036f), float2(0.022f, 0.036f)); // donjon
    d = min(d, sd_roof(q - float2(0.0f, 0.072f), 0.028f, 0.03f));
    float2 s = float2(abs(q.x), q.y); // symétrie : deux tours, deux remparts
    d = min(d, sd_box(s - float2(0.058f, 0.024f), float2(0.012f, 0.024f)));
    d = min(d, sd_roof(s - float2(0.058f, 0.048f), 0.016f, 0.022f));
    d = min(d, sd_box(s - float2(0.03f, 0.011f), float2(0.03f, 0.011f)));  // rempart
    d = min(d, sd_box(s - float2(0.086f, 0.008f), float2(0.02f, 0.008f))); // rempart extérieur
    // créneaux : des dents sur le rempart et le haut des tours
    float teeth = step(0.5f, frac(q.x * 160.0f));
    float crenel = sd_box(s - float2(0.03f, 0.0245f), float2(0.03f, 0.0025f));
    d = min(d, max(crenel, 0.5f - teeth) * 1.0f + (1.0f - teeth) * 0.01f);
    d = min(d, sd_segment(q, float2(0.0f, 0.1f), float2(0.0f, 0.122f)) - 0.0012f); // mât
    return d * 1.35f;
}

// le chevalier : casque à pointe, corps, bouclier dans le dos, épée ; pieds en `c`
float knight(float2 p, float2 c)
{
    float2 q = (p - c) / 1.55f;
    float d = sd_box(q - float2(0.0f, 0.028f), float2(0.013f, 0.02f)) - 0.006f; // corps
    d = min(d, length(q - float2(0.0f, 0.066f)) - 0.02f);                       // casque
    d = min(d, sd_roof(q - float2(0.0f, 0.082f), 0.006f, 0.016f));               // pointe
    d = min(d, sd_box(q - float2(-0.008f, 0.004f), float2(0.005f, 0.008f)));     // jambes
    d = min(d, sd_box(q - float2(0.008f, 0.004f), float2(0.005f, 0.008f)));
    d = min(d, sd_segment(q, float2(0.02f, 0.03f), float2(0.034f, 0.088f)) - 0.0022f); // épée
    d = min(d, sd_segment(q, float2(0.016f, 0.04f), float2(0.03f, 0.036f)) - 0.002f);  // garde
    d = min(d, sd_box(q - float2(-0.019f, 0.03f), float2(0.004f, 0.016f)) - 0.003f);    // bouclier
    return d * 1.55f;
}

// la cape et l'écharpe rouges, au vent (vers la droite)
float cape(float2 p, float2 c, float t)
{
    float2 q = (p - c) / 1.55f - float2(0.004f, 0.046f);
    float along = saturate(q.x / 0.075f);
    float flap = sin(along * 7.0f - t * 3.2f) * 0.012f * along;
    float body = abs(q.y + along * 0.026f - flap) - lerp(0.02f, 0.006f, along);
    return max(body, max(-q.x, q.x - 0.075f)) * 1.55f;
}

// le dragon, loin dans le ciel, tourné vers +x : queue, corps, cou, tête, deux ailes qui battent
float dragon(float2 p, float2 c, float size, float t)
{
    float2 q = (p - c) / size;
    float flap = sin(t * 4.6f);
    float d = sd_segment(q, float2(-1.25f, 0.1f + 0.06f * sin(t * 2.3f)), float2(-0.45f, 0.0f)) -
              lerp(0.012f, 0.06f, saturate((q.x + 1.25f) / 0.8f)); // queue
    d = min(d, sd_segment(q, float2(-0.45f, 0.0f), float2(0.35f, 0.02f)) - 0.1f); // corps
    d = min(d, sd_segment(q, float2(0.35f, 0.02f), float2(0.78f, 0.2f)) - 0.05f); // cou
    d = min(d, sd_triangle(q, float2(0.72f, 0.27f), float2(1.05f, 0.19f), float2(0.74f, 0.13f)) - 0.02f);
    d = min(d, sd_triangle(q, float2(0.74f, 0.26f), float2(0.66f, 0.38f), float2(0.82f, 0.25f))); // corne
    // ailes : de l'épaule au bout, la membrane retombe vers l'arrière
    float2 tip = float2(-0.25f, 0.2f + flap * 0.62f);
    d = min(d, sd_triangle(q, float2(0.22f, 0.07f), tip + float2(0.0f, 0.42f), float2(-0.5f, 0.03f)) - 0.01f);
    d = min(d, sd_triangle(q, float2(0.22f, 0.07f), tip + float2(0.35f, 0.3f), float2(-0.1f, 0.05f)) - 0.01f);
    return d * size;
}

float text_field(float2 p, float2 center, float2 half_size, float4 rect)
{
    float2 q = (p - center) / half_size;
    float inside = step(abs(q.x), 1.0f) * step(abs(q.y), 1.0f);
    float2 uv = float2(lerp(rect.x, rect.z, saturate(q.x * 0.5f + 0.5f)),
                       lerp(rect.y, rect.w, saturate(0.5f - q.y * 0.5f)));
    return text_tex.SampleLevel(text_smp, uv, 0.0f).r * inside;
}

float4 main(Input input) : SV_Target0
{
    float2 res = screen.xy;
    float t = screen.z;
    float progress = saturate(screen.w);
    float outro = saturate(anim.x);
    float title_in = saturate(anim.y);
    float aspect = res.x / res.y;
    float2 p = float2((input.uv.x - 0.5f) * aspect, 0.5f - input.uv.y);
    // Cadrage (écran-titre) : chaque plan se déplace selon sa profondeur, autour d'un zoom commun.
    // `screen_p` reste le repère de l'écran (titre, corde de chargement).
    float2 screen_p = p;
    float2 cam = view.xy;
    float2 scene_p = p / max(view.z, 0.5f);
    float aa = 1.5f / res.y / max(view.z, 0.5f);
    float pan = t * 0.006f; // lent travelling : les couches lointaines défilent moins vite
    p = scene_p - cam * 0.08f; // le ciel : très loin

    // ── Ciel : nuit en haut, aube à l'horizon, soleil qui se lève derrière le château ──
    float rise = ease(t / 9.0f);
    float2 sun = float2(0.0f, lerp(-0.2f, -0.045f, rise) + outro * 0.05f);
    float h = saturate(p.y + 0.5f);
    float3 col = lerp(float3(0.95f, 0.36f, 0.12f), float3(0.2f, 0.07f, 0.2f), smoothstep(0.26f, 0.56f, h));
    col = lerp(col, float3(0.012f, 0.018f, 0.06f), smoothstep(0.45f, 1.0f, h));
    float sun_d = length(p - sun);
    float glow = exp(-sun_d * 5.5f) * (0.75f + 0.5f * rise) + exp(-sun_d * 22.0f) * 0.9f;
    // rayons : stries autour du soleil, qui tournent lentement
    float angle = atan2(p.y - sun.y, p.x - sun.x);
    float rays = pow(noise1(angle * 9.0f + t * 0.07f) * noise1(angle * 23.0f - t * 0.05f), 1.6f);
    glow += rays * exp(-sun_d * 2.6f) * 0.9f * (0.4f + 0.6f * rise);
    col += float3(1.0f, 0.52f, 0.2f) * glow;
    col += float3(1.0f, 0.9f, 0.7f) * smoothstep(0.034f, 0.03f, sun_d) * 1.4f; // le disque

    // étoiles : elles scintillent et pâlissent vers l'aube
    float2 cell = floor(p * 260.0f);
    float star = hash21(cell + anim.z);
    float2 in_cell = frac(p * 260.0f) - 0.5f;
    float twinkle = 0.6f + 0.4f * sin(t * (1.0f + star * 4.0f) + star * 40.0f);
    float stars = step(0.992f, star) * smoothstep(0.35f, 0.0f, length(in_cell)) * twinkle;
    col += stars * smoothstep(0.52f, 0.95f, h) * (1.0f - saturate(glow)) * 1.3f;

    // nuages bas, éclairés par-dessous
    float cloud = fbm2(float2(p.x * 2.2f + pan * 3.0f + t * 0.012f, p.y * 9.0f));
    float band = smoothstep(0.1f, 0.28f, h) * smoothstep(0.62f, 0.36f, h);
    cloud = smoothstep(0.5f, 0.82f, cloud) * band;
    col = lerp(col, float3(0.5f, 0.16f, 0.2f) + float3(0.9f, 0.4f, 0.12f) * exp(-sun_d * 3.0f), cloud * 0.6f);

    // ── Le dragon passe devant le soleil, de la droite (le volcan) vers la gauche ──
    p = scene_p - cam * 0.15f;
    float fly = frac(t / 13.0f + 0.66f);
    float2 dragon_at = float2(lerp(1.15f, -1.15f, fly), 0.33f + sin(fly * PI) * 0.06f + sin(t * 0.8f) * 0.008f);
    float2 dp = p;
    dp.x = dragon_at.x - (p.x - dragon_at.x); // il vole vers la gauche
    float dragon_d = dragon(dp, dragon_at, 0.062f, t);
    col = lerp(col, float3(0.03f, 0.01f, 0.04f), smoothstep(aa, -aa, dragon_d));

    // ── Montagnes lointaines : enneigées à gauche ──
    p = scene_p - cam * 0.25f;
    float far_ridge = -0.16f + 0.2f * fbm1(p.x * 2.4f + pan * 4.0f + 3.0f) +
                      0.1f * smoothstep(0.1f, -0.9f, p.x);
    float3 far_col = lerp(float3(0.2f, 0.12f, 0.27f), float3(0.62f, 0.3f, 0.26f), exp(-sun_d * 2.2f));
    // neige au-dessus d'une altitude, en langues qui descendent dans les creux
    float snow_line = -0.035f + 0.045f * noise1(p.x * 26.0f + pan * 4.0f) + 0.02f * noise1(p.x * 90.0f);
    float snow = smoothstep(snow_line, snow_line + 0.01f, p.y) * smoothstep(0.3f, -0.3f, p.x);
    far_col = lerp(far_col, float3(0.82f, 0.74f, 0.86f), snow * 0.8f);
    col = lerp(col, far_col, smoothstep(aa, -aa, p.y - far_ridge));

    // ── Le volcan, à droite : cratère rougeoyant, fumée, braises ──
    p = scene_p - cam * 0.4f;
    float2 crater = CRATER;
    float cone = crater.y - abs(p.x - crater.x) * 0.52f - 0.012f * noise1(p.x * 30.0f);
    cone = min(cone, crater.y - 0.012f + 0.4f * abs(p.x - crater.x)); // le creux du cratère
    float volcano = smoothstep(aa, -aa, p.y - cone);
    float3 volcano_col = float3(0.09f, 0.04f, 0.07f);
    // coulées : veines de lave qui descendent du cratère
    float down = saturate((crater.y - p.y) / 0.3f);
    float vein = noise2(float2((p.x - crater.x) * 46.0f / (0.25f + down), down * 7.0f - t * 0.15f));
    float lava = pow(saturate(vein * 1.6f - 0.72f), 2.0f) * (1.0f - down) * 5.0f;
    volcano_col += float3(1.0f, 0.3f, 0.04f) * lava * (0.75f + 0.25f * sin(t * 2.0f + p.x * 30.0f));
    col = lerp(col, volcano_col, volcano);
    float crater_d = length((p - crater) * float2(1.0f, 1.6f));
    float pulse = 0.8f + 0.2f * sin(t * 2.4f) + 0.1f * sin(t * 7.1f);
    col += float3(1.0f, 0.3f, 0.05f) * exp(-crater_d * 16.0f) * 1.3f * pulse;
    // fumée : elle monte, s'élargit et part au vent (vers la gauche)
    float2 sp = p - crater;
    float up = saturate(sp.y / 0.42f);
    float plume_x = sp.x + up * up * 0.16f;
    float plume = fbm2(float2(plume_x * 9.0f / (0.4f + up * 2.4f), sp.y * 7.0f - t * 0.32f));
    float plume_mask = step(0.0f, sp.y) * smoothstep(0.05f + up * 0.2f, 0.0f, abs(plume_x)) * (1.0f - up * 0.75f);
    float smoke = smoothstep(0.28f, 0.62f, plume) * plume_mask;
    float3 smoke_col = lerp(float3(1.0f, 0.36f, 0.1f), float3(0.42f, 0.25f, 0.34f), saturate(up * 3.0f));
    col = lerp(col, smoke_col, smoke * 0.85f);
    for (int e = 0; e < 18; e++) { // braises
        float fe = float(e);
        float life = frac(t * (0.08f + 0.05f * hash11(fe + 3.0f)) + hash11(fe));
        float2 at = crater + float2((hash11(fe + 9.0f) - 0.5f) * 0.05f - life * life * 0.14f +
                                        sin(t * 1.7f + fe) * 0.006f,
                                    life * (0.16f + 0.2f * hash11(fe + 5.0f)));
        col += float3(1.0f, 0.45f, 0.1f) * exp(-length(p - at) * 420.0f) * (1.0f - life) * 1.6f;
    }

    // ── Collines et forêts ; dunes du désert à droite ──
    p = scene_p - cam * 0.6f;
    float mid = -0.215f + 0.075f * fbm1(p.x * 3.6f + pan * 9.0f + 40.0f);
    float pines = abs(frac(p.x * 46.0f + pan * 115.0f) - 0.5f) * 2.0f;
    float pine_h = 0.03f * (1.0f - pines) * (0.5f + 0.5f * noise1(floor(p.x * 46.0f + pan * 115.0f)));
    float forest = smoothstep(-0.02f, -0.3f, p.x); // sapins à gauche
    float dunes = sin(p.x * 13.0f + pan * 60.0f) * 0.012f + sin(p.x * 5.0f + 1.0f) * 0.02f;
    float desert = smoothstep(0.22f, 0.5f, p.x);
    float mid_line = mid + pine_h * forest + dunes * desert;
    float3 mid_col = lerp(float3(0.07f, 0.05f, 0.12f), float3(0.28f, 0.12f, 0.1f), desert * 0.8f);
    mid_col += float3(0.5f, 0.2f, 0.08f) * exp(-sun_d * 3.2f) * 0.6f;
    col = lerp(col, mid_col, smoothstep(aa, -aa, p.y - mid_line));
    // brume au pied des collines
    float mist = fbm2(float2(p.x * 2.6f - t * 0.03f, p.y * 11.0f + 5.0f));
    float mist_band = smoothstep(-0.12f, -0.24f, p.y) * smoothstep(-0.4f, -0.26f, p.y);
    col = lerp(col, float3(0.75f, 0.36f, 0.3f), mist * mist_band * 0.38f);

    // ── La colline du château, au centre, à contre-jour ──
    p = scene_p - cam * 0.8f;
    float hill = -0.27f + 0.225f * exp(-pow(p.x / 0.3f, 2.0f)) + 0.006f * noise1(p.x * 60.0f);
    float2 keep = KEEP_AT;
    float castle_d = castle(p, keep);
    float back = min(p.y - hill, castle_d);
    float3 back_col = float3(0.035f, 0.02f, 0.06f);
    col = lerp(col, back_col, smoothstep(aa, -aa, back));
    col += float3(1.0f, 0.5f, 0.18f) * exp(-abs(back) * 260.0f) * 0.32f * step(0.0f, back); // liseré
    // fenêtres éclairées
    float2 wq = (p - keep) / 1.35f;
    float win = min(sd_box(wq - float2(0.0f, 0.045f), float2(0.004f, 0.007f)),
                    sd_box(float2(abs(wq.x), wq.y) - float2(0.058f, 0.03f), float2(0.003f, 0.005f)));
    col += float3(1.0f, 0.68f, 0.25f) * smoothstep(aa, -aa, win) * (0.85f + 0.15f * sin(t * 9.0f + p.x * 400.0f));
    // la bannière rouge du donjon
    float2 fq = (p - keep) / 1.35f - float2(0.0f, 0.113f);
    float wave_f = sin(fq.x * 190.0f - t * 6.0f) * 0.0022f * saturate(fq.x / 0.03f);
    float flag = max(abs(fq.y - wave_f) - lerp(0.0075f, 0.003f, saturate(fq.x / 0.034f)), max(-fq.x, fq.x - 0.034f));
    col = lerp(col, THREAD * 0.95f, smoothstep(aa, -aa, flag));

    // ── Premier plan : la falaise du chevalier à gauche, l'herbe au vent ──
    p = scene_p - cam;
    float gust = sin(t * 1.4f + p.x * 5.0f) * 0.5f + sin(t * 2.3f + p.x * 11.0f) * 0.25f;
    float blades = pow(noise1(p.x * 170.0f + gust * 0.9f), 2.0f) * 0.02f + noise1(p.x * 63.0f + gust * 0.5f) * 0.007f;
    float cliff = smoothstep(-0.3f, -0.52f, p.x); // 1 sur le plateau de la falaise
    float ground = -0.425f + 0.225f * cliff - 0.02f * smoothstep(-0.7f, -0.95f, p.x) +
                   0.012f * fbm1(p.x * 3.0f + 9.0f);
    float fore = p.y - (ground + blades);
    float knight_d = knight(p, KNIGHT_AT);
    float cape_d = cape(p, KNIGHT_AT, t);
    float3 fore_col = float3(0.012f, 0.008f, 0.022f);
    col = lerp(col, THREAD * float3(0.8f, 0.75f, 0.75f), smoothstep(aa, -aa, cape_d));
    col = lerp(col, fore_col, smoothstep(aa, -aa, min(fore, knight_d)));
    col += float3(1.0f, 0.45f, 0.18f) * exp(-abs(knight_d) * 380.0f) * 0.3f * step(0.0f, knight_d) *
           step(0.0f, fore) * step(0.0f, cape_d); // contre-jour sur l'armure

    // lucioles
    for (int f = 0; f < 14; f++) {
        float ff = float(f);
        float2 at = float2((hash11(ff * 3.1f) - 0.5f) * aspect * 1.05f + sin(t * 0.3f + ff) * 0.03f,
                           -0.42f + hash11(ff * 7.7f) * 0.22f + sin(t * 0.5f + ff * 2.0f) * 0.02f);
        float blink = 0.5f + 0.5f * sin(t * (0.8f + hash11(ff)) + ff * 5.0f);
        col += float3(1.0f, 0.75f, 0.35f) * exp(-length(p - at) * 300.0f) * blink * 0.8f;
    }

    // ── Le fil rouge : trois brins tressés ; du chevalier au mât du donjon, puis au cratère ──
    // ses attaches suivent chacune leur plan : l'écharpe (premier plan), le mât, le cratère
    p = scene_p;
    float2 scarf = SCARF + cam;
    float2 mast = MAST + cam * 0.8f;
    float2 crater_at = CRATER + cam * 0.4f;
    float unroll_a = ease((t - 0.35f) / 1.7f);
    float unroll_b = ease((t - 1.95f) / 1.3f);
    float thread = thread_glow(p, t, 0.0f, scarf, mast, 0.085f, unroll_a, 0.0026f, aa) +
                   thread_glow(p, t, 1.0f, scarf, mast, 0.085f, unroll_a, 0.0012f, aa) * 0.6f +
                   thread_glow(p, t, 2.0f, scarf, mast, 0.085f, unroll_a, 0.0012f, aa) * 0.45f;
    thread += thread_glow(p, t, 3.0f, mast, crater_at, 0.03f, unroll_b, 0.0026f, aa) +
              thread_glow(p, t, 4.0f, mast, crater_at, 0.03f, unroll_b, 0.0012f, aa) * 0.6f +
              thread_glow(p, t, 5.0f, mast, crater_at, 0.03f, unroll_b, 0.0012f, aa) * 0.45f;
    col += THREAD * thread * (1.0f + outro * 2.0f);
    // l'aiguille : un éclat à la pointe du fil tant qu'il se déroule
    float first = step(unroll_a, 0.999f);
    float tip_x = first > 0.5f ? lerp(scarf.x, mast.x, unroll_a) : lerp(mast.x, crater_at.x, unroll_b);
    float tip_y = first > 0.5f ? thread_y(tip_x, t, 0.0f, scarf, mast, 0.085f)
                               : thread_y(tip_x, t, 3.0f, mast, crater_at, 0.03f);
    float needle = exp(-length(p - float2(tip_x, tip_y)) * 90.0f) * step(0.001f, unroll_a) *
                   (1.0f - smoothstep(0.92f, 1.0f, unroll_b));
    col += float3(1.0f, 0.75f, 0.6f) * needle * 1.6f;

    // ── Écran-titre : la scène s'assombrit et se désature derrière les cartes et les listes ──
    float dim = saturate(menu.y);
    float grey = dot(col, float3(0.299f, 0.587f, 0.114f));
    col = lerp(col, lerp(col, float3(grey, grey * 0.9f, grey * 1.12f), 0.5f) * 0.15f, dim);
    p = screen_p; // le titre et la corde de chargement sont dans le repère de l'écran

    // ── Le titre : il apparaît de gauche à droite, derrière un éclat ; halo rouge ──
    float zoom = 1.0f + outro * 0.1f;
    float2 title_half = float2(title.w > 0.5f ? title.z : min(0.56f, aspect * 0.4f), 0.0f) * zoom;
    title_half.y = title_half.x / max(text_info.x, 0.01f);
    float2 title_at = title.w > 0.5f ? title.xy : float2(0.0f, 0.255f);
    float field = text_field(p, title_at, title_half, title_rect);
    float sweep = lerp(-title_half.x - 0.12f, title_half.x + 0.12f, ease(title_in));
    float shown = smoothstep(sweep, sweep - 0.1f, p.x);
    float edge = fwidth(field) * 0.75f + 0.002f;
    float letter = smoothstep(0.5f - edge, 0.5f + edge, field) * shown;
    float halo = smoothstep(0.12f, 0.5f, field) * shown;
    col = lerp(col, col * 0.35f, halo * 0.55f);                // assombrit derrière les lettres
    col += THREAD * halo * halo * 0.5f * (1.0f + outro * 2.0f); // halo rouge
    float shade = saturate((p.y - title_at.y) / max(title_half.y, 0.001f) * 0.5f + 0.5f);
    float3 ink = lerp(float3(1.0f, 0.66f, 0.42f), float3(1.0f, 0.97f, 0.9f), shade);
    float sheen_x = frac(t * 0.14f) * 3.2f - 1.6f;
    ink += float3(1.0f, 0.9f, 0.8f) * pow(saturate(1.0f - abs(p.x + p.y * 0.4f - sheen_x) / 0.14f), 3.0f) * 0.7f;
    col = lerp(col, ink, letter);
    col += float3(1.0f, 0.85f, 0.7f) * exp(-abs(p.x - sweep) * 26.0f) * halo * 1.3f *
           (1.0f - smoothstep(0.9f, 1.0f, title_in)); // l'éclat qui écrit

    // ── La corde du bas : elle se remplit avec le chargement, et vibre ──
    float bar_half = min(0.36f, aspect * 0.32f);
    float u_bar = (p.x + bar_half) / (2.0f * bar_half);
    float in_bar = step(0.0f, u_bar) * step(u_bar, 1.0f);
    float pluck = sin(u_bar * PI) * sin(t * 13.0f) * 0.0022f * exp(-frac(t * 0.5f) * 3.0f);
    float bar_d = abs(p.y - (-0.435f + pluck));
    float filled = smoothstep(progress + 0.004f, progress - 0.004f, u_bar);
    // écran-titre : la corde s'efface, le menu dessine la sienne au même endroit
    float loading = 1.0f - smoothstep(0.0f, 0.35f, menu.x);
    float string_core = smoothstep(0.0022f + aa, 0.0022f - aa, bar_d) * in_bar * loading;
    col = lerp(col, float3(0.2f, 0.03f, 0.05f), string_core * 0.9f * (1.0f - filled));
    col += THREAD * (string_core + exp(-bar_d * 150.0f) * 0.5f * in_bar * loading) * filled;
    float pegs = min(length(p - float2(-bar_half, -0.435f)), length(p - float2(bar_half, -0.435f))) - 0.0055f;
    col = lerp(col, float3(0.75f, 0.55f, 0.45f), smoothstep(aa, -aa, pegs) * loading); // les chevilles
    float2 bead = float2(-bar_half + progress * 2.0f * bar_half, -0.435f + pluck);
    col += float3(1.0f, 0.55f, 0.45f) * exp(-length(p - bead) * 150.0f) * 1.3f * loading; // le nœud qui avance
    // la légende, sous la corde
    float2 label_half = float2(0.0f, 0.0125f);
    label_half.x = label_half.y * max(text_info.y, 0.01f);
    float label = text_field(p, float2(0.0f, -0.467f), label_half, label_rect);
    float label_edge = fwidth(label) * 0.75f + 0.004f;
    col = lerp(col, float3(0.95f, 0.8f, 0.72f),
               smoothstep(0.5f - label_edge, 0.5f + label_edge, label) * (0.55f + 0.2f * sin(t * 2.2f)) * loading);

    // ── Sortie : le soleil éclate, puis tout s'éteint ; entrée : fondu depuis le noir ──
    float flare = sin(saturate(outro / 0.6f) * PI); // l'éclat monte puis retombe
    col += float3(1.0f, 0.62f, 0.3f) * exp(-sun_d * lerp(5.0f, 2.2f, flare)) * flare * 1.1f;
    float dusk = smoothstep(0.35f, 1.0f, outro);     // puis la scène rougit et s'éteint
    col *= lerp(float3(1.0f, 1.0f, 1.0f), float3(1.0f, 0.22f, 0.26f), dusk) * (1.0f - dusk);
    col *= smoothstep(0.0f, 0.9f, t) * saturate(menu.z);

    // vignette, grain, courbe de ton, encodage sRGB (la swapchain n'est pas sRGB)
    float2 v = input.uv - 0.5f;
    col *= 1.0f - dot(v, v) * 0.85f;
    col = 1.0f - exp(-col * 1.25f);
    col += (hash21(input.uv * res + frac(t) * 61.0f) - 0.5f) / 255.0f * 2.0f;
    col = pow(saturate(col), 1.0f / 2.2f);
    return float4(col, 1.0f);
}
