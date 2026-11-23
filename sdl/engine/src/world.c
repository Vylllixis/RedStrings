#include "engine/world.h"

#include <SDL3/SDL_stdinc.h>
#include <stddef.h>
#include <string.h>

#include "engine/log.h"

#define WORLD_MAX_CAPACITY (1U << 20)

/* ── Description ── */

uint32_t eng_field_size(const EngFieldDesc *field)
{
    switch (field->type) {
    case ENG_FIELD_BOOL:
        return 1U;
    case ENG_FIELD_I32:
    case ENG_FIELD_U32:
    case ENG_FIELD_FLOAT:
        return 4U;
    case ENG_FIELD_VEC3:
        return 12U;
    case ENG_FIELD_QUAT:
        return 16U;
    case ENG_FIELD_STRING:
        return field->length;
    }
    return 0U;
}

static uint32_t field_alignment(EngFieldType type)
{
    switch (type) {
    case ENG_FIELD_BOOL:
    case ENG_FIELD_STRING:
        return 1U;
    case ENG_FIELD_I32:
    case ENG_FIELD_U32:
    case ENG_FIELD_FLOAT:
    case ENG_FIELD_VEC3:
    case ENG_FIELD_QUAT:
        return 4U;
    }
    return 1U;
}

static bool validate_desc(const EngWorld *world, const EngComponentDesc *desc)
{
    if (desc == NULL || desc->name == NULL || desc->name[0] == '\0') {
        ENG_LOG_ERROR("monde : composant sans nom");
        return false;
    }
    if (desc->size == 0 || desc->size > ENG_WORLD_MAX_COMPONENT_SIZE) {
        ENG_LOG_ERROR("monde : composant « %s » : taille %u invalide (1 à %u)", desc->name,
                      (unsigned)desc->size, (unsigned)ENG_WORLD_MAX_COMPONENT_SIZE);
        return false;
    }
    if (eng_world_find_component(world, desc->name) != 0) {
        ENG_LOG_ERROR("monde : composant « %s » déjà enregistré", desc->name);
        return false;
    }
    if (desc->field_count > ENG_WORLD_MAX_FIELDS ||
        (desc->field_count > 0 && desc->fields == NULL)) {
        ENG_LOG_ERROR("monde : composant « %s » : liste de champs invalide", desc->name);
        return false;
    }
    for (uint32_t i = 0; i < desc->field_count; i++) {
        const EngFieldDesc *field = &desc->fields[i];
        const uint32_t size = eng_field_size(field);
        if (field->name == NULL || field->name[0] == '\0' || size == 0) {
            ENG_LOG_ERROR(
                "monde : composant « %s » : champ %u sans nom, de type inconnu ou sans taille",
                desc->name, (unsigned)i);
            return false;
        }
        if (field->type != ENG_FIELD_STRING && field->length != 0) {
            ENG_LOG_ERROR("monde : « %s.%s » : `length` ne sert qu'aux chaînes", desc->name,
                          field->name);
            return false;
        }
        if (field->offset > desc->size || size > desc->size - field->offset) {
            ENG_LOG_ERROR("monde : « %s.%s » dépasse la structure (%u octets)", desc->name,
                          field->name, (unsigned)desc->size);
            return false;
        }
        if (field->offset % field_alignment(field->type) != 0) {
            ENG_LOG_ERROR("monde : « %s.%s » mal aligné", desc->name, field->name);
            return false;
        }
        for (uint32_t j = 0; j < i; j++) {
            if (strcmp(desc->fields[j].name, field->name) == 0) {
                ENG_LOG_ERROR("monde : « %s » : champ « %s » répété", desc->name, field->name);
                return false;
            }
        }
    }
    return true;
}

/* ── Tables ── */

static EngComponentTable *table_of(const EngWorld *world, uint32_t component)
{
    if (component == 0 || component > world->component_count) {
        return NULL;
    }
    return (EngComponentTable *)&world->tables[component - 1U];
}

static void free_table(EngComponentTable *table)
{
    SDL_free(table->sparse);
    SDL_free(table->entities);
    SDL_free(table->data);
    *table = (EngComponentTable){0};
}

