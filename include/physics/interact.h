/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef INTERACT_H
#define INTERACT_H
#include "common/common.h"
#include "math/cvectors.h"
#include "collections/dynamic_array.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// #define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
// #define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// CREATe A RULE USING THE DEFINED ENUMS 
//----------------------------------------------------------------------------------
// FORMAT
// <...

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
enum Object{
    NEWTON_OBJECT,
};

enum Quantity{
    ALL,
    NONE
};

enum Possession{
    HAS,
};

enum ObjectRelation{
    IS_RELATED,
    NOT_RELATED,
    IN_CONTACT_WITH
};

enum Action{
    DO_NOTHING,
    INTERACT_WITH
};

enum Directionality{
    TO,
    FROM
};

// enum Interaction{
//     DO_NOTHING,
//     INTERACT_WITH
// };

enum Criteria{
    OVERLAPPING,
    ARE_RELATED
};

typedef struct Rule {
    Vector2d coordinates; // coordinates of the cell in world coordinates
    float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
} Rule;


// typedef struct CoordinateSpace {
//     Basis2d basis; // basis vectors representing the direction and length of one step to the right and down respectively
//     DynamicArray *lineSegments_u; // array of line segments representing the "horizontal" lines of the field (if applicable)
//     DynamicArray *lineSegments_v; // array of line segments representing the "vertical" lines of the field (if applicable)
//     DynamicArray *cells; // the cells or field units within the coordinate space (in linear form) of field units
//     float rows, columns; // number of rows and columns in the coordinate space
// } CoordinateSpace;



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// Field CreateField(Rectangloid object, int rows, int columns, ColourRgba lineColour, DynamicArray *items);
// Field CalculateFieldLines(Field field);
// Field InitialiseFieldCells(Field field);
// Vector2d GetCellIndicesFromCoordinates(Field field, Vector2d objectPos);
// Field UpdateFieldCellValues(Field field);
//void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif
