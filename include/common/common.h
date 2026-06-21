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
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#endif