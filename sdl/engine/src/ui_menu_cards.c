/* Dessin des menus : les trois cartes des parties, suspendues au fil rouge, et le panneau de la
 * carte visée (actions, saisie du nom, confirmation de suppression). Voir ui_menu.h. */
#include "engine/ui_menu.h"

#ifdef OW_UI

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_menu_draw.h"

#define PI 3.14159265F

/* ── vignette du lieu : un petit paysage à l'aube, dessiné en formes pleines ── */

typedef enum PlaceKind {
    PLACE_PLAIN = 0,
    PLACE_MOUNTAIN,
    PLACE_DESERT,
    PLACE_VOLCANO,
    PLACE_CASTLE
} PlaceKind;

static PlaceKind place_kind(const char *place)
{
    if (strncmp(place, "Mont", 4) == 0) {
        return PLACE_MOUNTAIN;
    }
    if (strncmp(place, "D\xC3\xA9s", 4) == 0 || strncmp(place, "Des", 3) == 0) {
        return PLACE_DESERT;
    }
    if (strncmp(place, "Volc", 4) == 0) {
        return PLACE_VOLCANO;
    }
    if (strncmp(place, "Ch", 2) == 0) {
        return PLACE_CASTLE;
    }
    return PLACE_PLAIN;
}

/* une crête : tout ce qui est sous la courbe, jusqu'au bas de la vignette */
static void ridge(const MenuCtx *c, float x0, float x1, float bottom, float base, float amp,
                  float freq, float phase, ImU32 col)
{
    enum { STEPS = 28 };
    ImVec2_c pts[STEPS + 3];
    int n = 0;
    pts[n++] = (ImVec2_c){x0, bottom};
    for (int i = 0; i <= STEPS; i++) {
        const float t = (float)i / (float)STEPS;
        const float y = base - (amp * (0.55F + (0.3F * sinf((t * freq) + phase)) +
                                       (0.15F * sinf((t * freq * 2.7F) + (phase * 1.9F)))));
        pts[n++] = (ImVec2_c){x0 + ((x1 - x0) * t), y};
    }
    pts[n++] = (ImVec2_c){x1, bottom};
    ImDrawList_AddConcavePolyFilled(c->dl, pts, n, col);
}

static void peak(const MenuCtx *c, float x, float base, float half, float height, ImU32 rock,
                 ImU32 snow)
{
    ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){x - half, base}, (ImVec2_c){x, base - height},
                                 (ImVec2_c){x + half, base}, rock);
    ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){x - (half * 0.34F), base - (height * 0.66F)},
                                 (ImVec2_c){x, base - height},
                                 (ImVec2_c){x + (half * 0.34F), base - (height * 0.66F)}, snow);
}

static void small_castle(const MenuCtx *c, float x, float base, float s, ImU32 col, float a)
{
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x - (s * 0.5F), base - (s * 0.9F)},
                             (ImVec2_c){x + (s * 0.5F), base}, col, 0.0F, 0);
    ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){x - (s * 0.62F), base - (s * 0.9F)},
                                 (ImVec2_c){x, base - (s * 1.6F)},
                                 (ImVec2_c){x + (s * 0.62F), base - (s * 0.9F)}, col);
    for (int side = -1; side <= 1; side += 2) {
        const float tx = x + ((float)side * s * 1.15F);
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){tx - (s * 0.28F), base - (s * 0.62F)},
                                 (ImVec2_c){tx + (s * 0.28F), base}, col, 0.0F, 0);
        ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){tx - (s * 0.36F), base - (s * 0.62F)},
                                     (ImVec2_c){tx, base - (s * 1.1F)},
                                     (ImVec2_c){tx + (s * 0.36F), base - (s * 0.62F)}, col);
    }
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x - (s * 1.5F), base - (s * 0.3F)},
                             (ImVec2_c){x + (s * 1.5F), base}, col, 0.0F, 0);
    /* la fenêtre éclairée et la bannière rouge */
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x - (s * 0.1F), base - (s * 0.62F)},
                             (ImVec2_c){x + (s * 0.1F), base - (s * 0.36F)},
                             mm_col(1.0F, 0.72F, 0.3F, a), 0.0F, 0);
    ImDrawList_AddLine(c->dl, (ImVec2_c){x, base - (s * 1.6F)}, (ImVec2_c){x, base - (s * 2.1F)},
                       col, s * 0.08F, 0);
    ImDrawList_AddTriangleFilled(c->dl, (ImVec2_c){x, base - (s * 2.1F)},
                                 (ImVec2_c){x + (s * 0.6F), base - (s * 1.95F)},
                                 (ImVec2_c){x, base - (s * 1.8F)}, mm_red(a));
}

