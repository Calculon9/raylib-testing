#ifndef GEOMETRY_EDITOR_H
#define GEOMETRY_EDITOR_H

#include "input/pointer_input.h"
#include "world/world.h"

InputRouteResult UpdateGeometryEditor(World2d *world, const InputFrame *input);
void GeometryEditor_DrawHandles(const World2d *world, Camera2d *universe_camera);

#endif