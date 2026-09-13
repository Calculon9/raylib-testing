#include "entities/entity_internal.h"

// Validate dimensions shared by every entity constructor.
bool Entity_ValidateDimensions(const Newtonoid2dParams *params)
{
    if (!params || params->width <= 0.0f || params->height <= 0.0f)
    {
        if (params)
        {
            LOG_WARN("Invalid size: width = %f, height = %f\n",
                     params->width, params->height);
        }
        return false;
    }

    return true;
}

