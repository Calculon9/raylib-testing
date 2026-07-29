/**********************************************************************************************
*
*   SYSTEM MANAGER MODULE - Phase 4 Architecture Cleanup
*
*   Formalizes system lifecycle management with explicit Init/Update/Draw/Shutdown phases.
*   Replaces implicit call chains with centralized system orchestration.
*
**********************************************************************************************/
#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// System lifecycle state
typedef enum {
    SYSTEM_STATE_UNINITIALIZED = 0,
    SYSTEM_STATE_INITIALIZED = 1,
    SYSTEM_STATE_RUNNING = 2,
    SYSTEM_STATE_SHUTDOWN = 3
} SystemState;

// System identifiers
typedef enum {
    SYSTEM_VIEWPORT = 0,
    SYSTEM_UNIVERSE = 1,
    SYSTEM_WORLD = 2,
    SYSTEM_UI = 3,
    SYSTEM_DEBUG_OVERLAY = 4,
    SYSTEM_COUNT = 5  // Total number of systems
} SystemID;

// System registration structure
typedef struct {
    SystemID id;
    const char *name;
    void (*init_fn)(void);
    void (*update_fn)(int mouse_x, int mouse_y);
    void (*draw_fn)(void);
    void (*shutdown_fn)(void);
    SystemState state;
} System;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Initialize the system manager itself
void InitSystemManager(void);

// Register a system with the manager
void RegisterSystem(SystemID id, const char *name,
                    void (*init_fn)(void),
                    void (*update_fn)(int, int),
                    void (*draw_fn)(void),
                    void (*shutdown_fn)(void));

// Lifecycle operations
void InitAllSystems(void);
void UpdateAllSystems(int mouse_x, int mouse_y);
void DrawAllSystems(void);
void ShutdownAllSystems(void);

// Query system state
SystemState GetSystemState(SystemID id);
bool IsSystemInitialized(SystemID id);

// Utility for debugging
void PrintSystemStatus(void);

#endif // SYSTEM_MANAGER_H