static void vignette(const MenuCtx *c, const char *place, float x0, float y0, float x1, float y1)
{
    const PlaceKind kind = place_kind(place);
    const float w = x1 - x0;
    const float h = y1 - y0;
    const float t = c->a->clock;
    /* le ciel de l'aube : nuit en haut, braise à l'horizon ; plus froid en montagne, plus rouge
     * au volcan */
    static const float top[5][3] = {{0.07F, 0.06F, 0.2F},
                                    {0.06F, 0.08F, 0.22F},
                                    {0.2F, 0.07F, 0.16F},
                                    {0.06F, 0.02F, 0.05F},
                                    {0.08F, 0.06F, 0.2F}};
    static const float low[5][3] = {{0.96F, 0.5F, 0.22F},
                                    {0.78F, 0.56F, 0.66F},
                                    {1.0F, 0.6F, 0.24F},
                                    {0.62F, 0.13F, 0.06F},
                                    {0.98F, 0.46F, 0.18F}};
    const ImU32 sky_top = mm_col(top[kind][0], top[kind][1], top[kind][2], 1.0F);
    const ImU32 sky_low = mm_col(low[kind][0], low[kind][1], low[kind][2], 1.0F);
    ImDrawList_AddRectFilledMultiColor(c->dl, (ImVec2_c){x0, y0}, (ImVec2_c){x1, y1}, sky_top,
                                       sky_top, sky_low, sky_low);
    /* le soleil et son halo */
    const float sun_x =
        x0 + (w * (kind == PLACE_DESERT ? 0.7F : (kind == PLACE_MOUNTAIN ? 0.26F : 0.5F)));
    const float sun_y = y0 + (h * (kind == PLACE_DESERT ? 0.42F : 0.66F));
    const float sun_r = h * (kind == PLACE_DESERT ? 0.17F : 0.11F);
    if (kind != PLACE_VOLCANO) {
        ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){sun_x, sun_y}, sun_r * 2.6F,
                                   mm_col(1.0F, 0.62F, 0.3F, 0.16F), 28);
        ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){sun_x, sun_y}, sun_r * 1.6F,
                                   mm_col(1.0F, 0.7F, 0.36F, 0.28F), 24);
        ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){sun_x, sun_y}, sun_r,
                                   mm_col(1.0F, 0.92F, 0.72F, 1.0F), 24);
    }
    const ImU32 far = mm_col(0.2F, 0.12F, 0.27F, 1.0F);
    const ImU32 near = mm_col(0.045F, 0.03F, 0.075F, 1.0F);
    switch (kind) {
    case PLACE_MOUNTAIN: {
        const ImU32 rock = mm_col(0.2F, 0.17F, 0.33F, 1.0F);
        const ImU32 snow = mm_col(0.9F, 0.86F, 0.95F, 1.0F);
        peak(c, x0 + (w * 0.28F), y1, w * 0.3F, h * 0.62F, far, snow);
        peak(c, x0 + (w * 0.62F), y1, w * 0.34F, h * 0.8F, rock, snow);
        peak(c, x0 + (w * 0.9F), y1, w * 0.24F, h * 0.5F, far, snow);
        ridge(c, x0, x1, y1, y1 - (h * 0.02F), h * 0.2F, 9.0F, 1.2F, near);
        break;
    }
    case PLACE_DESERT:
        ridge(c, x0, x1, y1, y1 - (h * 0.1F), h * 0.26F, 5.0F, 0.6F,
              mm_col(0.52F, 0.25F, 0.13F, 1.0F));
        ridge(c, x0, x1, y1, y1, h * 0.24F, 7.5F, 2.4F, mm_col(0.26F, 0.11F, 0.08F, 1.0F));
        break;
    case PLACE_VOLCANO: {
        const float cx = x0 + (w * 0.56F);
        const float crater_y = y0 + (h * 0.36F);
        const float pulse = 0.75F + (0.25F * sinf(t * 2.4F));
        ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){cx, crater_y}, h * 0.34F,
                                   mm_col(1.0F, 0.3F, 0.06F, 0.18F * pulse), 28);
        ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){cx, crater_y}, h * 0.16F,
                                   mm_col(1.0F, 0.45F, 0.1F, 0.5F * pulse), 24);
        const ImVec2_c cone[4] = {{cx - (w * 0.46F), y1},
                                  {cx - (w * 0.06F), crater_y},
                                  {cx + (w * 0.06F), crater_y},
                                  {cx + (w * 0.4F), y1}};
        ImDrawList_AddConvexPolyFilled(c->dl, cone, 4, mm_col(0.07F, 0.03F, 0.05F, 1.0F), 0);
        for (int e = 0; e < 5; e++) { /* braises */
            const float life = fmodf((t * (0.22F + (0.05F * (float)e))) + ((float)e * 0.37F), 1.0F);
            ImDrawList_AddCircleFilled(
                c->dl,
                (ImVec2_c){cx + (sinf((float)e * 2.3F) * w * 0.08F), crater_y - (life * h * 0.32F)},
                h * 0.014F, mm_col(1.0F, 0.6F, 0.2F, 1.0F - life), 8);
        }
        ridge(c, x0, x1, y1, y1, h * 0.14F, 8.0F, 0.4F, near);
        break;
    }
    case PLACE_CASTLE:
        ridge(c, x0, x1, y1, y1 - (h * 0.06F), h * 0.26F, 6.0F, 2.0F, far);
        ridge(c, x0, x1, y1, y1 + (h * 0.02F), h * 0.3F, 3.2F, 5.2F, near);
        small_castle(c, x0 + (w * 0.5F), y1 - (h * 0.24F), h * 0.13F, near, 1.0F);
        break;
    default:
        ridge(c, x0, x1, y1, y1 - (h * 0.08F), h * 0.28F, 6.5F, 0.9F,
              mm_col(0.16F, 0.14F, 0.22F, 1.0F));
        ridge(c, x0, x1, y1, y1, h * 0.26F, 4.4F, 3.6F, mm_col(0.05F, 0.07F, 0.07F, 1.0F));
        for (int tree = 0; tree < 4; tree++) { /* un bosquet de chênes */
            const float tx = x0 + (w * (0.16F + (0.07F * (float)tree)));
            const float ty = y1 - (h * (0.2F + (0.03F * (float)(tree % 2))));
            ImDrawList_AddCircleFilled(c->dl, (ImVec2_c){tx, ty}, h * 0.075F,
                                       mm_col(0.05F, 0.07F, 0.07F, 1.0F), 14);
        }
        break;
    }
    ImDrawList_AddRect(c->dl, (ImVec2_c){x0, y0}, (ImVec2_c){x1, y1},
                       mm_col(1.0F, 0.9F, 0.84F, 0.22F), 0.0F, 1.2F * c->u, 0);
}

