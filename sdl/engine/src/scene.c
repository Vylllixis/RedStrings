#include "engine/scene.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"

/* ── Écriture ── */

typedef struct Out {
    char *data;
    size_t capacity;
    size_t length; /* octets demandés jusqu'ici (peut dépasser capacity) */
    bool ok;
} Out;

static void put(Out *out, const char *text, size_t length)
{
    if (out->data != NULL && out->length < out->capacity) {
        const size_t room = out->capacity - out->length;
        memcpy(out->data + out->length, text, length < room ? length : room);
    }
    out->length += length;
}

static void put_text(Out *out, const char *text)
{
    put(out, text, strlen(text));
}

static void put_format(Out *out, const char *format, ...) SDL_PRINTF_VARARG_FUNC(2);
static void put_format(Out *out, const char *format, ...)
{
    char buffer[64];
    va_list args;
    va_start(args, format);
    /* Faux positif de clang-tidy 18 (le va_list est initialisé par va_start juste au-dessus). */
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    const int n = vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if (n > 0 && (size_t)n < sizeof buffer) {
        put(out, buffer, (size_t)n);
    } else {
        out->ok = false;
    }
}

static void put_float(Out *out, float value)
{
    if (!isfinite(value)) {
        ENG_LOG_ERROR("scène : valeur non finie, impossible à écrire");
        out->ok = false;
        return;
    }
    put_format(out, "%.9g", (double)value);
}

static void put_string(Out *out, const char *text, uint32_t capacity)
{
    put(out, "\"", 1);
    for (uint32_t i = 0; i < capacity && text[i] != '\0'; i++) {
        const unsigned char c = (unsigned char)text[i];
        switch (c) {
        case '"':
            put_text(out, "\\\"");
            break;
        case '\\':
            put_text(out, "\\\\");
            break;
        case '\n':
            put_text(out, "\\n");
            break;
        case '\r':
            put_text(out, "\\r");
            break;
        case '\t':
            put_text(out, "\\t");
            break;
        default:
            if (c < 0x20U || c == 0x7FU) {
                put_format(out, "\\x%02X", (unsigned)c);
            } else {
                put(out, &text[i], 1);
            }
        }
    }
    put(out, "\"", 1);
}

static void put_field(Out *out, const EngFieldDesc *field, const uint8_t *data)
{
    const uint8_t *value = data + field->offset;
    put_text(out, field->name);
    put_text(out, " = ");
    switch (field->type) {
    case ENG_FIELD_BOOL:
        put_text(out, *value != 0 ? "true" : "false");
        break;
    case ENG_FIELD_I32: {
        int32_t v;
        memcpy(&v, value, sizeof v);
        put_format(out, "%d", (int)v);
        break;
    }
    case ENG_FIELD_U32: {
        uint32_t v;
        memcpy(&v, value, sizeof v);
        put_format(out, "%u", (unsigned)v);
        break;
    }
    case ENG_FIELD_FLOAT:
    case ENG_FIELD_VEC3:
    case ENG_FIELD_QUAT: {
        const uint32_t count =
            field->type == ENG_FIELD_FLOAT ? 1U : (field->type == ENG_FIELD_VEC3 ? 3U : 4U);
        for (uint32_t i = 0; i < count; i++) {
            float v;
            memcpy(&v, value + ((size_t)i * sizeof v), sizeof v);
            if (i > 0) {
                put(out, " ", 1);
            }
            put_float(out, v);
        }
        break;
    }
    case ENG_FIELD_STRING:
        put_string(out, (const char *)value, field->length);
        break;
    }
    put(out, "\n", 1);
}

