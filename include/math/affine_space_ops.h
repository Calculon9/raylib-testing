/**********************************************************************************************
*
AFFINE SPACE OPS MODULE
*
**********************************************************************************************/
#ifndef AFFINE_SPACE_OPS_H
#define AFFINE_SPACE_OPS_H

#include "math/coordinate_space.h"

Vector2d Frame_TransformPoint_ToParent(Vector2d local_point, const Frame2d *frame);
Vector2d Frame_TransformPoint_FromParent(Vector2d parent_point, const Frame2d *frame);
Vector2d Frame_CalcExtent_Local(const Frame2d *frame);
Vector2d Frame_GetTopLeft_Local(const Frame2d *frame);
Vector2d Frame_CalcTopLeft_InParent(const Frame2d *frame);
Matrix2x2 Frame_CalcExtents_InParent(const Frame2d *frame);
Matrix2x2 Frame_CalcAABB_InParent(const Frame2d *frame);
bool Frame_ContainsPoint_InParent(Vector2d parent_point, const Frame2d *frame);
bool Frame_ContainsPoint_Local(Vector2d local_point, const Frame2d *frame);
Matrix2x2 Frame_CalcAABB_Local(const Frame2d *frame);
Matrix3x3 Frame_CalcTransform(Frame2d source, Frame2d destination);
Matrix3x3 Frame_CalcChainTransform(Frame2d source, Frame2d middle, Frame2d destination);
Vector2d Frame_GetBasisScaling(Basis2d source, Basis2d destination);
// bool VectorIsInFrame_2d(Vector2d vector, const Frame2d *frame);
// Vector2d CalcSpaceHalfExtent(const Space2d *space);
// Vector2d CalcSpaceOriginFromCenter(const Space2d *space, Vector2d center);
// Matrix2x2 CalcSpaceBoundsFromCenter(const Space2d *space, Vector2d center);

#endif
