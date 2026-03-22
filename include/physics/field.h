/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef FIELD_H
#define FIELD_H
#include <stddef.h>
#include "math/cvectors.h"
#include "collections/dynamic_array.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// #define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
// #define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------


typedef struct Field_Rect
{
    Rectangloid shape;  // shape
    Vector2d unit_vect; // resolution
    float row_units, column_units;
    DynamicArray *grid; // grid (in linear form) of field units, with values representing the field's properties at that unit (e.g., occupied, empty, etc.)
    DynamicArray *items; // objects that will be mapped to & interacting with the field
} Field_Rect;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Field_Rect CreateField_Rect(Rectangloid object, float unitWidth, float unitHeight, DynamicArray *items);
Field_Rect CalculateField_Rect(Field_Rect field);
// Rectangloid CreateRectangloid_FromObject(NewtonObject2d newtOb, float height, float width, ColourRgba colour);
void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif