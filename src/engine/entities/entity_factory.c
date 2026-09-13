#include "entities/entity_factory.h"
#include "entities/gear.h"
#include "entities/portal.h"
#include "entities/rotor.h"
#include "entities/standard.h"

// Select the entity constructor associated with the requested preset.
bool EntityFactory_Create(const EntityCreateParams *params, EntityCreateResult *out_result)
{
    if (!params || !out_result)
    {
        return false;
    }

    *out_result = (EntityCreateResult){0};

    switch (params->preset)
    {
    case ENTITY_PRESET_STANDARD:
        out_result->entity = StandardEntity_Create(&params->physics);
        break;
    case ENTITY_PRESET_ROTOR:
        out_result->entity = RotorEntity_Create(&params->physics);
        break;
    case ENTITY_PRESET_GEAR:
        out_result->entity = GearEntity_Create(&params->physics);
        break;
    case ENTITY_PRESET_PORTAL:
        out_result->entity = PortalEntity_Create(
            &params->physics, &out_result->component.data.portal);
        if (out_result->entity)
        {
            out_result->component.type = ENTITY_COMPONENT_PORTAL;
        }
        break;
    default:
        break;
    }

    return out_result->entity != NULL;
}
