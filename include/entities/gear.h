#ifndef GEAR_ENTITY_H
#define GEAR_ENTITY_H

#include "physics/newtonoid.h"

// Create an allocated gear entity from the supplied creation parameters.
Newtonoid2d *GearEntity_Create(const Newtonoid2dParams *params);

#endif