/* ── contour d'une carte ── */

enum { PERIMETER_MAX = 40 };

/* le tour d'un rectangle arrondi, depuis le milieu du haut, dans le sens des aiguilles */
static int perimeter(float x0, float y0, float x1, float y1, float r, ImVec2_c *out)
{
    const float cx[4] = {x1 - r, x1 - r, x0 + r, x0 + r};
    const float cy[4] = {y0 + r, y1 - r, y1 - r, y0 + r};
    int n = 0;
    out[n++] = (ImVec2_c){(x0 + x1) * 0.5F, y0};
    for (int corner = 0; corner < 4; corner++) {
        const float from = (-PI * 0.5F) + ((float)corner * PI * 0.5F);
        for (int k = 0; k <= 5; k++) {
            const float ang = from + ((float)k / 5.0F * PI * 0.5F);
            out[n++] = (ImVec2_c){cx[corner] + (cosf(ang) * r), cy[corner] + (sinf(ang) * r)};
        }
    }
    out[n++] = (ImVec2_c){(x0 + x1) * 0.5F, y0};
    return n;
}

static float path_length(const ImVec2_c *pts, int n)
{
    float total = 0.0F;
    for (int i = 1; i < n; i++) {
        total += hypotf(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
    }
    return total;
}

static ImVec2_c path_point(const ImVec2_c *pts, int n, float dist)
{
    for (int i = 1; i < n; i++) {
        const float len = hypotf(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
        if (dist <= len || i == n - 1) {
            const float t = len > 0.0F ? mm_clamp01(dist / len) : 0.0F;
            return (ImVec2_c){pts[i - 1].x + ((pts[i].x - pts[i - 1].x) * t),
                              pts[i - 1].y + ((pts[i].y - pts[i - 1].y) * t)};
        }
        dist -= len;
    }
    return pts[n - 1];
}

/* le fil qui coud le tour de la carte choisie : la fraction `frac` du contour */
static void sew(const MenuCtx *c, const ImVec2_c *pts, int n, float frac, float width, float alpha)
{
    if (frac < 0.01F) {
        return;
    }
    const float target = path_length(pts, n) * mm_clamp01(frac);
    ImVec2_c part[PERIMETER_MAX + 1];
    int count = 0;
    float walked = 0.0F;
    part[count++] = pts[0];
    for (int i = 1; i < n; i++) {
        const float len = hypotf(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
        if (walked + len >= target) {
            part[count++] = path_point(&pts[i - 1], 2, target - walked);
            break;
        }
        part[count++] = pts[i];
        walked += len;
    }
    mm_thread(c, part, count, width, alpha);
}

/* des points de couture : le contour d'un emplacement libre */
static void stitches(const MenuCtx *c, const ImVec2_c *pts, int n, float offset, ImU32 col,
                     float width)
{
    const float total = path_length(pts, n);
    const float dash = 11.0F * c->u;
    const float gap = 9.0F * c->u;
    const float first = fmodf(offset, dash + gap);
    const int count = total > first ? (int)ceilf((total - first) / (dash + gap)) : 0;
    for (int i = 0; i < count; i++) {
        const float d = first + ((float)i * (dash + gap));
        const ImVec2_c a = path_point(pts, n, d);
        const ImVec2_c b = path_point(pts, n, fminf(d + dash, total));
        ImDrawList_AddLine(c->dl, a, b, col, width, 0);
    }
}

/* ── une carte ── */

typedef struct CardBox {
    float cx; /* centre, pixels */
    float cy;
    float w;
    float h;
    float alpha;
    float angle;
    float knot_y; /* le nœud sur le fil, à la verticale du centre */
} CardBox;

static void card_box(const MenuCtx *c, uint32_t index, CardBox *box)
{
    const EngMotionCard *card = &c->a->cards[index];
    const float scale = card->scale.x * c->a->fit;
    box->cx = (c->w * 0.5F) + (card->x.x * c->h);
    box->cy = (c->h * 0.5F) + (card->y.x * c->h);
    box->w = ENG_MOTION_CARD_W * scale * c->h;
    box->h = ENG_MOTION_CARD_H * scale * c->h;
    box->alpha = mm_clamp01(card->alpha.x);
    box->angle = card->angle.x;
    box->knot_y = (c->h * 0.5F) + (ENG_MOTION_THREAD_Y * c->h) + (c->h * 0.011F * box->alpha);
}

static void used_card(const MenuCtx *c, uint32_t index, const EngMenuSlot *s, const CardBox *b,
                      bool editing)
{
    const float x0 = b->cx - (b->w * 0.5F);
    const float y0 = b->cy - (b->h * 0.5F);
    const float pad = b->w * 0.055F;
    const float inner = b->w - (2.0F * pad);
    const float pic_bottom = y0 + pad + (b->h * 0.4F);
    vignette(c, s->place, x0 + pad, y0 + pad, x0 + b->w - pad, pic_bottom);
    char line[96];
    (void)snprintf(line, sizeof line, "EMPLACEMENT %u", (unsigned)(index + 1U));
    const float tiny = b->h * 0.034F;
    mm_tracked(c, c->font, tiny, x0 + pad, pic_bottom + (b->h * 0.032F), mm_soft(0.62F), line,
               tiny * 0.22F, 0.0F);
    /* le nom ; pendant la saisie : le texte tapé et son curseur */
    const float name_size = mm_fit(c->bold, b->h * 0.082F, s->name[0] != '\0' ? s->name : " ", 0.0F,
                                   inner - (editing ? 8.0F * c->u : 0.0F));
    const float name_y = pic_bottom + (b->h * 0.083F);
    mm_text(c, c->bold, name_size, x0 + pad, name_y, mm_ivory(1.0F), s->name, 0.0F);
    if (editing) {
        const float caret = 0.5F + (0.5F * cosf(c->a->clock * 6.5F));
        const float cx = x0 + pad + mm_text_w(c->bold, name_size, s->name) + (2.0F * c->u);
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){cx, name_y + (name_size * 0.14F)},
                                 (ImVec2_c){cx + (2.0F * c->u), name_y + (name_size * 1.16F)},
                                 mm_red(c->a->typed > 0.05F ? 1.0F : caret), 0.0F, 0);
    }
    const float sub_y = name_y + (b->h * 0.118F);
    if (s->unreadable) {
        mm_text(c, c->font, b->h * 0.05F, x0 + pad, sub_y, mm_col(1.0F, 0.45F, 0.42F, 1.0F),
                "Fichier illisible", 0.0F);
        mm_text(c, c->font, b->h * 0.04F, x0 + pad, sub_y + (b->h * 0.075F), mm_soft(0.8F),
                "Il peut seulement être supprimé.", 0.0F);
        return;
    }
    const float sum_size = mm_fit(c->font, b->h * 0.052F, s->summary, 0.0F, inner);
    mm_text(c, c->font, sum_size, x0 + pad, sub_y, mm_col(1.0F, 0.87F, 0.8F, 1.0F), s->summary,
            0.0F);
    (void)snprintf(line, sizeof line, "%s%s%s", s->place,
                   s->place[0] != '\0' && s->date[0] != '\0' ? "  ·  " : "", s->date);
    const float info = mm_fit(c->font, b->h * 0.041F, line, 0.0F, inner);
    mm_text(c, c->font, info, x0 + pad, sub_y + (b->h * 0.078F), mm_soft(0.78F), line, 0.0F);
    if (s->boss_count > 0U) { /* un losange par boss, plein une fois vaincu */
        const float r = b->h * 0.02F;
        const float by = y0 + b->h - pad - r;
        uint32_t beaten = 0;
        for (uint32_t k = 0; k < s->boss_count && k < 8U; k++) {
            const ImVec2_c at = {x0 + pad + r + ((float)k * r * 3.1F), by};
            const bool done = (s->bosses >> k & 1U) != 0U;
            beaten += done ? 1U : 0U;
            if (done) {
                ImDrawList_AddNgonFilled(c->dl, at, r, mm_red(1.0F), 4);
            } else {
                ImDrawList_AddNgon(c->dl, at, r, mm_col(1.0F, 0.9F, 0.86F, 0.42F), 4, 1.3F * c->u,
                                   0);
            }
        }
        (void)snprintf(line, sizeof line, "%u / %u boss", (unsigned)beaten,
                       (unsigned)s->boss_count);
        const float bs = b->h * 0.036F;
        mm_text(c, c->font, bs, x0 + b->w - pad - mm_text_w(c->font, bs, line), by - (bs * 0.66F),
                mm_soft(0.72F), line, 0.0F);
    }
}

static void empty_card(const MenuCtx *c, uint32_t index, const CardBox *b, float emph)
{
    const float r = b->h * 0.085F * (1.0F + (0.06F * sinf(c->a->clock * 2.2F) * emph));
    const float cy = b->cy - (b->h * 0.08F);
    const ImU32 ink = mm_col(1.0F, 0.9F + (0.06F * emph), 0.86F, 0.5F + (0.5F * emph));
    ImDrawList_AddCircle(c->dl, (ImVec2_c){b->cx, cy}, r, ink, 40, 1.8F * c->u, 0);
    const float arm = r * 0.5F;
    ImDrawList_AddLine(c->dl, (ImVec2_c){b->cx - arm, cy}, (ImVec2_c){b->cx + arm, cy}, ink,
                       2.4F * c->u, 0);
    ImDrawList_AddLine(c->dl, (ImVec2_c){b->cx, cy - arm}, (ImVec2_c){b->cx, cy + arm}, ink,
                       2.4F * c->u, 0);
    const float size =
        mm_fit(c->bold, b->h * 0.05F, "NOUVELLE PARTIE", b->h * 0.009F, b->w * 0.84F);
    const float track = size * 0.18F;
    mm_tracked(
        c, c->bold, size, b->cx - (mm_tracked_w(c->bold, size, "NOUVELLE PARTIE", track) * 0.5F),
        b->cy + (b->h * 0.1F), mm_ivory(0.62F + (0.38F * emph)), "NOUVELLE PARTIE", track, 0.0F);
    char line[48];
    (void)snprintf(line, sizeof line, "Emplacement %u  ·  libre", (unsigned)(index + 1U));
    const float small = b->h * 0.04F;
    mm_text(c, c->font, small, b->cx - (mm_text_w(c->font, small, line) * 0.5F),
            b->cy + (b->h * 0.19F), mm_soft(0.66F), line, 0.0F);
}

/* la carte, suspendue par deux brins au nœud du fil ; `falling` : le fil est coupé */
static void draw_card(const MenuCtx *c, uint32_t index, const EngMenuSlot *s, const CardBox *b,
                      float emph, bool editing, bool falling)
{
    if (b->alpha < 0.012F) {
        return;
    }
    const float x0 = b->cx - (b->w * 0.5F);
    const float y0 = b->cy - (b->h * 0.5F);
    const float x1 = x0 + b->w;
    const float y1 = y0 + b->h;
    const float r = b->w * 0.04F;
    const int start = mm_mark(c);
    if (emph > 0.01F && !c->hc) { /* la carte choisie rougeoie */
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x0 - (16.0F * c->u), y0 - (16.0F * c->u)},
                                 (ImVec2_c){x1 + (16.0F * c->u), y1 + (16.0F * c->u)},
                                 mm_red(0.07F * emph), r + (16.0F * c->u), 0);
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x0 - (7.0F * c->u), y0 - (7.0F * c->u)},
                                 (ImVec2_c){x1 + (7.0F * c->u), y1 + (7.0F * c->u)},
                                 mm_red(0.13F * emph), r + (7.0F * c->u), 0);
    }
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x0, y0}, (ImVec2_c){x1, y1},
                             c->hc ? mm_col(0.0F, 0.0F, 0.0F, 0.97F)
                                   : mm_col(0.05F, 0.036F, 0.082F, s->used ? 0.88F : 0.7F),
                             r, 0);
    ImVec2_c edge[PERIMETER_MAX];
    const int edge_n = perimeter(x0, y0, x1, y1, r, edge);
    if (s->used) {
        used_card(c, index, s, b, editing);
        ImDrawList_AddRect(c->dl, (ImVec2_c){x0, y0}, (ImVec2_c){x1, y1},
                           mm_col(1.0F, 0.9F, 0.86F, 0.2F), r, 1.4F * c->u, 0);
    } else {
        empty_card(c, index, b, emph);
        stitches(c, edge, edge_n, emph > 0.5F ? c->a->clock * 26.0F * c->u : 0.0F,
                 mm_col(1.0F, 0.9F - (0.5F * emph), 0.86F - (0.5F * emph), 0.4F + (0.5F * emph)),
                 1.8F * c->u);
    }
    if (s->used) {
        sew(c, edge, edge_n, emph, 2.4F * c->u, 1.0F);
    }
    const float knot_x = b->cx;
    mm_xform(c, start, knot_x, b->knot_y, 1.0F, b->angle, 0.0F, 0.0F, b->alpha);
    if (falling) {
        return;
    }
    /* les deux brins, du nœud aux coins du haut de la carte (qui a tourné autour du nœud) */
    const float cs = cosf(b->angle);
    const float sn = sinf(b->angle);
    for (int side = -1; side <= 1; side += 2) {
        const float lx = (float)side * b->w * 0.33F;
        const float ly = y0 - b->knot_y;
        const ImVec2_c strand[2] = {
            {knot_x, b->knot_y},
            {knot_x + (lx * cs) - (ly * sn), b->knot_y + (lx * sn) + (ly * cs)}};
        ImDrawList_AddLine(c->dl, strand[0], strand[1], mm_col(1.0F, 0.2F, 0.24F, 0.9F * b->alpha),
                           1.6F * c->u, 0);
    }
    mm_bead(c, knot_x, b->knot_y, 4.2F * c->u * (1.0F + (0.3F * emph)), b->alpha);
}