bool eng_scene_write(const EngWorld *world, char *out, size_t capacity, size_t *needed)
{
    Out o = {.data = out, .capacity = capacity > 0 ? capacity - 1U : 0, .ok = true};
    put_text(&o, ENG_SCENE_MAGIC " ");
    put_format(&o, "%u\n", (unsigned)ENG_SCENE_VERSION);

    /* Numéro de scène de chaque entité, par case : affecté dans l'ordre du parcours. */
    uint32_t *ids = SDL_calloc(world->capacity, sizeof *ids);
    if (ids == NULL) {
        ENG_LOG_ERROR("scène : mémoire insuffisante pour écrire");
        return false;
    }
    uint32_t next_id = 0;
    uint32_t cursor = 0;
    EngEntity root;
    while (eng_world_next_entity(world, &cursor, &root)) {
        if (eng_world_parent(world, root).generation != 0) {
            continue;
        }
        for (EngEntity e = root; e.generation != 0; e = eng_world_next_in_subtree(world, root, e)) {
            ids[e.index] = next_id++;
            const EngEntity parent = eng_world_parent(world, e);
            put(&o, "\n", 1);
            put_format(&o, "entity %u parent ", (unsigned)ids[e.index]);
            if (parent.generation != 0) {
                put_format(&o, "%u", (unsigned)ids[parent.index]);
            } else {
                put(&o, "-", 1);
            }
            put(&o, "\n", 1);
            for (uint32_t c = 1; c <= eng_world_component_count(world); c++) {
                const EngComponentDesc *desc = eng_world_component(world, c);
                const uint8_t *data = eng_world_get(world, e, c);
                if (desc->field_count == 0 || data == NULL) {
                    continue;
                }
                put_text(&o, "[");
                put_text(&o, desc->name);
                put_text(&o, "]\n");
                for (uint32_t f = 0; f < desc->field_count; f++) {
                    put_field(&o, &desc->fields[f], data);
                }
            }
        }
    }
    SDL_free(ids);
    if (o.data != NULL && capacity > 0) {
        o.data[o.length < o.capacity ? o.length : o.capacity] = '\0';
    }
    *needed = o.length + 1U;
    return o.ok;
}

/* ── Lecture ── */

typedef struct Reader {
    EngWorld *world;
    EngSceneError *error;
    EngEntity *entities; /* créées par cette lecture, dans l'ordre du fichier */
    int64_t *parents;    /* numéro du parent de chacune, -1 : racine */
    uint32_t *lines;     /* ligne de déclaration de chacune */
    uint32_t count;
    uint32_t reserved;
} Reader;

static bool fail(const Reader *reader, uint32_t line, const char *format, ...)
    SDL_PRINTF_VARARG_FUNC(3);
static bool fail(const Reader *reader, uint32_t line, const char *format, ...)
{
    if (reader->error != NULL) {
        va_list args;
        va_start(args, format);
        /* Faux positif de clang-tidy 18 (le va_list est initialisé par va_start juste au-dessus).
         */
        // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
        (void)vsnprintf(reader->error->message, sizeof reader->error->message, format, args);
        va_end(args);
        reader->error->line = line;
    }
    return false;
}

static bool reserve(Reader *reader)
{
    if (reader->count < reader->reserved) {
        return true;
    }
    const uint32_t wanted = reader->reserved == 0 ? 64U : reader->reserved * 2U;
    EngEntity *entities = SDL_realloc(reader->entities, (size_t)wanted * sizeof *entities);
    if (entities != NULL) {
        reader->entities = entities;
    }
    int64_t *parents =
        entities != NULL ? SDL_realloc(reader->parents, (size_t)wanted * sizeof *parents) : NULL;
    if (parents != NULL) {
        reader->parents = parents;
    }
    uint32_t *lines =
        parents != NULL ? SDL_realloc(reader->lines, (size_t)wanted * sizeof *lines) : NULL;
    if (lines != NULL) {
        reader->lines = lines;
        reader->reserved = wanted;
    }
    return lines != NULL;
}

/* Découpe `text` (modifié) en jetons séparés par espaces/tabulations ; rend le nombre de jetons (au
 * plus `max`, +1 si plus). */
static uint32_t split(char *text, char **tokens, uint32_t max)
{
    uint32_t count = 0;
    char *p = text;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            *p++ = '\0';
        }
        if (*p == '\0') {
            break;
        }
        if (count < max) {
            tokens[count] = p;
        }
        count++;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
    }
    return count;
}

static bool parse_float(const char *token, float *out)
{
    char *end = NULL;
    const double wide = strtod(token, &end);
    const float narrow = (float)wide; /* hors de la plage d'un float : devient infini */
    if (end == token || *end != '\0' || !isfinite(wide) || !isfinite(narrow)) {
        return false;
    }
    *out = narrow;
    return true;
}

