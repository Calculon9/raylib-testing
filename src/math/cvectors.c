#include "common/common.h"
#include "memory/cmemory.h"
#include "math/cvectors.h"

// Sum all Vector2 in a contiguous array, return by value (Stack)
Vector2d VectorSumArray_2d(Vector2d *array, size_t count)
{
    Vector2d result = {0.0f, 0.0f};

    if (array == NULL)
        return result;

    for (size_t i = 0; i < count; i++)
    {
        result.x += array[i].x; // Use . because it's a contiguous array of structs
        result.y += array[i].y;
    }

    return result;
}

Vector2d VectorScale_2d(Vector2d vector, float scalar)
{
    Vector2d result = {0.0f, 0.0f};
    result.x = scalar * vector.x; // Use . because it's a contiguous array of structs
    result.y = scalar * vector.y;
    return result;
}

float VectorMagnitude_2d(Vector2d vector)
{
    return sqrtf((vector.x * vector.x) + (vector.y * vector.y));
}

float VectorRadians_2d(Vector2d vector)
{
    return atan2f(vector.y, vector.x);
}

Polar2d PolarForm_2d(Vector2d vector)
{
    Polar2d polar = {0.0f};
    polar.magnitude = sqrtf((vector.x * vector.x) + (vector.y * vector.y));
    polar.radians = atan2f(vector.y, vector.x);
    return polar;
}

// Result > 0: The vectors are pointing in the same general direction (angle < pi/2).
// Result = 0: The vectors are perfectly perpendicular (pi/2).
// Result < 0: The vectors are pointing away from each other (angle > pi/2).
float Vector_2d_Dot(Vector2d a, Vector2d b)
{
    return (a.x * b.x) + (a.y * b.y);
}

// Basis2d BasisTransform_2d(Basis2d basis_to_change, Transform scalar)
// {
//     Vector2d result = {0.0f, 0.0f};
//     result.x = scalar * vector.x; // Use . because it's a contiguous array of structs
//     result.y = scalar * vector.y;
//     return result;
// }

// Sum all Vector2 in a contiguous array, return dynamic allocation (Heap)
Vector2d *VectorSumArray_2d_Dynamic(Vector2d *array, size_t count)
{
    // We only need to allocate ONE Vector2 to hold the result, not a whole array!
    Vector2d *presult = AllocateArray(1, sizeof(Vector2d));

    // Calculate the sum using our stack function to avoid duplicating logic
    *presult = VectorSumArray_2d(array, count);

    return presult;
}

// Sum all Vector3 in the array, return by value (Stack).
Vector3d VectorSumArray_3d(Vector3d *array, size_t count)
{
    Vector3d result = {0.0f, 0.0f, 0.0f};

    if (array == NULL)
        return result;

    for (size_t i = 0; i < count; i++)
    {
        result.x += array[i].x; // Use . because it's a contiguous array of structs
        result.y += array[i].y;
        result.z += array[i].z;
    }

    return result;
};

// Sum all Vector3 in a contiguous array, return dynamic allocation (Heap)
Vector3d *VectorSumArray_3d_Dynamic(Vector3d *array, size_t count)
{
    // We only need to allocate ONE Vector3 to hold the result, not a whole array!
    Vector3d *presult = AllocateArray(1, sizeof(Vector3d));

    // Calculate the sum using our stack function to avoid duplicating logic
    *presult = VectorSumArray_3d(array, count);

    return presult;
}

Matrix3x3 MatrixMultiply_3x3_3x3(Matrix3x3 A, Matrix3x3 B) {
    Matrix3x3 result = { 0 };

    // Row 1
    result.m0 = A.m0*B.m0 + A.m3*B.m1 + A.m6*B.m2;
    result.m3 = A.m0*B.m3 + A.m3*B.m4 + A.m6*B.m5;
    result.m6 = A.m0*B.m6 + A.m3*B.m7 + A.m6*B.m8;

    // Row 2
    result.m1 = A.m1*B.m0 + A.m4*B.m1 + A.m7*B.m2;
    result.m4 = A.m1*B.m3 + A.m4*B.m4 + A.m7*B.m5;
    result.m7 = A.m1*B.m6 + A.m4*B.m7 + A.m7*B.m8;

    // Row 3
    result.m2 = A.m2*B.m0 + A.m5*B.m1 + A.m8*B.m2;
    result.m5 = A.m2*B.m3 + A.m5*B.m4 + A.m8*B.m5;
    result.m8 = A.m2*B.m6 + A.m5*B.m7 + A.m8*B.m8;

    return result;
}