/* le contenu affiché sur la carte `index` : pendant la saisie d'un nom, ce qui est tapé */
static void shown_slot(const MenuCtx *c, uint32_t index, EngMenuSlot *out, bool *editing)
{
    const EngMenu *m = c->m;
    *out = m->slots[index];
    *editing = m->screen == ENG_MENU_NAME && (int32_t)index == m->slot;
    if (!*editing) {
        return;
    }
    if (m->name_purpose == ENG_MENU_NAME_CREATE) { /* aperçu de la partie qui va naître */
        memset(out, 0, sizeof *out);
        out->used = true;
        (void)snprintf(out->summary, sizeof out->summary, "Niv. 1  ·  nouvelle partie");
        (void)snprintf(out->place, sizeof out->place, "Château");
    }
    (void)snprintf(out->name, sizeof out->name, "%s", m->text);
}

static float card_emphasis(const MenuCtx *c, uint32_t index)
{
    const EngMenu *m = c->m;
    if (m->screen == ENG_MENU_SAVES) {
        return c->a->emphasis[index];
    }
    if (eng_menu_family(m->screen) == ENG_MENU_FAMILY_CARDS) {
        return (int32_t)index == m->slot ? 1.0F : 0.0F;
    }
    return 0.0F;
}

void mm_draw_cards(MenuCtx *c)
{
    const EngMenuMotion *a = c->a;
    CardBox box[ENG_MENU_SLOTS];
    uint32_t order[ENG_MENU_SLOTS] = {0, 1, 2};
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        card_box(c, i, &box[i]);
    }
    for (uint32_t i = 0; i + 1U < ENG_MENU_SLOTS; i++) { /* nœuds de gauche à droite */
        for (uint32_t k = i + 1U; k < ENG_MENU_SLOTS; k++) {
            if (box[order[k]].cx < box[order[i]].cx) {
                const uint32_t swap = order[i];
                order[i] = order[k];
                order[k] = swap;
            }
        }
    }
    /* le fil, d'un bord à l'autre de l'écran : il plie sous chaque carte et pend entre deux */
    enum { SEGMENT = 14, KNOTS = ENG_MENU_SLOTS + 2 };
    const float rest_y = (c->h * 0.5F) + (ENG_MOTION_THREAD_Y * c->h);
    ImVec2_c knots[KNOTS];
    knots[0] = (ImVec2_c){-0.06F * c->w, rest_y - (c->h * 0.012F)};
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        knots[i + 1U] = (ImVec2_c){box[order[i]].cx, box[order[i]].knot_y};
    }
    knots[KNOTS - 1] = (ImVec2_c){1.06F * c->w, rest_y - (c->h * 0.012F)};
    ImVec2_c line[((KNOTS - 1) * SEGMENT) + 1];
    int count = 0;
    for (int k = 0; k + 1 < KNOTS; k++) {
        const float span = fabsf(knots[k + 1].x - knots[k].x);
        for (int i = 0; i < SEGMENT; i++) {
            const float t = (float)i / (float)SEGMENT;
            const float x = knots[k].x + ((knots[k + 1].x - knots[k].x) * t);
            const float droop = 4.0F * t * (1.0F - t) * span * 0.018F;
            line[count++] =
                (ImVec2_c){x, knots[k].y + ((knots[k + 1].y - knots[k].y) * t) + droop +
                                  (sinf((x * 0.012F) + (a->clock * 1.2F)) * 0.6F * c->u)};
        }
    }
    line[count++] = knots[KNOTS - 1];
    const int shown = (int)ceilf((float)count * mm_clamp01(a->thread));
    mm_thread(c, line, shown, 2.3F * c->u, 1.0F);
    if (a->thread > 0.01F && a->thread < 0.99F && shown > 0) { /* l'aiguille qui tend le fil */
        ImDrawList_AddCircleFilled(c->dl, line[shown - 1], 6.0F * c->u,
                                   mm_col(1.0F, 0.86F, 0.78F, 0.9F), 16);
    }
    /* les cartes : la carte choisie en dernier, par-dessus les autres */
    for (int pass = 0; pass < 2; pass++) {
        for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
            const float emph = card_emphasis(c, i);
            if ((emph > 0.5F) != (pass == 1)) {
                continue;
            }
            EngMenuSlot slot;
            bool editing = false;
            shown_slot(c, i, &slot, &editing);
            CardBox b = box[i];
            if (c->m->screen == ENG_MENU_DELETE && (int32_t)i == c->m->slot) {
                b.angle += sinf(a->clock * 31.0F) * 0.012F; /* elle tremble */
            }
            draw_card(c, i, &slot, &b, emph, editing, false);
        }
    }
    if (a->ghost_time >= 0.0F) { /* la carte supprimée : son fil est coupé, elle tombe */
        const float t = a->ghost_time / ENG_MOTION_GHOST_TIME;
        const float scale = a->ghost_scale * a->fit;
        CardBox b = {.cx = (c->w * 0.5F) + (a->ghost_x * c->h) + (t * 0.04F * c->h),
                     .cy = (c->h * 0.5F) + ((a->ghost_y + (t * t * 0.9F)) * c->h),
                     .w = ENG_MOTION_CARD_W * scale * c->h,
                     .h = ENG_MOTION_CARD_H * scale * c->h,
                     .alpha = 1.0F - (t * t),
                     .angle = t * 0.55F,
                     .knot_y = 0.0F};
        b.knot_y = b.cy - (b.h * 0.5F) - (c->h * 0.03F);
        draw_card(c, (uint32_t)(a->ghost_index < 0 ? 0 : a->ghost_index), &a->ghost, &b, 0.0F,
                  false, true);
    }
}