uint32_t eng_world_register(EngWorld *world, const EngComponentDesc *desc)
{
    if (world->entities.items == NULL) {
        ENG_LOG_ERROR("monde : non initialisé");
        return 0;
    }
    if (world->component_count >= ENG_WORLD_MAX_COMPONENTS) {
        ENG_LOG_ERROR("monde : trop de composants (%u)", (unsigned)ENG_WORLD_MAX_COMPONENTS);
        return 0;
    }
    if (!validate_desc(world, desc)) {
        return 0;
    }
    EngComponentTable *table = &world->tables[world->component_count];
    *table = (EngComponentTable){.desc = *desc};
    table->stride = (desc->size + 7U) & ~7U;
    table->sparse = SDL_calloc(world->capacity, sizeof *table->sparse);
    table->entities = SDL_calloc(world->capacity, sizeof *table->entities);
    table->data = SDL_calloc(world->capacity, table->stride);
    if (table->sparse == NULL || table->entities == NULL || table->data == NULL) {
        ENG_LOG_ERROR("monde : mémoire insuffisante pour le composant « %s »", desc->name);
        free_table(table);
        return 0;
    }
    return ++world->component_count;
}

uint32_t eng_world_component_count(const EngWorld *world)
{
    return world->component_count;
}

const EngComponentDesc *eng_world_component(const EngWorld *world, uint32_t component)
{
    const EngComponentTable *table = table_of(world, component);
    return table != NULL ? &table->desc : NULL;
}

uint32_t eng_world_find_component(const EngWorld *world, const char *name)
{
    if (name == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < world->component_count; i++) {
        if (strcmp(world->tables[i].desc.name, name) == 0) {
            return i + 1U;
        }
    }
    return 0;
}

/* ── Composants natifs ── */

void eng_transform_matrix(const EngTransform *transform, mat4 out)
{
    /* Les champs du composant ne sont pas alignés sur 16 octets (rotation est à l'offset 12) : cglm
     * les lit en SSE aligné, donc on passe par des copies locales alignées. */
    vec3 position;
    vec3 scale;
    versor raw;
    versor rotation;
    SDL_memcpy(position, transform->position, sizeof position);
    SDL_memcpy(scale, transform->scale, sizeof scale);
    SDL_memcpy(raw, transform->rotation, sizeof raw);
    glm_quat_normalize_to(raw, rotation);
    glm_mat4_identity(out);
    glm_translate(out, position);
    glm_quat_rotate(out, rotation, out);
    glm_scale(out, scale);
}

static const EngFieldDesc NAME_FIELDS[] = {
    {"text", ENG_FIELD_STRING, offsetof(EngName, text), ENG_NAME_MAX},
};

static const EngFieldDesc TRANSFORM_FIELDS[] = {
    {"position", ENG_FIELD_VEC3, offsetof(EngTransform, position), 0},
    {"rotation", ENG_FIELD_QUAT, offsetof(EngTransform, rotation), 0},
    {"scale", ENG_FIELD_VEC3, offsetof(EngTransform, scale), 0},
};

static const EngFieldDesc MESH_RENDERER_FIELDS[] = {
    {"model", ENG_FIELD_STRING, offsetof(EngMeshRenderer, model), ENG_MESH_PATH_MAX},
    {"visible", ENG_FIELD_BOOL, offsetof(EngMeshRenderer, visible), 0},
    {"cast_shadow", ENG_FIELD_BOOL, offsetof(EngMeshRenderer, cast_shadow), 0},
};

static const EngTransform TRANSFORM_INITIAL = {.position = {0.0F, 0.0F, 0.0F},
                                               .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                               .scale = {1.0F, 1.0F, 1.0F}};

static const EngMeshRenderer MESH_RENDERER_INITIAL = {.visible = true, .cast_shadow = true};

static bool register_builtins(EngWorld *world)
{
    const EngComponentDesc name = {
        .name = "Name", .size = sizeof(EngName), .fields = NAME_FIELDS, .field_count = 1};
    const EngComponentDesc transform = {.name = "Transform",
                                        .size = sizeof(EngTransform),
                                        .fields = TRANSFORM_FIELDS,
                                        .field_count = 3,
                                        .initial = &TRANSFORM_INITIAL};
    const EngComponentDesc mesh = {.name = "MeshRenderer",
                                   .size = sizeof(EngMeshRenderer),
                                   .fields = MESH_RENDERER_FIELDS,
                                   .field_count = 3,
                                   .initial = &MESH_RENDERER_INITIAL};
    return eng_world_register(world, &name) == ENG_COMPONENT_NAME &&
           eng_world_register(world, &transform) == ENG_COMPONENT_TRANSFORM &&
           eng_world_register(world, &mesh) == ENG_COMPONENT_MESH_RENDERER;
}

