#ifndef STANDARD_ENTITY_H
#define STANDARD_ENTITY_H

#include "physics/newtonoid.h"

// Create an allocated ordinary entity from the supplied creation parameters.
Newtonoid2d *StandardEntity_Create(const Newtonoid2dParams *params);

#endif
