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
Matrix3x3 MtxTransform_CalcSiblingToSibling_Frame(Frame2d source, Frame2d destination);
Matrix3x3 MtxTransform_CalcSiblingToSibling_Mtx(Matrix3x3 source_mtx, Matrix3x3 destination_mtx);
Matrix3x3 MtxTransform_CalcChainToAncestor_Mtx(Matrix3x3 source_mtx, Matrix3x3 middle_mtx);
Matrix3x3 MtxTransform_CalcChainToAncestor_Frame(Frame2d source, Frame2d middle);
Vector2d Frame_GetBasisScaling(Basis2d source, Basis2d destination);
void Frame_CentreBounds(Frame2d *frame, Vector2d local_point);
Matrix3x3 MtxTransform_BuildLocalToParent(Vector2d origin_in_parent, float rotation_radians, Vector2d scale);
Matrix3x3 MtxTransform_GetLocalToParent(Frame2d frame);
Basis2d Basis_BuildFromRotationScale(float rotation_radians, Vector2d scale);
Basis2d Basis_BuildLocalToParent(Vector2d origin_in_parent, float rotation_radians, Vector2d scale);
Basis2d BasisTransform_CalcChainToAncestor_Basis(Basis2d source, Basis2d middle);
// bool VectorIsInFrame_2d(Vector2d vector, const Frame2d *frame);
// Vector2d CalcSpaceHalfExtent(const Space2d *space);
// Vector2d CalcSpaceOriginFromCenter(const Space2d *space, Vector2d center);
// Matrix2x2 CalcSpaceBoundsFromCenter(const Space2d *space, Vector2d center);

#endif