/* ── le panneau de la carte visée ── */

static float panel_x(const MenuCtx *c)
{
    return (c->w * 0.5F) - (0.06F * c->a->fit * c->h);
}

static float panel_header(const MenuCtx *c, const EngMenu *m, float x, float y, float enter,
                          float presence)
{
    char title[ENG_MENU_LABEL];
    mm_upper(eng_menu_title(m), title, sizeof title);
    const float size = c->px * 0.82F;
    const float track = size * 0.24F;
    const float slide = (1.0F - eng_ease_out_cubic(enter / 0.3F)) * 24.0F * c->u;
    mm_tracked(c, c->bold, size, x + slide, y, mm_col(1.0F, 0.8F, 0.76F, presence), title, track,
               1.0F);
    const float tw = mm_tracked_w(c->bold, size, title, track);
    const ImVec2_c rule[2] = {
        {x, y + (size * 1.55F)},
        {x + ((tw + (60.0F * c->u)) * eng_ease_out_cubic((enter - 0.04F) / 0.4F)),
         y + (size * 1.55F)}};
    mm_thread(c, rule, 2, 2.0F * c->u, presence);
    return y + (size * 1.55F) + (c->px * 0.9F);
}

static void name_panel(MenuCtx *c, const EngMenu *m, float x, float y, float width, float enter,
                       float presence, bool live)
{
    const EngMenuMotion *a = c->a;
    const float in = eng_ease_out_cubic((enter - 0.05F) / 0.3F);
    const float shake = live ? sinf(a->clock * 55.0F) * a->deny * 7.0F * c->u : 0.0F;
    const float fx = x + ((1.0F - in) * 24.0F * c->u) + shake;
    /* le nom, en grand, avec son curseur ; un nom proposé (pas encore modifié) est surligné */
    const float size = mm_fit(c->bold, c->px * 2.1F, m->text[0] != '\0' ? m->text : " ", 0.0F,
                              width - (20.0F * c->u));
    const float tw = mm_text_w(c->bold, size, m->text);
    const int start = mm_mark(c);
    if (m->text_fresh && m->text[0] != '\0') {
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){fx - (4.0F * c->u), y + (size * 0.1F)},
                                 (ImVec2_c){fx + tw + (4.0F * c->u), y + (size * 1.24F)},
                                 mm_red(0.32F * presence), 4.0F * c->u, 0);
    }
    mm_text(c, c->bold, size, fx, y, mm_ivory(presence), m->text, 1.0F);
    mm_xform(c, start, fx, y + (size * 0.7F), 1.0F + (live ? 0.035F * a->typed : 0.0F), 0.0F, 0.0F,
             0.0F, 1.0F);
    const float blink = live && a->typed > 0.05F ? 1.0F : 0.5F + (0.5F * cosf(a->clock * 6.5F));
    ImDrawList_AddRectFilled(c->dl, (ImVec2_c){fx + tw + (3.0F * c->u), y + (size * 0.12F)},
                             (ImVec2_c){fx + tw + (6.0F * c->u), y + (size * 1.2F)},
                             mm_red(blink * presence), 0.0F, 0);
    const float rule_y = y + (size * 1.42F);
    const ImVec2_c rule[2] = {{x, rule_y}, {x + (width * in), rule_y}};
    mm_thread(c, rule, 2, 2.2F * c->u, presence);
    /* caractères utilisés */
    uint32_t chars = 0;
    for (const char *p = m->text; *p != '\0'; p++) {
        chars += ((unsigned char)*p & 0xC0U) != 0x80U ? 1U : 0U;
    }
    char counter[24];
    (void)snprintf(counter, sizeof counter, "%u / %u", (unsigned)chars,
                   (unsigned)ENG_MENU_NAME_CHARS);
    const float cs = c->px * 0.62F;
    mm_text(c, c->font, cs, x + width - mm_text_w(c->font, cs, counter), rule_y + (cs * 0.5F),
            mm_soft(0.7F * presence), counter, 1.0F);
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    const MenuRows rows = {.x = x + (18.0F * c->u),
                           .y = rule_y + (c->px * 2.0F),
                           .size = c->px * 1.25F,
                           .row_h = c->px * 1.25F * 1.8F,
                           .width = width,
                           .enter = enter - 0.12F,
                           .presence = presence,
                           .live = live,
                           .danger = -1};
    const float bottom = mm_rows(c, items, n, m->selected, &rows);
    mm_hint(c, x + (width * 0.42F), bottom + (c->px * 0.6F),
            "écris le nom   ·   Entrée : valider   ·   Échap : annuler", presence);
}