/* ── Monde ── */

bool eng_world_init(EngWorld *world, uint32_t capacity)
{
    *world = (EngWorld){0};
    if (capacity == 0 || capacity > WORLD_MAX_CAPACITY) {
        ENG_LOG_ERROR("monde : capacité %u invalide (1 à %u)", (unsigned)capacity,
                      (unsigned)WORLD_MAX_CAPACITY);
        return false;
    }
    if (!eng_pool_init(&world->entities, NULL, sizeof(EngEntityRecord), capacity)) {
        return false;
    }
    world->capacity = capacity;
    if (!register_builtins(world)) {
        eng_world_shutdown(world);
        return false;
    }
    return true;
}

void eng_world_shutdown(EngWorld *world)
{
    for (uint32_t i = 0; i < world->component_count; i++) {
        free_table(&world->tables[i]);
    }
    if (world->entities.items != NULL) {
        eng_pool_shutdown(&world->entities);
    }
    *world = (EngWorld){0};
}

static EngEntityRecord *record_of(const EngWorld *world, EngEntity entity)
{
    return world->entities.items != NULL ? eng_pool_get(&world->entities, entity) : NULL;
}

bool eng_world_alive(const EngWorld *world, EngEntity entity)
{
    return record_of(world, entity) != NULL;
}

uint32_t eng_world_entity_count(const EngWorld *world)
{
    return world->entities.items != NULL ? eng_pool_count(&world->entities) : 0;
}

bool eng_world_next_entity(const EngWorld *world, uint32_t *cursor, EngEntity *out)
{
    return world->entities.items != NULL && eng_pool_next(&world->entities, cursor, out);
}

/* ── Composants d'une entité ── */

static uint8_t *element(const EngComponentTable *table, uint32_t dense)
{
    return table->data + ((size_t)dense * table->stride);
}

void *eng_world_get(const EngWorld *world, EngEntity entity, uint32_t component)
{
    const EngComponentTable *table = table_of(world, component);
    if (table == NULL || record_of(world, entity) == NULL) {
        return NULL;
    }
    const uint32_t slot = table->sparse[entity.index];
    return slot != 0 ? element(table, slot - 1U) : NULL;
}

bool eng_world_has(const EngWorld *world, EngEntity entity, uint32_t component)
{
    return eng_world_get(world, entity, component) != NULL;
}

void *eng_world_add(EngWorld *world, EngEntity entity, uint32_t component)
{
    EngComponentTable *table = table_of(world, component);
    if (table == NULL) {
        ENG_LOG_ERROR("monde : composant %u inconnu", (unsigned)component);
        return NULL;
    }
    if (record_of(world, entity) == NULL) {
        ENG_LOG_WARN("monde : ajout de « %s » à une entité invalide ou détruite", table->desc.name);
        return NULL;
    }
    uint32_t slot = table->sparse[entity.index];
    if (slot != 0) {
        return element(table, slot - 1U);
    }
    const uint32_t dense = table->count++;
    table->sparse[entity.index] = dense + 1U;
    table->entities[dense] = entity;
    uint8_t *data = element(table, dense);
    memset(data, 0, table->stride);
    if (table->desc.initial != NULL) {
        memcpy(data, table->desc.initial, table->desc.size);
    }
    return data;
}

bool eng_world_remove(EngWorld *world, EngEntity entity, uint32_t component)
{
    EngComponentTable *table = table_of(world, component);
    if (table == NULL || record_of(world, entity) == NULL) {
        return false;
    }
    const uint32_t slot = table->sparse[entity.index];
    if (slot == 0) {
        return false;
    }
    const uint32_t hole = slot - 1U;
    const uint32_t last = table->count - 1U;
    if (hole != last) { /* le dernier élément prend la place libérée */
        memcpy(element(table, hole), element(table, last), table->stride);
        table->entities[hole] = table->entities[last];
        table->sparse[table->entities[hole].index] = hole + 1U;
    }
    table->sparse[entity.index] = 0;
    table->count--;
    return true;
}

uint32_t eng_world_table_count(const EngWorld *world, uint32_t component)
{
    const EngComponentTable *table = table_of(world, component);
    return table != NULL ? table->count : 0;
}

EngEntity eng_world_table_entity(const EngWorld *world, uint32_t component, uint32_t index)
{
    const EngComponentTable *table = table_of(world, component);
    return table != NULL && index < table->count ? table->entities[index] : ENG_ENTITY_NULL;
}

