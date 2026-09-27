#ifndef ENTITY_PREFAB_H
#define ENTITY_PREFAB_H

#include "entities/entity_factory.h"

// Load a supported JSON prefab into the creation parameters consumed by EntityFactory.
bool EntityPrefab_LoadFile(const char *file_path, EntityCreateParams *out_params);

#endif