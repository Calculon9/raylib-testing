#include "input/input_system.h"

#include <stddef.h>

#include "editor/geometry_editor.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/universe_system.h"

typedef InputRouteResult (*InputRouteHandler)(const InputFrame *input, InputRouteResult prior_result);

static InputRouteResult RouteUIInput(const InputFrame *input, InputRouteResult prior_result)
{
    (void)prior_result;
    return UpdateUISystem(input);
}

static InputRouteResult RouteGeometryEditorInput(const InputFrame *input, InputRouteResult prior_result)
{
    if (prior_result != INPUT_ROUTE_IGNORED)
    {
        return prior_result;
    }

    // With an entity selected the context world is its owning world, which the editor edits within.
    return UpdateGeometryEditor(UIState_GetContextWorld(), input);
}

static InputRouteResult RouteUniverseInput(const InputFrame *input, InputRouteResult prior_result)
{
    return UpdateUniverseSystem(input, prior_result);
}

static InputRouteResult RouteWorldInput(const InputFrame *input, InputRouteResult prior_result)
{
    return UpdateWorldSystem(input, prior_result);
}

static const InputRouteHandler input_route_chain[] = {
    RouteUIInput,
    RouteGeometryEditorInput,
    RouteUniverseInput,
    RouteWorldInput,
};

InputRouteResult UpdateInputSystem(const InputFrame *input)
{
    if (!input)
    {
        return INPUT_ROUTE_IGNORED;
    }

    InputRouteResult result = INPUT_ROUTE_IGNORED;
    DragInteraction_BeginContextFrame(DRAG_CONTEXT_GAME, input);

    for (size_t i = 0; i < ARRAY_COUNT(input_route_chain); i++)
    {
        result = input_route_chain[i](input, result);
    }

    DragInteraction_EndContextFrame(DRAG_CONTEXT_GAME, input);
    return result;
}