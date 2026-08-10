#include "input/input_system.h"

#include "editor/geometry_editor.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/universe_system.h"
#include "world/universe.h"

InputRouteResult UpdateInputSystem(const InputFrame *input)
{
    if (!input)
    {
        return INPUT_ROUTE_IGNORED;
    }

    InputRouteResult result = UpdateUISystem(input);
    DragInteractionState *drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    DragInteraction_BeginFrame(drag_ctx, input);

    World2d *active_world = Universe_GetSelectedWorld(&G_Universe);
    if (result == INPUT_ROUTE_IGNORED && active_world)
    {
        result = UpdateGeometryEditor(active_world, input);
    }
    result = UpdateUniverseSystem(input, result);
    result = UpdateWorldSystem(input, result);

    DragInteraction_EndFrame(drag_ctx, input);
    return result;
}