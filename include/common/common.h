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
#include "math/helpers.h"
#include "math/cmath.h"
#include "collections/dynamic_array.h"
#include "collections/linear_array.h"
#include "associations/flat_map.h"
#include <assert.h>

// Check if DEBUG is defined (usually passed by your compiler/IDE)
// ANSI Escape Codes for Colors (Works beautifully in modern terminals/cmd)
#define LOG_CLR_RESET   "\033[0m"
#define LOG_CLR_RED     "\033[1;31m"
#define LOG_CLR_YELLOW  "\033[1;33m"
#define LOG_CLR_GREEN   "\033[1;32m"

// --- ALWAYS ENABLED (Errors and Warnings should show in both Debug and Release) ---

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, LOG_CLR_RED "[ERROR] (%s:%d) " fmt LOG_CLR_RESET "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    fprintf(stderr, LOG_CLR_YELLOW "[WARN] (%s:%d) " fmt LOG_CLR_RESET "\n", __FILE__, __LINE__, ##__VA_ARGS__)


// --- DEBUG ONLY (Info/Logs disappear entirely in Release mode) ---
//#define DEBUG
#ifdef DEBUG
    #define LOG_INFO(fmt, ...) \
        printf(LOG_CLR_GREEN "[INFO] " fmt LOG_CLR_RESET "\n", ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...) ((void)0)
#endif

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
// Color Type and Constants
//----------------------------------------------------------------------------------
typedef union
{
    struct
    {
        unsigned char r, g, b, a;
    };
    unsigned int rgba; // Access the whole thing at once
} ColourRgba;

// Some Basic Colors
#define COLOURLESS_RGBA (ColourRgba){ 0, 0, 0, 0 }  
#define LIGHTGRAY_RGBA  (ColourRgba){ 200, 200, 200, 255 }   // Light Gray
#define GRAY_RGBA       (ColourRgba){ 130, 130, 130, 255 }   // Gray
#define DARKGRAY_RGBA   (ColourRgba){ 80, 80, 80, 255 }      // Dark Gray
#define YELLOW_RGBA     (ColourRgba){ 253, 249, 0, 255 }     // Yellow
#define GOLD_RGBA       (ColourRgba){ 255, 203, 0, 255 }     // Gold
#define ORANGE_RGBA     (ColourRgba){ 255, 161, 0, 255 }     // Orange
#define PINK_RGBA       (ColourRgba){ 255, 109, 194, 255 }   // Pink
#define RED_RGBA        (ColourRgba){ 230, 41, 55, 255 }     // Red
#define MAROON_RGBA     (ColourRgba){ 190, 33, 55, 255 }     // Maroon
#define GREEN_RGBA      (ColourRgba){ 0, 228, 48, 255 }      // Green
#define LIME_RGBA       (ColourRgba){ 0, 158, 47, 255 }      // Lime
#define DARKGREEN_RGBA  (ColourRgba){ 0, 117, 44, 255 }      // Dark Green
#define SKYBLUE_RGBA    (ColourRgba){ 102, 191, 255, 255 }   // Sky Blue
#define BLUE_RGBA       (ColourRgba){ 0, 121, 241, 255 }     // Blue
#define DARKBLUE_RGBA   (ColourRgba){ 0, 82, 172, 255 }      // Dark Blue
#define PURPLE_RGBA     (ColourRgba){ 200, 122, 255, 255 }   // Purple
#define VIOLET_RGBA     (ColourRgba){ 135, 60, 190, 255 }    // Violet
#define DARKPURPLE_RGBA (ColourRgba){ 112, 31, 126, 255 }    // Dark Purple
#define BEIGE_RGBA      (ColourRgba){ 211, 176, 131, 255 }   // Beige
#define BROWN_1_RGBA      (ColourRgba){ 127, 106, 79, 255 }    // Brown
#define BROWN_1_RGBA_2    (ColourRgba){ 127, 106, 79, 127 }    // Brown
#define BROWN_1_RGBA_3    (ColourRgba){ 127, 106, 79, 85 }    // Brown
#define BROWN_1_RGBA_4    (ColourRgba){ 127, 106, 79, 64 }    // Brown
#define BROWN_2_RGBA_1    (ColourRgba){150, 115, 70, 255}    // Brown
#define DARKBROWN_RGBA  (ColourRgba){ 76, 63, 47, 255 }      // Dark Brown
#define WHITE_RGBA      (ColourRgba){ 255, 255, 255, 255 }   // White
#define BLACK_RGBA      (ColourRgba){ 0, 0, 0, 255 }         // Black
#define BLANK_RGBA      (ColourRgba){ 0, 0, 0, 0 }           // Blank (Transparent)
#define MAGENTA_RGBA    (ColourRgba){ 255, 0, 255, 255 }     // Magenta
#define RED_ERROR_RGBA (ColourRgba){ 0, 0, 0, 255 }  
#define YELLOW_WARNING_RGBA (ColourRgba){ 255, 255, 176, 255 }

// OLIVE GARDEN
#define OLIVE_GARDEN_GREEN_XL (ColourRgba){ 96, 108, 56, 100 }
#define OLIVE_GARDEN_GREEN_L (ColourRgba){ 96, 108, 56, 255 }
#define OLIVE_GARDEN_GREEN_D (ColourRgba){ 40, 54, 24, 255 }
#define OLIVE_GARDEN_CREAM (ColourRgba){ 254, 250, 224, 255 }
#define OLIVE_GARDEN_TAN_L (ColourRgba){ 254, 250, 224, 255 }
#define OLIVE_GARDEN_TAN_D (ColourRgba){ 188, 108, 37, 255 }


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#endif