static bool parse_integer(const char *token, bool is_signed, int64_t *out)
{
    if (is_signed ? false : token[0] == '-') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    const long long v = strtoll(token, &end, 10);
    if (end == token || *end != '\0' || errno != 0) {
        return false;
    }
    *out = v;
    return true;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/* `value` commence par un guillemet. Écrit la chaîne dans `out` (`capacity` octets, zéro compris).
 * Rend NULL si valide, sinon un message. */
static const char *parse_string(const char *value, char *out, uint32_t capacity)
{
    uint32_t length = 0;
    const char *p = value + 1;
    while (*p != '"') {
        unsigned char c = (unsigned char)*p;
        if (c == '\0') {
            return "chaîne non terminée (guillemet fermant manquant)";
        }
        p++;
        if (c == '\\') {
            const char escape = *p++;
            switch (escape) {
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case '"':
            case '\\':
                c = (unsigned char)escape;
                break;
            case 'x': {
                const int hi = hex_digit(p[0]);
                const int lo = hi >= 0 ? hex_digit(p[1]) : -1;
                if (lo < 0) {
                    return "séquence \\x invalide (deux chiffres hexadécimaux attendus)";
                }
                c = (unsigned char)((hi << 4) | lo);
                p += 2;
                if (c == 0) {
                    return "caractère nul interdit dans une chaîne";
                }
                break;
            }
            default:
                return "séquence d'échappement inconnue";
            }
        }
        if (length + 1U >= capacity) {
            return "chaîne trop longue pour ce champ";
        }
        out[length++] = (char)c;
    }
    out[length] = '\0';
    for (p++; *p == ' ' || *p == '\t'; p++) {
    }
    return *p == '\0' ? NULL : "texte après la chaîne";
}

/* `value` : texte après le « = » (espaces de tête déjà retirés). NULL si valide, sinon un message.
 */
static const char *parse_value(const EngFieldDesc *field, char *value, uint8_t *data)
{
    uint8_t *target = data + field->offset;
    if (field->type == ENG_FIELD_STRING) {
        if (value[0] != '"') {
            return "une chaîne entre guillemets est attendue";
        }
        char *buffer = SDL_malloc(field->length);
        if (buffer == NULL) {
            return "mémoire insuffisante";
        }
        const char *message = parse_string(value, buffer, field->length);
        if (message == NULL) {
            memset(target, 0, field->length);
            memcpy(target, buffer, strlen(buffer));
        }
        SDL_free(buffer);
        return message;
    }
    char *tokens[4];
    const uint32_t wanted =
        field->type == ENG_FIELD_VEC3 ? 3U : (field->type == ENG_FIELD_QUAT ? 4U : 1U);
    const uint32_t count = split(value, tokens, 4);
    if (count != wanted) {
        return wanted == 1U ? "une seule valeur est attendue"
                            : (wanted == 3U ? "trois valeurs sont attendues"
                                            : "quatre valeurs sont attendues");
    }
    switch (field->type) {
    case ENG_FIELD_BOOL:
        if (strcmp(tokens[0], "true") == 0) {
            *target = 1;
        } else if (strcmp(tokens[0], "false") == 0) {
            *target = 0;
        } else {
            return "« true » ou « false » est attendu";
        }
        return NULL;
    case ENG_FIELD_I32: {
        int64_t v;
        if (!parse_integer(tokens[0], true, &v) || v < INT32_MIN || v > INT32_MAX) {
            return "entier 32 bits signé invalide";
        }
        const int32_t narrow = (int32_t)v;
        memcpy(target, &narrow, sizeof narrow);
        return NULL;
    }
    case ENG_FIELD_U32: {
        int64_t v;
        if (!parse_integer(tokens[0], false, &v) || v < 0 || v > (int64_t)UINT32_MAX) {
            return "entier 32 bits non signé invalide";
        }
        const uint32_t narrow = (uint32_t)v;
        memcpy(target, &narrow, sizeof narrow);
        return NULL;
    }
    case ENG_FIELD_FLOAT:
    case ENG_FIELD_VEC3:
    case ENG_FIELD_QUAT: {
        float parsed[4];
        for (uint32_t i = 0; i < wanted; i++) {
            if (!parse_float(tokens[i], &parsed[i])) {
                return "nombre invalide (fini et représentable en float attendu)";
            }
        }
        memcpy(target, parsed, (size_t)wanted * sizeof(float));
        return NULL;
    }
    case ENG_FIELD_STRING:
        break;
    }
    return "type de champ inconnu";
}

static void rollback(Reader *reader)
{
    for (uint32_t i = 0; i < reader->count; i++) {
        if (eng_world_alive(reader->world, reader->entities[i])) {
            (void)eng_world_destroy(reader->world, reader->entities[i]);
        }
    }
}

/* Traite une ligne (déjà copiée, sans fin de ligne). `state` : 0 = pas d'entête, 1 = entête lu. */
typedef struct LineState {
    const EngComponentDesc *component; /* composant en cours */
    uint8_t *data;                     /* ses données dans l'entité courante */
    uint32_t seen;                     /* champs déjà lus (bits) */
    uint32_t component_id;
    bool header;
} LineState;

static bool handle_line(Reader *reader, LineState *st, char *line, uint32_t number)
{
    char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '#') {
        return true;
    }
    if (!st->header) {
        char *tokens[3];
        if (split(p, tokens, 3) != 2 || strcmp(tokens[0], ENG_SCENE_MAGIC) != 0) {
            return fail(reader, number, "entête attendu : « " ENG_SCENE_MAGIC " %u »",
                        (unsigned)ENG_SCENE_VERSION);
        }
        int64_t version;
        if (!parse_integer(tokens[1], false, &version) || version != (int64_t)ENG_SCENE_VERSION) {
            return fail(reader, number, "version de scène non gérée « %s » (celle-ci gère %u)",
                        tokens[1], (unsigned)ENG_SCENE_VERSION);
        }
        st->header = true;
        return true;
    }
    if (strncmp(p, "entity", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        char *tokens[5];
        int64_t id;
        int64_t parent = -1;
        if (split(p, tokens, 5) != 4 || strcmp(tokens[2], "parent") != 0 ||
            !parse_integer(tokens[1], false, &id)) {
            return fail(reader, number, "« entity N parent P » attendu (P : numéro ou « - »)");
        }
        if (id != (int64_t)reader->count) {
            return fail(reader, number, "numéro d'entité %lld, %u attendu (numéros consécutifs)",
                        (long long)id, (unsigned)reader->count);
        }
        if (strcmp(tokens[3], "-") != 0 && (!parse_integer(tokens[3], false, &parent))) {
            return fail(reader, number, "parent « %s » invalide", tokens[3]);
        }
        if (!reserve(reader)) {
            return fail(reader, number, "mémoire insuffisante");
        }
        const EngEntity entity = eng_world_create(reader->world, NULL);
        if (entity.generation == 0) {
            return fail(reader, number, "monde plein (capacité %u)",
                        (unsigned)reader->world->capacity);
        }
        reader->entities[reader->count] = entity;
        reader->parents[reader->count] = parent;
        reader->lines[reader->count] = number;
        reader->count++;
        st->component = NULL;
        return true;
    }
    if (*p == '[') {
        char *close = strchr(p, ']');
        if (close == NULL || close[1] != '\0') {
            return fail(reader, number, "« [Composant] » attendu");
        }
        if (reader->count == 0) {
            return fail(reader, number, "composant avant la première entité");
        }
        *close = '\0';
        const uint32_t id = eng_world_find_component(reader->world, p + 1);
        if (id == 0) {
            return fail(reader, number, "composant inconnu « %s »", p + 1);
        }
        const EngEntity entity = reader->entities[reader->count - 1U];
        if (eng_world_has(reader->world, entity, id)) {
            return fail(reader, number, "composant « %s » en double dans cette entité", p + 1);
        }
        st->component = eng_world_component(reader->world, id);
        st->component_id = id;
        st->data = eng_world_add(reader->world, entity, id);
        st->seen = 0;
        if (st->data == NULL) {
            return fail(reader, number, "composant « %s » impossible à ajouter", p + 1);
        }
        return true;
    }
    if (st->component == NULL) {
        return fail(reader, number, "ligne inattendue (« entity » ou « [Composant] » attendu)");
    }
    char *equal = strchr(p, '=');
    if (equal == NULL) {
        return fail(reader, number, "« champ = valeur » attendu");
    }
    char *name_end = equal;
    while (name_end > p && (name_end[-1] == ' ' || name_end[-1] == '\t')) {
        name_end--;
    }
    *name_end = '\0';
    char *value = equal + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    /* un composant a pu être déplacé par un ajout : on relit ses données */
    const EngEntity entity = reader->entities[reader->count - 1U];
    st->data = eng_world_get(reader->world, entity, st->component_id);
    for (uint32_t f = 0; f < st->component->field_count; f++) {
        const EngFieldDesc *field = &st->component->fields[f];
        if (strcmp(field->name, p) != 0) {
            continue;
        }
        if ((st->seen & (1U << f)) != 0) {
            return fail(reader, number, "champ « %s » en double", p);
        }
        const char *message = parse_value(field, value, st->data);
        if (message != NULL) {
            return fail(reader, number, "%s.%s : %s", st->component->name, p, message);
        }
        st->seen |= 1U << f;
        return true;
    }
    return fail(reader, number, "champ inconnu « %s » dans %s", p, st->component->name);
}

bool eng_scene_read(EngWorld *world, const char *text, size_t length, EngSceneError *error)
{
    if (error != NULL) {
        *error = (EngSceneError){0};
    }
    Reader reader = {.world = world, .error = error};
    LineState st = {0};
    bool ok = true;
    uint32_t number = 0;
    size_t pos = 0;
    while (ok && pos < length) {
        size_t end = pos;
        while (end < length && text[end] != '\n') {
            end++;
        }
        size_t line_length = end - pos;
        if (line_length > 0 && text[pos + line_length - 1U] == '\r') {
            line_length--;
        }
        number++;
        if (line_length >= ENG_SCENE_MAX_LINE) {
            ok = fail(&reader, number, "ligne trop longue (%u caractères au plus)",
                      ENG_SCENE_MAX_LINE - 1U);
        } else {
            char line[ENG_SCENE_MAX_LINE];
            memcpy(line, text + pos, line_length);
            line[line_length] = '\0';
            ok = handle_line(&reader, &st, line, number);
        }
        pos = end + 1U;
    }
    if (ok && !st.header) {
        ok = fail(&reader, number, "scène vide : entête « " ENG_SCENE_MAGIC " %u » manquant",
                  (unsigned)ENG_SCENE_VERSION);
    }
    for (uint32_t i = 0; ok && i < reader.count; i++) {
        if (reader.parents[i] < 0) {
            continue;
        }
        if (reader.parents[i] >= (int64_t)reader.count) {
            ok = fail(&reader, reader.lines[i], "parent %lld inexistant",
                      (long long)reader.parents[i]);
        } else if (!eng_world_set_parent(world, reader.entities[i],
                                         reader.entities[reader.parents[i]])) {
            ok = fail(&reader, reader.lines[i],
                      "parent %lld refusé (boucle ou hiérarchie trop profonde)",
                      (long long)reader.parents[i]);
        }
    }
    if (!ok) {
        rollback(&reader);
        if (error != NULL) {
            ENG_LOG_WARN("scène : ligne %u : %s", (unsigned)error->line, error->message);
        }
    }
    SDL_free(reader.entities);
    SDL_free(reader.parents);
    SDL_free(reader.lines);
    return ok;
}

/* ── Fichiers ── */

bool eng_scene_save(const EngWorld *world, const char *path)
{
    char resolved[512];
    char temporary[520];
    if (!eng_path_resolve(resolved, sizeof resolved, path) ||
        SDL_snprintf(temporary, sizeof temporary, "%s.tmp", resolved) >= (int)sizeof temporary) {
        ENG_LOG_ERROR("scène : chemin « %s » trop long", path);
        return false;
    }
    size_t needed = 0;
    if (!eng_scene_write(world, NULL, 0, &needed)) {
        return false;
    }
    char *text = SDL_malloc(needed);
    if (text == NULL) {
        ENG_LOG_ERROR("scène : mémoire insuffisante (%zu octets)", needed);
        return false;
    }
    bool ok = eng_scene_write(world, text, needed, &needed);
    SDL_IOStream *stream = ok ? SDL_IOFromFile(temporary, "wb") : NULL;
    if (stream == NULL) {
        if (ok) {
            ENG_LOG_ERROR("scène : écriture de %s impossible : %s", temporary, SDL_GetError());
        }
        ok = false;
    } else {
        const bool written = SDL_WriteIO(stream, text, needed - 1U) == needed - 1U;
        const bool closed = SDL_CloseIO(stream);
        ok = written && closed;
        if (!ok) {
            ENG_LOG_ERROR("scène : écriture de %s incomplète", temporary);
        }
    }
    SDL_free(text);
    if (ok && !SDL_RenamePath(temporary, resolved)) {
        ENG_LOG_ERROR("scène : renommage vers %s impossible : %s", resolved, SDL_GetError());
        ok = false;
    }
    if (!ok) {
        (void)SDL_RemovePath(temporary);
    }
    return ok;
}

bool eng_scene_load(EngWorld *world, const char *path, EngSceneError *error)
{
    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        if (error != NULL) {
            *error = (EngSceneError){0};
            SDL_snprintf(error->message, sizeof error->message, "chemin trop long (%zu caractères)",
                         strlen(path));
        }
        return false;
    }
    size_t size = 0;
    char *text = SDL_LoadFile(resolved, &size);
    if (text == NULL) {
        if (error != NULL) {
            *error = (EngSceneError){0};
            SDL_snprintf(error->message, sizeof error->message, "lecture de %s impossible : %s",
                         resolved, SDL_GetError());
        }
        return false;
    }
    const bool ok = eng_scene_read(world, text, size, error);
    SDL_free(text);
    return ok;
}
