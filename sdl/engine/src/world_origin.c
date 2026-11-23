#include "engine/world_origin.h"

#include <math.h>
#include <string.h>

#include "engine/world_cell.h"

void eng_origin_init(EngWorldOrigin *o, double rebase_distance)
{
    if (o == NULL) {
        return;
    }
    memset(o, 0, sizeof *o);
    o->rebase_distance = rebase_distance > 1.0 ? rebase_distance : 1.0;
}

bool eng_origin_update(EngWorldOrigin *o, const double p[3], float shift[3])
{
    if (shift != NULL) {
        shift[0] = shift[1] = shift[2] = 0.0F;
    }
    if (o == NULL || p == NULL || shift == NULL || !isfinite(p[0]) || !isfinite(p[2])) {
        return false;
    }
    const double dx = p[0] - o->x;
    const double dz = p[2] - o->z;
    if (fabs(dx) <= o->rebase_distance && fabs(dz) <= o->rebase_distance) {
        return false;
    }
    const double nx = floor(p[0] / ENG_CELL_SIZE + 0.5) * ENG_CELL_SIZE;
    const double nz = floor(p[2] / ENG_CELL_SIZE + 0.5) * ENG_CELL_SIZE;
    shift[0] = (float)(o->x - nx); /* exact : différence de multiples de 256 */
    shift[2] = (float)(o->z - nz);
    o->x = nx;
    o->z = nz;
    o->rebases++;
    return true;
}

void eng_origin_to_local(const EngWorldOrigin *o, const double world[3], float local[3])
{
    if (o == NULL || world == NULL || local == NULL) {
        return;
    }
    local[0] = (float)(world[0] - o->x);
    local[1] = (float)(world[1] - o->y);
    local[2] = (float)(world[2] - o->z);
}

void eng_origin_to_world(const EngWorldOrigin *o, const float local[3], double world[3])
{
    if (o == NULL || world == NULL || local == NULL) {
        return;
    }
    world[0] = o->x + (double)local[0];
    world[1] = o->y + (double)local[1];
    world[2] = o->z + (double)local[2];
}
