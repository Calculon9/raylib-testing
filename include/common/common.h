/**********************************************************************************************
*
COMMON MODULE
*
**********************************************************************************************/

#ifndef COMMON_H
#define COMMON_H

// 1. External dependencies
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "memory/cmemory.h"
#include "math/cvectors.h"
#include "collections/dynamic_array.h"
#include "collections/linear_array.h"

// 2. Global Math Constants
#ifndef PI
    #define PI 3.14159265358979323846f
#endif
#define EPSILON 0.00001f


//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
//#define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
//#define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct String32
{
    char string[32];
} String32;

typedef struct String64
{
    char string[64];
} String64;

typedef struct String128
{
    char string[128];
} String128;

typedef struct String256
{
    char string[256];
} String256;


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#endif