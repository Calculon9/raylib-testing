#include "math/geometry.h"

// Returns the boxed coords from a collection of vertice vectors (must all be relative to the associated object's coords)
Matrix2x2 GetBoxedCoords(LArray *vertices)
{
    Matrix2x2 box_coords = {0};
    if (vertices->count < 2)
    {
        return box_coords;
    }
    Vector2d *pts = vertices->items;

    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
    box_coords.col1 = pts[0];
    box_coords.col2 = pts[0];
    Vector2d vertice = {0};
    for (size_t i = 1; i < vertices->count; i++)
    {
        vertice = pts[i];

        box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
        box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

        box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
        box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);
    }
    return box_coords;
}

Vector2d GetBoxedDimensions(LArray *vertices)
{
    Matrix2x2 box_coords = GetBoxedCoords(vertices);

    Vector2d box_dims = (Vector2d){box_coords.col2.x - box_coords.col1.x, box_coords.col2.y - box_coords.col1.y};

    return box_dims;
}

// Returns true if box1 fits within box2
bool BoxFitsWithinBox(Matrix2x2 box1, Matrix2x2 box2)
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
bool ShapeFitsWithinShape(LArray *shape1_vertices, LArray *shape2_vertices)
{
    if (shape1_vertices->count < 2 && shape2_vertices->count < 2)
    {
        return false;
    }
    Vector2d *pts1 = shape1_vertices->items;
    Vector2d *pts2 = shape2_vertices->items;

    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
    Matrix2x2 box1_coords = GetBoxedCoords(shape1_vertices);
    Matrix2x2 box2_coords = GetBoxedCoords(shape2_vertices);

    // Check if box1 is completely contained within box2
    if (BoxFitsWithinBox(box1_coords, box2_coords))
    {
        return true;
    }
    return false;
}

// Returns the adjusted offset for Box B so it is perfectly centered inside Box A
Vector2d GetCenteredBoxOffset(Vector2d box_a_dimensions, Vector2d box_b_dimensions)
{
    Vector2d centered_coords;

    // Calculate half of the empty space remaining on the X axis
    float total_empty_width = box_a_dimensions.x - box_b_dimensions.x;
    centered_coords.x = total_empty_width / 2.0f;

    // Calculate half of the empty space remaining on the Y axis
    float total_empty_height = box_a_dimensions.y - box_b_dimensions.y;
    centered_coords.y = total_empty_height / 2.0f;

    return centered_coords;
}

// Returns the adjusted offset for Box B so it is perfectly centered inside Box A
void NormaliseVerticesToLocal(LArray *points)
{
    if (points == NULL || points->count == 0) return;

    Vector2d *vertices = (Vector2d *)points->items;

    float min_x = INFINITY;
    float min_y = INFINITY;

    // STEP 1: Find the absolute minimum extreme point of the shape (The AABB Min)
    for (size_t i = 0; i < points->count; i++)
    {
        if (vertices[i].x < min_x) min_x = vertices[i].x;
        if (vertices[i].y < min_y) min_y = vertices[i].y;
    }

    // STEP 2: Translate all vertices by subtracting the minimums.
    // This acts as a translation vector T = (-min_x, -min_y)
    for (size_t i = 0; i < points->count; i++)
    {
        vertices[i].x -= min_x;
        vertices[i].y -= min_y;
    }
}

void CenterVerticesToExtents(LArray *points)
{
    if (points == NULL || points->count == 0) return;

    Vector2d *vertices = (Vector2d *)points->items;

    float min_x = INFINITY, max_x = -INFINITY;
    float min_y = INFINITY, max_y = -INFINITY;

    // 1. Find the true maximum and minimum bounds of the raw geometry
    for (size_t i = 0; i < points->count; i++)
    {
        if (vertices[i].x < min_x) min_x = vertices[i].x;
        if (vertices[i].x > max_x) max_x = vertices[i].x;
        if (vertices[i].y < min_y) min_y = vertices[i].y;
        if (vertices[i].y > max_y) max_y = vertices[i].y;
    }

    // 2. Calculate the exact center point of this bounding box
    Vector2d mid_point = {
        (min_x + max_x) / 2.0f,
        (min_y + max_y) / 2.0f
    };

    // 3. Shift all vertices so that the center of the bounding box is exactly (0,0)
    for (size_t i = 0; i < points->count; i++)
    {
        vertices[i].x -= mid_point.x;
        vertices[i].y -= mid_point.y;
    }
}