void *eng_world_table_data(const EngWorld *world, uint32_t component, uint32_t index)
{
    const EngComponentTable *table = table_of(world, component);
    return table != NULL && index < table->count ? element(table, index) : NULL;
}

EngEntity eng_world_find_by_name(const EngWorld *world, const char *name)
{
    if (name == NULL) {
        return ENG_ENTITY_NULL;
    }
    const EngComponentTable *table = table_of(world, ENG_COMPONENT_NAME);
    for (uint32_t i = 0; table != NULL && i < table->count; i++) {
        const EngName *candidate = (const EngName *)element(table, i);
        if (strncmp(candidate->text, name, ENG_NAME_MAX) == 0) {
            return table->entities[i];
        }
    }
    return ENG_ENTITY_NULL;
}

EngEntity eng_world_create(EngWorld *world, const char *name)
{
    EngEntity entity;
    EngEntityRecord *record =
        world->entities.items != NULL ? eng_pool_alloc(&world->entities, &entity) : NULL;
    if (record == NULL) {
        ENG_LOG_ERROR("monde : plein (%u entités)", (unsigned)world->capacity);
        return ENG_ENTITY_NULL;
    }
    if (name != NULL) {
        EngName *component = eng_world_add(world, entity, ENG_COMPONENT_NAME);
        if (component != NULL) {
            SDL_strlcpy(component->text, name, sizeof component->text);
        }
    }
    return entity;
}

/* ── Hiérarchie ── */

EngEntity eng_world_parent(const EngWorld *world, EngEntity entity)
{
    const EngEntityRecord *record = record_of(world, entity);
    return record != NULL ? record->parent : ENG_ENTITY_NULL;
}

EngEntity eng_world_first_child(const EngWorld *world, EngEntity entity)
{
    const EngEntityRecord *record = record_of(world, entity);
    return record != NULL ? record->first_child : ENG_ENTITY_NULL;
}

EngEntity eng_world_next_sibling(const EngWorld *world, EngEntity entity)
{
    const EngEntityRecord *record = record_of(world, entity);
    return record != NULL ? record->next_sibling : ENG_ENTITY_NULL;
}

uint32_t eng_world_depth(const EngWorld *world, EngEntity entity)
{
    const EngEntityRecord *record = record_of(world, entity);
    return record != NULL ? record->depth : 0;
}

static bool same_entity(EngEntity a, EngEntity b)
{
    return a.index == b.index && a.generation == b.generation;
}

/* Retire `entity` de la liste d'enfants de son parent (sans toucher à `entity.parent`). */
static void unlink_from_parent(EngWorld *world, EngEntity entity, EngEntityRecord *record)
{
    EngEntityRecord *parent = record_of(world, record->parent);
    EngEntityRecord *previous = record_of(world, record->prev_sibling);
    EngEntityRecord *next = record_of(world, record->next_sibling);
    if (previous != NULL) {
        previous->next_sibling = record->next_sibling;
    } else if (parent != NULL) {
        parent->first_child = record->next_sibling;
    }
    if (next != NULL) {
        next->prev_sibling = record->prev_sibling;
    } else if (parent != NULL) {
        parent->last_child = record->prev_sibling;
    }
    (void)entity;
    record->parent = ENG_ENTITY_NULL;
    record->prev_sibling = ENG_ENTITY_NULL;
    record->next_sibling = ENG_ENTITY_NULL;
}

EngEntity eng_world_next_in_subtree(const EngWorld *world, EngEntity root, EngEntity current)
{
    if (record_of(world, root) == NULL) {
        return ENG_ENTITY_NULL;
    }
    bool inside = false;
    for (EngEntity up = current; record_of(world, up) != NULL && !inside;
         up = record_of(world, up)->parent) {
        inside = same_entity(up, root);
    }
    if (!inside) {
        return ENG_ENTITY_NULL;
    }
    const EngEntityRecord *record = record_of(world, current);
    if (record_of(world, record->first_child) != NULL) {
        return record->first_child;
    }
    while (!same_entity(current, root)) {
        record = record_of(world, current);
        if (record_of(world, record->next_sibling) != NULL) {
            return record->next_sibling;
        }
        current = record->parent;
    }
    return ENG_ENTITY_NULL;
}

/* Hauteur du sous-arbre de `entity` : 0 pour une feuille (les profondeurs enregistrées sont à
 * jour). */
