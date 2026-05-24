#include "math/geometry.h"

// Returns the boxed coords from a collection of vertice vectors (must all be relative to the associated object's coords)
Matrix2x2 GetBoxedCoords(LArray vertices)
{
    Matrix2x2 box_coords = {0};
    if (vertices.count < 2)
    {
        return box_coords;
    }
    Vector2d *pts = vertices.items;

    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
    box_coords.col1 = pts[0];
    box_coords.col2 = pts[0];
    Vector2d vertice = {0};
    for (size_t i = 1; i < vertices.count; i++)
    {
        vertice = pts[i];

        box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
        box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

        box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
        box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);
    }
    return box_coords;
}

// Returns true if box1 fits within box2
bool BoxFitsWithin(Matrix2x2 box1, Matrix2x2 box2)
{
    // Check if box1 is completely contained within box2
    if (box1.col1.x >= box2.col1.x && box1.col2.x <= box2.col2.x &&
        box1.col1.y >= box2.col1.y && box1.col2.y <= box2.col2.y)
    {
        return true;
    }
    return false;
}

// Returns true if shape1 fits within shape2
bool ShapeFitsWithinShape(LArray shape1_vertices, LArray shape2_vertices)
{
    if (shape1_vertices.count < 2 && shape2_vertices.count < 2)
    {
        return false;
    }
    Vector2d *pts1 = shape1_vertices.items;
    Vector2d *pts2 = shape2_vertices.items;

    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
    Matrix2x2 box1_coords = GetBoxedCoords(shape1_vertices);
    Matrix2x2 box2_coords = GetBoxedCoords(shape2_vertices);

    // Check if box1 is completely contained within box2
    if (BoxFitsWithin(box1_coords, box2_coords))
    {
        return true;
    }
    return false;
}

