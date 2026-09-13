#ifndef ENTITY_INTERNAL_H
#define ENTITY_INTERNAL_H

#include "physics/newtonoid.h"

// Validate dimensions shared by every entity constructor.
bool Entity_ValidateDimensions(const Newtonoid2dParams *params);

#endif
