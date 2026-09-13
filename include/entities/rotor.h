#ifndef ROTOR_ENTITY_H
#define ROTOR_ENTITY_H

#include "physics/newtonoid.h"

// Create an allocated rotor entity from the supplied creation parameters.
Newtonoid2d *RotorEntity_Create(const Newtonoid2dParams *params);

#endif