static uint32_t subtree_height(const EngWorld *world, EngEntity entity)
{
    const uint32_t base = record_of(world, entity)->depth;
    uint32_t height = 0;
    for (EngEntity e = entity; e.generation != 0; e = eng_world_next_in_subtree(world, entity, e)) {
        const uint32_t h = record_of(world, e)->depth - base;
        height = h > height ? h : height;
    }
    return height;
}

/* Recalcule les profondeurs du sous-arbre de `entity`, dont la racine vaut `depth`. */
static void set_depths(EngWorld *world, EngEntity entity, uint32_t depth)
{
    record_of(world, entity)->depth = depth;
    for (EngEntity e = eng_world_next_in_subtree(world, entity, entity); e.generation != 0;
         e = eng_world_next_in_subtree(world, entity, e)) {
        EngEntityRecord *record = record_of(world, e);
        record->depth = record_of(world, record->parent)->depth + 1U;
    }
}

bool eng_world_set_parent(EngWorld *world, EngEntity child, EngEntity parent)
{
    EngEntityRecord *child_record = record_of(world, child);
    if (child_record == NULL) {
        ENG_LOG_WARN("monde : set_parent sur une entité invalide ou détruite");
        return false;
    }
    EngEntityRecord *parent_record = NULL;
    if (parent.generation != 0) {
        parent_record = record_of(world, parent);
        if (parent_record == NULL) {
            ENG_LOG_WARN("monde : parent invalide ou détruit");
            return false;
        }
        for (EngEntity up = parent; record_of(world, up) != NULL;
             up = record_of(world, up)->parent) {
            if (same_entity(up, child)) {
                ENG_LOG_WARN("monde : set_parent créerait une boucle");
                return false;
            }
        }
        if (parent_record->depth + 1U + subtree_height(world, child) >= ENG_WORLD_MAX_DEPTH) {
            ENG_LOG_WARN("monde : hiérarchie trop profonde (%u niveaux au plus)",
                         (unsigned)ENG_WORLD_MAX_DEPTH);
            return false;
        }
    }
    if (child_record->parent.generation != 0) {
        unlink_from_parent(world, child, child_record);
    }
    if (parent_record != NULL) {
        child_record->parent = parent;
        child_record->prev_sibling = parent_record->last_child;
        EngEntityRecord *tail = record_of(world, parent_record->last_child);
        if (tail != NULL) {
            tail->next_sibling = child;
        } else {
            parent_record->first_child = child;
        }
        parent_record->last_child = child;
    }
    set_depths(world, child, parent_record != NULL ? parent_record->depth + 1U : 0U);
    return true;
}

/* Détruit `entity` et son sous-arbre : on détruit une feuille à la fois (la première trouvée en
 * descendant par les premiers enfants), la racine en dernier. Au plus profondeur × entités pas. */
static void destroy_subtree(EngWorld *world, EngEntity root)
{
    for (bool done = false; !done;) {
        EngEntity leaf = root;
        while (record_of(world, record_of(world, leaf)->first_child) != NULL) {
            leaf = record_of(world, leaf)->first_child;
        }
        done = same_entity(leaf, root);
        EngEntityRecord *record = record_of(world, leaf);
        if (record->parent.generation != 0) {
            unlink_from_parent(world, leaf, record);
        }
        for (uint32_t c = 1; c <= world->component_count; c++) {
            (void)eng_world_remove(world, leaf, c);
        }
        (void)eng_pool_free(&world->entities, leaf);
    }
}

bool eng_world_destroy(EngWorld *world, EngEntity entity)
{
    if (record_of(world, entity) == NULL) {
        ENG_LOG_WARN("monde : destruction d'une entité invalide ou déjà détruite");
        return false;
    }
    destroy_subtree(world, entity);
    return true;
}

bool eng_world_world_matrix(const EngWorld *world, EngEntity entity, mat4 out)
{
    glm_mat4_identity(out);
    if (record_of(world, entity) == NULL) {
        return false;
    }
    EngEntity chain[ENG_WORLD_MAX_DEPTH];
    uint32_t length = 0;
    for (EngEntity up = entity; record_of(world, up) != NULL && length < ENG_WORLD_MAX_DEPTH;
         up = record_of(world, up)->parent) {
        chain[length++] = up;
    }
    while (length > 0) {
        const EngTransform *local = eng_world_get(world, chain[--length], ENG_COMPONENT_TRANSFORM);
        if (local != NULL) {
            mat4 matrix;
            eng_transform_matrix(local, matrix);
            glm_mat4_mul(out, matrix, out);
        }
    }
    return true;
}
