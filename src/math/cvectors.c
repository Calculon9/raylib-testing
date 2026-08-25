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

Vector2d VectorSum_2d(Vector2d a, Vector2d b)
{
    Vector2d result = {0.0f, 0.0f};
    result.x = a.x + b.x; // Use . because it's a contiguous array of structs
    result.y = a.y + b.y;
    return result;
}

Vector2d VectorScale_2d(Vector2d vector, float scalar)
{
    Vector2d result = {0.0f, 0.0f};
    result.x = scalar * vector.x; // Use . because it's a contiguous array of structs
    result.y = scalar * vector.y;
    return result;
}

float VectorDistanceSquared_2d(Vector2d a, Vector2d b)
{
    Vector2d difference = VectorDiff_2d(a, b);
    return (difference.x * difference.x) + (difference.y * difference.y);
}

bool VectorWithinDistance_2d(Vector2d a, Vector2d b, float max_distance)
{
    return VectorDistanceSquared_2d(a, b) <= max_distance * max_distance;
}

float VectorMagnitude_2d(Vector2d vector)
{
    // Euclidean length: |v| = sqrt(v.x^2 + v.y^2). This is used to turn a
    // velocity or basis vector into a scalar speed/scale.
    return sqrtf((vector.x * vector.x) + (vector.y * vector.y));
}

float VectorRadians_2d(Vector2d vector)
{
    // atan2 preserves the vector's quadrant, producing the heading angle from
    // the positive X axis rather than losing sign information through atan(y/x).
    return atan2f(vector.y, vector.x);
}

