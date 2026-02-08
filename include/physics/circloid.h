/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef CIRCLOIDS_H
#define CIRCLOIDS_H
#include <stddef.h>
#include "math/cvectors.h"
#include "colour/colour.h"
#include "physics/newton_object.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define NEW_CIRCLOID() allocate_block(sizeof(Circloid))
//#define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
    NewtonObject2d object;
    ColourRgba colourRgba;
    float radius;
} Circloid;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

//void *allocate_array(size_t element_count, size_t element_bytes);


#endif