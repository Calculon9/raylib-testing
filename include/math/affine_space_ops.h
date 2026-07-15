/**********************************************************************************************
*
AFFINE SPACE OPS MODULE
*
**********************************************************************************************/
#ifndef AFFINE_SPACE_OPS_H
#define AFFINE_SPACE_OPS_H

#include "math/coordinate_space.h"

Vector2d TransformPoint_ToParent(Vector2d local_point, const Frame2d *frame);
Vector2d TransformPoint_FromParent(Vector2d parent_point, const Frame2d *frame);
Vector2d CalcFrameExtent_Local(const Frame2d *frame);
Vector2d GetFrameTopLeft_Local(const Frame2d *frame);
Vector2d CalcFrameTopLeft_InParent(const Frame2d *frame);
Matrix2x2 CalcFrameExtents_InParent(const Frame2d *frame);
Matrix2x2 CalcFrameAABB_InParent(const Frame2d *frame);
bool FrameContainsPoint_FromParent(Vector2d parent_point, const Frame2d *frame);
bool FrameContainsPoint_Local(Vector2d local_point, const Frame2d *frame);

// bool VectorIsInFrame_2d(Vector2d vector, const Frame2d *frame);
// Vector2d CalcSpaceHalfExtent(const Space2d *space);
// Vector2d CalcSpaceOriginFromCenter(const Space2d *space, Vector2d center);
// Matrix2x2 CalcSpaceBoundsFromCenter(const Space2d *space, Vector2d center);

#endif
