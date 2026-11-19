#include "engine/daynight.h"

#include <math.h>
#include <stdio.h>

static double wrap_hour(double h)
{
    double r = fmod(h, 24.0);
    if (r < 0.0) {
        r += 24.0;
    }
    return r >= 24.0 ? 0.0 : r;
}

void eng_daynight_init(EngDayNight *d, double start_hour)
{
    if (d == NULL) {
        return;
    }
    d->hour = isfinite(start_hour) ? wrap_hour(start_hour) : 12.0;
    d->day_seconds = ENG_DAYNIGHT_DAY_SECONDS;
    d->speed = 1.0F;
    d->max_elevation_deg = 62.0F;
    d->night_exposure_ev = 1.5F;
}

void eng_daynight_tick(EngDayNight *d, double dt)
{
    if (d == NULL || !isfinite(dt) || dt <= 0.0 || !(d->day_seconds > 0.0F) ||
        !isfinite(d->speed)) {
        return;
    }
    double hours = dt * (double)d->speed * 24.0 / (double)d->day_seconds;
    if (hours > 24.0) {
        hours = fmod(hours, 24.0);
    }
    d->hour = wrap_hour(d->hour + hours);
}

bool eng_daynight_set_hour(EngDayNight *d, double hour)
{
    if (d == NULL || !isfinite(hour)) {
        return false;
    }
    d->hour = wrap_hour(hour);
    return true;
}

void eng_daynight_sun(const EngDayNight *d, float *azimuth_deg, float *elevation_deg)
{
    const double h = d != NULL ? d->hour : 12.0;
    const double max_el = d != NULL ? (double)d->max_elevation_deg : 62.0;
    /* 6 h : est (+X, azimut 90°), midi : sud (-Z, 180°), 18 h : ouest (-X, 270° = -90°) */
    double az = 90.0 + (h - 6.0) * 15.0;
    az = fmod(az + 180.0, 360.0);
    if (az < 0.0) {
        az += 360.0;
    }
    az -= 180.0;
    const double el = max_el * sin((h - 6.0) * (3.14159265358979323846 / 12.0));
    if (azimuth_deg != NULL) {
        *azimuth_deg = (float)az;
    }
    if (elevation_deg != NULL) {
        *elevation_deg = (float)el;
    }
}

float eng_daynight_night_weight(float sun_elevation_deg)
{
    if (!isfinite(sun_elevation_deg)) {
        return 0.0F;
    }
    float t = (2.0F - sun_elevation_deg) / 12.0F; /* +2° -> 0, -10° -> 1 */
    t = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
    return t * t * (3.0F - 2.0F * t);
}

float eng_daynight_exposure_bias(const EngDayNight *d, float sun_elevation_deg)
{
    const float ev = d != NULL && isfinite(d->night_exposure_ev) && d->night_exposure_ev > 0.0F
                         ? d->night_exposure_ev
                         : 0.0F;
    return ev * eng_daynight_night_weight(sun_elevation_deg);
}

size_t eng_daynight_format(const EngDayNight *d, char *out, size_t capacity)
{
    if (d == NULL || out == NULL || capacity < 6U) {
        return 0U;
    }
    const int minutes = (int)floor(d->hour * 60.0) % (24 * 60);
    const int n = snprintf(out, capacity, "%02d:%02d", minutes / 60, minutes % 60);
    return n > 0 ? (size_t)n : 0U;
}
