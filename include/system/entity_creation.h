#ifndef ENTITY_CREATION_H
#define ENTITY_CREATION_H

#include "physics/newtonoid.h"

// Create an allocated Newtonoid from editor and command-queue parameters.
Newtonoid2d *CreateEntityFromParams(const Newtonoid2dParams *params);

#endif