Vector2d VectorComponents_2d(float magnitude, float radians)
{
    // Convert polar coordinates back to Cartesian components:
    // x = r * cos(theta), y = r * sin(theta).
    Vector2d result = {0.0f, 0.0f};
    result.x = magnitude * cosf(radians);
    result.y = magnitude * sinf(radians);
    return result;
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
float VectorDot_2d(Vector2d a, Vector2d b)
{
    // The dot product is |a| * |b| * cos(theta); with a unit b it gives the
    // signed component of a along b, which is why collision code uses it for
    // projection and approach-speed tests.
    return (a.x * b.x) + (a.y * b.y);
}

// float VectorDot_2d(Polar2d a, Polar2d b)
// {
//     return (a.x * b.x) + (a.y * b.y);
// }

// Matrix2x2 MatrixRotate90_2x2(Matrix2x2 M)
// {
//     // 
//     float 
// }

// Basis2d BasisTransform_2d(Basis2d basis_to_change, Transform scalar)
// {
//     Vector2d result = {0.0f, 0.0f};
//     result.x = scalar * vector.x; // Use . because it's a contiguous array of structs
//     result.y = scalar * vector.y;
//     return result;
// }

// Sum all Vector2 in a contiguous array, return dynamic allocation (Heap)
// Vector2d *VectorSumArray_2d_Dynamic(Vector2d *array, size_t count)
// {
//     // We only need to allocate ONE Vector2 to hold the result, not a whole array!
//     Vector2d *presult = AllocateArray(1, sizeof(Vector2d));

//     // Calculate the sum using our stack function to avoid duplicating logic
//     *presult = VectorSumArray_2d(array, count);

//     return presult;
// }

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

Vector2d VectorMultiply_2d(Vector2d a, Vector2d b)
{
    Vector2d result = {0.0f, 0.0f};
    result.x = a.x * b.x; // Use . because it's a contiguous array of structs
    result.y = a.y * b.y;
    return result;
}

// Sum all Vector3 in a contiguous array, return dynamic allocation (Heap)
// Vector3d *VectorSumArray_3d_Dynamic(Vector3d *array, size_t count)
// {
//     // We only need to allocate ONE Vector3 to hold the result, not a whole array!
//     Vector3d *presult = AllocateArray(1, sizeof(Vector3d));

//     // Calculate the sum using our stack function to avoid duplicating logic
//     *presult = VectorSumArray_3d(array, count);

//     return presult;
// }

Matrix3x3 MatrixMultiply_3x3_3x3(Matrix3x3 A, Matrix3x3 B)
{
    Matrix3x3 result = {0};

    // Column 1 = A * B.col1
    result.col1.x = (A.col1.x * B.col1.x) + (A.col2.x * B.col1.y) + (A.col3.x * B.col1.z);
    result.col1.y = (A.col1.y * B.col1.x) + (A.col2.y * B.col1.y) + (A.col3.y * B.col1.z);
    result.col1.z = (A.col1.z * B.col1.x) + (A.col2.z * B.col1.y) + (A.col3.z * B.col1.z);

    // Column 2 = A * B.col2
    result.col2.x = (A.col1.x * B.col2.x) + (A.col2.x * B.col2.y) + (A.col3.x * B.col2.z);
    result.col2.y = (A.col1.y * B.col2.x) + (A.col2.y * B.col2.y) + (A.col3.y * B.col2.z);
    result.col2.z = (A.col1.z * B.col2.x) + (A.col2.z * B.col2.y) + (A.col3.z * B.col2.z);

    // Column 3 = A * B.col3
    result.col3.x = (A.col1.x * B.col3.x) + (A.col2.x * B.col3.y) + (A.col3.x * B.col3.z);
    result.col3.y = (A.col1.y * B.col3.x) + (A.col2.y * B.col3.y) + (A.col3.y * B.col3.z);
    result.col3.z = (A.col1.z * B.col3.x) + (A.col2.z * B.col3.y) + (A.col3.z * B.col3.z);

    return result;
}

Matrix2x2 MatrixMultiply_2x2_2x2(Matrix2x2 A, Matrix2x2 B)
{
    Matrix2x2 result = {0};

    // Column 1 = A * B.col1
    result.col1.x = (A.col1.x * B.col1.x) + (A.col2.x * B.col1.y);
    result.col1.y = (A.col1.y * B.col1.x) + (A.col2.y * B.col1.y);

    // Column 2 = A * B.col2
    result.col2.x = (A.col1.x * B.col2.x) + (A.col2.x * B.col2.y);
    result.col2.y = (A.col1.y * B.col2.x) + (A.col2.y * B.col2.y);

    return result;
}

Vector2d MatrixMultiply_3x3_Vector2d(Matrix3x3 A, Vector2d v)
{
    Vector2d result = {0};

    result.x = (A.col1.x * v.x) + (A.col2.x * v.y) + (A.col3.x * 1.0f);
    result.y = (A.col1.y * v.x) + (A.col2.y * v.y) + (A.col3.y * 1.0f);

    return result;
}

Matrix3x3 MatrixInvert_3x3(Matrix3x3 M)
{
    float a = M.col1.x, b = M.col2.x, c = M.col3.x;
    float d = M.col1.y, e = M.col2.y, f = M.col3.y;
    float g = M.col1.z, h = M.col2.z, i = M.col3.z;

    // Calculate the determinant from row values derived from column storage.
    float det = a * (e * i - f * h) -
                b * (d * i - f * g) +
                c * (d * h - e * g);

    if (det == 0.0f)
        return (Matrix3x3){0}; // Cannot invert

    float invDet = 1.0f / det;
    Matrix3x3 res = {0};

    // Inverse entries (row-major symbols), then pack into column vectors.
    res.col1.x = (e * i - f * h) * invDet;
    res.col1.y = (f * g - d * i) * invDet;
    res.col1.z = (d * h - e * g) * invDet;

    res.col2.x = (c * h - b * i) * invDet;
    res.col2.y = (a * i - c * g) * invDet;
    res.col2.z = (b * g - a * h) * invDet;

    res.col3.x = (b * f - c * e) * invDet;
    res.col3.y = (c * d - a * f) * invDet;
    res.col3.z = (a * e - b * d) * invDet;

    return res;
}



float VectorBox_2d(Vector2d vector)
{
    float box = vector.x * vector.y;

    // Just want the absolute area
    if (box < 0)
    {
        box *= -1;
    }
    return box;
}



// Returns the boxed coords from a collection of vertice vectors (must all be relative to the associated object's coords)
// Matrix2x2 GetEnvelopingSubspace2d_FromMatrix(DynamicArray position_vectors)
// {
//    Matrix2x2 subspace_coords = {0};
//    if (position_vectors == NULL)
//    {
//       return subspace_coords;
//    }
//    if (position_vectors.)
//    Matrix2x2 box_coords = {0};
//    Vector2d *pts = vertices.coll.items;

//    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
//    box_coords.col1 = pts[0];
//    box_coords.col2 = pts[1];
//    Vector2d vertice = {0};
//    for (size_t i = 1; i < vertices.coll.count; i++)
//    {
//       vertice = pts[i];

//       // Check if x is a min or max
//       if (vertice.x > box_coords.col2.x)
//       {
//          box_coords.col2.x = vertice.x;
//       }
//       else if (vertice.x < box_coords.col1.x)
//       {
//          box_coords.col1.x = vertice.x;
//       }

//       // Check if y is a min or max
//       if (vertice.y > box_coords.col2.y)
//       {
//          box_coords.col2.y = vertice.y;
//       } else if (vertice.y > box_coords.col1.y)
//       {
//          box_coords.col1.y = vertice.y;
//       }
//    }
//    return box_coords;
// }

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