Matrix3x3 MatrixInvert_3x3(Matrix3x3 M) {
    // 1. Calculate the Determinant
    float det = M.m0 * (M.m4 * M.m8 - M.m5 * M.m7) -
                M.m3 * (M.m1 * M.m8 - M.m2 * M.m7) +
                M.m6 * (M.m1 * M.m5 - M.m2 * M.m4);

    if (det == 0.0f) return (Matrix3x3){ 0 }; // Cannot invert

    float invDet = 1.0f / det;
    Matrix3x3 res;

    // 2. Calculate the Adjugate Matrix (Cofactors Transposed) / Det
    res.m0 = (M.m4 * M.m8 - M.m5 * M.m7) * invDet;
    res.m3 = (M.m6 * M.m5 - M.m3 * M.m8) * invDet;
    res.m6 = (M.m3 * M.m7 - M.m6 * M.m4) * invDet;

    res.m1 = (M.m2 * M.m7 - M.m1 * M.m8) * invDet;
    res.m4 = (M.m0 * M.m8 - M.m2 * M.m6) * invDet;
    res.m7 = (M.m6 * M.m1 - M.m0 * M.m7) * invDet;

    res.m2 = (M.m1 * M.m5 - M.m2 * M.m4) * invDet;
    res.m5 = (M.m2 * M.m3 - M.m0 * M.m5) * invDet;
    res.m8 = (M.m0 * M.m4 - M.m1 * M.m3) * invDet;

    return res;
}


Matrix3x3 BasisTransform_2d(Basis2d source, Basis2d destination, Vector2d destination_origin)
{
    // matSource stays the same (usually 0,0 for world origin)
    Matrix3x3 matSource = {
        source.u.x, source.v.x, 0,
        source.u.y, source.v.y, 0,
        0,          0,          1
    };

    Matrix3x3 invSource = MatrixInvert_3x3(matSource);

    // matDest NEEDS the origin in the third column (m6, m7)
    Matrix3x3 matDest = {
        destination.u.x,   destination.v.x,   destination_origin.x, // <--- HERE
        destination.u.y,   destination.v.y,   destination_origin.y, // <--- HERE
        0,          0,          1
    };

    // Matrix Multiplication order is also vital:
    // Usually: Result = Dest * invSource
    return MatrixMultiply_3x3_3x3(matDest, invSource);
}

// Matrix3x3 BasisTransform_Scale_Rotate_2d(Basis2d source_basis, Basis2d dest_basis)// scale_u, float scale_v, float radians_u, float radians_v)
// {
//     Matrix3x3 mat;
//     // Get the angles of world and screen basis
//     float source_basis_u_rad = VectorRadians_2d(source_basis.u);
//     float source_basis_v_rad = VectorRadians_2d(source_basis.v);
//     float dest_basis_u_rad = VectorRadians_2d(dest_basis.u);
//     float dest_basis_v_rad = VectorRadians_2d(dest_basis.v);

//     // Get the scaling factor to go from world basis magnitude to screen basis magnitude. 
//     float source_basis_u_mag = VectorMagnitude_2d(source_basis.u);
//     float source_basis_v_mag = VectorMagnitude_2d(source_basis.v);
//     float dest_basis_u_mag = VectorMagnitude_2d(dest_basis.u);
//     float dest_basis_v_mag = VectorMagnitude_2d(dest_basis.v);
//     float scale_u = dest_basis_u_mag / source_basis_u_mag;
//     float scale_v = dest_basis_v_mag / source_basis_v_mag;

//     // Basis U: The "X-axis" of your world
//     // How much of the screen's X and Y does 1 unit of World-U cover?
//     mat.m0 = (cosf(source_basis_u_rad) - cosf(dest_basis_u_rad)) * scale_u; // BasisU.x
//     mat.m1 = (sinf(source_basis_u_rad) - sinf(dest_basis_u_rad)) * scale_u; // BasisU.y

//     // Basis V: The "Y-axis" of your world
//     // How much of the screen's X and Y does 1 unit of World-V cover?
//     mat.m3 = (cosf(source_basis_v_rad) - cosf(dest_basis_v_rad)) * scale_v;  // BasisV.x
//     mat.m4 = (sinf(source_basis_v_rad) - sinf(dest_basis_v_rad)) * scale_v; // BasisV.y

//     // Translation: Where World (0,0) is on the screen
//     mat.m6 = 0;
//     mat.m7 = 0;

//     // Standard affine bottom row
//     mat.m2 = 0;
//     mat.m5 = 0;
//     mat.m8 = 1;

//     return mat;
// }