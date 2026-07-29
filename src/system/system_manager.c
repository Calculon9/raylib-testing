/**********************************************************************************************
*
*   SYSTEM MANAGER MODULE IMPLEMENTATION - Phase 4 Architecture Cleanup
*
**********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include "system/system_manager.h"
#include "common/common.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

static System systems[SYSTEM_COUNT] = {0};
static bool manager_initialized = false;

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

void InitSystemManager(void)
{
    if (manager_initialized)
    {
        LOG_WARN("System manager already initialized\n");
        return;
    }

    // Initialize all system slots
    for (int i = 0; i < SYSTEM_COUNT; i++)
    {
        systems[i].id = (SystemID)i;
        systems[i].name = "Unregistered";
        systems[i].init_fn = NULL;
        systems[i].update_fn = NULL;
        systems[i].draw_fn = NULL;
        systems[i].shutdown_fn = NULL;
        systems[i].state = SYSTEM_STATE_UNINITIALIZED;
    }

    manager_initialized = true;
    LOG_INFO("System manager initialized\n");
}

void RegisterSystem(SystemID id, const char *name,
                    void (*init_fn)(void),
                    void (*update_fn)(int, int),
                    void (*draw_fn)(void),
                    void (*shutdown_fn)(void))
{
    if (!manager_initialized)
    {
        LOG_ERROR("System manager not initialized. Call InitSystemManager() first.\n");
        return;
    }

    if (id < 0 || id >= SYSTEM_COUNT)
    {
        LOG_ERROR("Invalid system ID %d\n", id);
        return;
    }

    systems[id].name = name;
    systems[id].init_fn = init_fn;
    systems[id].update_fn = update_fn;
    systems[id].draw_fn = draw_fn;
    systems[id].shutdown_fn = shutdown_fn;
    
    LOG_INFO("Registered system: %s (ID: %d)\n", name, id);
}

void InitAllSystems(void)
{
    if (!manager_initialized)
    {
        LOG_ERROR("System manager not initialized\n");
        return;
    }

    LOG_INFO("Initializing all systems...\n");
    
    for (int i = 0; i < SYSTEM_COUNT; i++)
    {
        System *sys = &systems[i];
        
        if (sys->init_fn != NULL)
        {
            LOG_INFO("  Initializing %s...\n", sys->name);
            sys->init_fn();
            sys->state = SYSTEM_STATE_INITIALIZED;
        }
    }
    
    LOG_INFO("All systems initialized successfully\n");
}

void UpdateAllSystems(int mouse_x, int mouse_y)
{
    for (int i = 0; i < SYSTEM_COUNT; i++)
    {
        System *sys = &systems[i];
        
        if (sys->update_fn != NULL && sys->state == SYSTEM_STATE_INITIALIZED)
        {
            sys->state = SYSTEM_STATE_RUNNING;
            sys->update_fn(mouse_x, mouse_y);
        }
    }
}

void DrawAllSystems(void)
{
    for (int i = 0; i < SYSTEM_COUNT; i++)
    {
        System *sys = &systems[i];
        
        if (sys->draw_fn != NULL && sys->state >= SYSTEM_STATE_INITIALIZED)
        {
            sys->draw_fn();
        }
    }
}

void ShutdownAllSystems(void)
{
    if (!manager_initialized)
        return;

    LOG_INFO("Shutting down all systems...\n");
    
    // Shutdown in reverse order
    for (int i = SYSTEM_COUNT - 1; i >= 0; i--)
    {
        System *sys = &systems[i];
        
        if (sys->shutdown_fn != NULL && sys->state >= SYSTEM_STATE_INITIALIZED)
        {
            LOG_INFO("  Shutting down %s...\n", sys->name);
            sys->shutdown_fn();
            sys->state = SYSTEM_STATE_SHUTDOWN;
        }
    }
    
    manager_initialized = false;
    LOG_INFO("All systems shut down successfully\n");
}

SystemState GetSystemState(SystemID id)
{
    if (id < 0 || id >= SYSTEM_COUNT)
        return SYSTEM_STATE_UNINITIALIZED;
    
    return systems[id].state;
}

bool IsSystemInitialized(SystemID id)
{
    return GetSystemState(id) >= SYSTEM_STATE_INITIALIZED;
}

void PrintSystemStatus(void)
{
    if (!manager_initialized)
    {
        printf("System manager not initialized\n");
        return;
    }

    printf("\n=== System Status ===\n");
    for (int i = 0; i < SYSTEM_COUNT; i++)
    {
        System *sys = &systems[i];
        const char *state_str = "UNKNOWN";
        
        switch (sys->state)
        {
            case SYSTEM_STATE_UNINITIALIZED: state_str = "UNINITIALIZED"; break;
            case SYSTEM_STATE_INITIALIZED: state_str = "INITIALIZED"; break;
            case SYSTEM_STATE_RUNNING: state_str = "RUNNING"; break;
            case SYSTEM_STATE_SHUTDOWN: state_str = "SHUTDOWN"; break;
        }
        
        printf("  [%d] %-20s : %s\n", i, sys->name, state_str);
    }
    printf("=====================\n\n");
}