void mm_draw_cards_screen(MenuCtx *c, const EngMenu *m, float enter, float presence, bool live)
{
    const EngMenuMotion *a = c->a;
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    const float thread_y = (c->h * 0.5F) + (ENG_MOTION_THREAD_Y * c->h);
    if (m->screen == ENG_MENU_SAVES) {
        /* la légende, au-dessus du fil */
        const float cap = c->px * 0.9F;
        const float track = cap * 0.3F;
        const char *caption = "CHOISIS UNE PARTIE";
        mm_tracked(c, c->font, cap, (c->w - mm_tracked_w(c->font, cap, caption, track)) * 0.5F,
                   thread_y - (c->h * 0.062F) -
                       ((1.0F - eng_ease_out_cubic(enter / 0.35F)) * 12.0F * c->u),
                   mm_col(1.0F, 0.84F, 0.8F, 0.85F * presence), caption, track, 1.0F);
        float cards_bottom = thread_y;
        for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
            CardBox b;
            card_box(c, i, &b);
            cards_bottom = fmaxf(cards_bottom, b.cy + (b.h * 0.5F));
            if (live && b.alpha > 0.5F &&
                mm_mouse_in(c, b.cx - (b.w * 0.5F), b.cy - (b.h * 0.5F), b.cx + (b.w * 0.5F),
                            b.cy + (b.h * 0.5F))) {
                c->hover = (int32_t)i;
            }
        }
        /* « Retour », sous les cartes */
        const float k = live ? a->emphasis[ENG_MENU_SLOTS]
                             : (m->selected == (int32_t)ENG_MENU_SLOTS ? 1.0F : 0.0F);
        const float size = c->px * (1.12F + (0.1F * k));
        const float rt = size * 0.22F;
        const float rw = mm_tracked_w(c->bold, size, "RETOUR", rt);
        const float rx = (c->w - rw) * 0.5F;
        const float ry =
            (c->h * 0.5F) +
            ((ENG_MOTION_THREAD_Y + 0.04F + (ENG_MOTION_CARD_H * 1.06F)) * a->fit * c->h) +
            (c->h * 0.05F);
        static const float soft[3] = {0.8F, 0.73F, 0.74F};
        static const float ivory[3] = {1.0F, 0.965F, 0.91F};
        const float alpha = presence * mm_smooth(0.25F, 0.6F, enter);
        if (c->hc && k > 0.5F) {
            ImDrawList_AddRectFilled(c->dl, (ImVec2_c){rx - (14.0F * c->u), ry - (4.0F * c->u)},
                                     (ImVec2_c){rx + rw + (14.0F * c->u), ry + (size * 1.3F)},
                                     mm_col(1.0F, 1.0F, 1.0F, 0.95F * alpha), 6.0F * c->u, 0);
            mm_tracked(c, c->bold, size, rx, ry, mm_col(0.0F, 0.0F, 0.0F, alpha), "RETOUR", rt,
                       0.0F);
        } else {
            mm_tracked(c, c->bold, size, rx, ry, mm_mix(soft, ivory, k, alpha), "RETOUR", rt, 1.0F);
            const ImVec2_c under[2] = {{rx, ry + (size * 1.34F)},
                                       {rx + (rw * k), ry + (size * 1.34F)}};
            mm_thread(c, under, 2, 2.0F * c->u, alpha * k);
        }
        if (live && mm_mouse_in(c, rx - (40.0F * c->u), ry - (size * 0.5F),
                                rx + rw + (40.0F * c->u), ry + (size * 1.9F))) {
            c->hover = (int32_t)ENG_MENU_SLOTS;
        }
        mm_hint(c, c->w * 0.5F, c->h - (c->px * 1.5F),
                "flèches : choisir   ·   Entrée : ouvrir   ·   Échap : retour", alpha);
        (void)cards_bottom;
        return;
    }
    /* la carte visée est à gauche ; son panneau à droite */
    const float x = panel_x(c);
    const float width = fminf(c->h * 0.6F, c->w - x - (c->h * 0.04F));
    const float top = thread_y + (c->h * 0.045F);
    if (!c->on_title || c->hc) { /* sans la scène du titre (qui s'assombrit seule) : un fond */
        ImDrawList_AddRectFilled(c->dl, (ImVec2_c){x - (24.0F * c->u), top - (20.0F * c->u)},
                                 (ImVec2_c){x + width + (24.0F * c->u), top + (c->h * 0.46F)},
                                 mm_col(0.03F, 0.022F, 0.055F, (c->hc ? 0.96F : 0.6F) * presence),
                                 14.0F * c->u, 0);
    }
    float y = panel_header(c, m, x, top, enter, presence);
    if (m->screen == ENG_MENU_NAME) {
        name_panel(c, m, x, y + (c->px * 0.2F), width, enter, presence, live);
        return;
    }
    int32_t danger = 3;
    if (m->screen == ENG_MENU_DELETE) { /* ce qui va être perdu */
        const EngMenuSlot *s =
            &m->slots[m->slot < 0 ? 0 : (m->slot >= (int32_t)ENG_MENU_SLOTS ? 0 : m->slot)];
        char line[96];
        (void)snprintf(line, sizeof line, "« %s »", s->name);
        const float size = mm_fit(c->bold, c->px * 1.3F, line, 0.0F, width);
        mm_text(c, c->bold, size, x, y, mm_ivory(presence), line, 1.0F);
        y += size * 1.5F;
        mm_text(c, c->font, c->px * 0.82F, x, y, mm_soft(0.9F * presence),
                "Cette partie sera retirée de l'emplacement.", 1.0F);
        y += c->px * 2.0F;
        danger = 1;
    }
    const MenuRows rows = {.x = x + (18.0F * c->u),
                           .y = y,
                           .size = c->px * 1.3F,
                           .row_h = c->px * 1.3F * 1.8F,
                           .width = width,
                           .enter = enter - 0.08F,
                           .presence = presence,
                           .live = live,
                           .danger = danger};
    const float bottom = mm_rows(c, items, n, m->selected, &rows);
    mm_hint(c, x + (width * 0.36F), bottom + (c->px * 0.5F),
            "flèches : choisir   ·   Entrée : valider   ·   Échap : retour", presence);
}

#else

typedef int eng_ui_menu_cards_unused; /* unité de traduction non vide sans OW_UI */

#endif
