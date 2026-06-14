#include "math/geometry.h"
#include "math/helpers.h"

LArray CreateVertices_Symmetric(int vertice_count, float radius)
{
    if (vertice_count < 0)
    {
        fprintf(stderr, "The provided number of contact vertices, %d, is less than 0. Returning an empty surface.\n", vertice_count);
        return MakeLArray(0, sizeof(Vector2d));
    }
    LArray points = MakeLArray(vertice_count, sizeof(Vector2d));
    float angle_step = (2.0f * PI) / vertice_count;

    char log_buffer[512] = {0};
    int log_offset = 0;

    for (int i = 0; i < vertice_count; i++)
    {
        float current_angle = i * angle_step;
        Vector2d p;
        p.x = radius * cosf(current_angle);
        p.y = radius * sinf(current_angle);
        printf("Generated vertice %d: Angle = %.3f\n", i, current_angle);
        LArray_Push(&points, &p);
        if (log_offset < (int)sizeof(log_buffer) - 30)
        {
            log_offset += snprintf(log_buffer + log_offset, sizeof(log_buffer) - log_offset, " (%.2f, %.2f)", ((Vector2d *)points.items)[i].x, ((Vector2d *)points.items)[i].y);
        }
    }
    CenterVerticesToExtents(&points);
    printf("VERTICES(SYM) CREATED:%s\n", log_buffer);
    return points;
}
LArray CreateVertices_Irregular(int vertice_count, float min_radius, float max_radius)
{
    if (vertice_count < 0)
    {
        fprintf(stderr, "The provided number of contact vertices, %d, is less than 0. Returning an empty surface.\n", vertice_count);
        return MakeLArray(0, sizeof(Vector2d));
    }
    LArray points = MakeLArray(vertice_count, sizeof(Vector2d));
    float angle_step = (2.0f * PI) / vertice_count;

    char log_buffer[512] = {0};
    int log_offset = 0;
    float radius_rand;
    for (int i = 0; i < vertice_count; i++)
    {
        radius_rand = GetRandomFloat(min_radius, max_radius);
        float current_angle = i * angle_step;
        Vector2d p;
        p.x = radius_rand * cosf(current_angle);
        p.y = radius_rand * sinf(current_angle);
        printf("Generated vertice %d: Angle = %.3f\n", i, current_angle);
        LArray_Push(&points, &p);
        if (log_offset < (int)sizeof(log_buffer) - 30)
        {
            log_offset += snprintf(log_buffer + log_offset, sizeof(log_buffer) - log_offset, " (%.2f, %.2f)", ((Vector2d *)points.items)[i].x, ((Vector2d *)points.items)[i].y);
        }
    }
    CenterVerticesToExtents(&points);
    printf("VERTICES(IRREG) CREATED:%s\n", log_buffer);
    return points;
}
// Creates surface vectors with the provided offset.
// 4----3
// |    |
// 1----2
Surface2d CreateSurface_Rectangular(Vector2d dimensions, Vector2d vertice_offset)
{
    Surface2d surf = {0};
    surf.surface_vectors = MakeLArray(4, sizeof(Vector2d));

    // Calculate the half-extents
    float hx = dimensions.x / 2.0f;
    float hy = dimensions.y / 2.0f;

    // Define the 4 corners relative to a center point of (0,0)
    // Typically ordered clockwise or counter-clockwise
    Vector2d vertices[4] = {
        {-hx, -hy}, // Top-Left corner
        {hx, -hy},  // Top-Right corner
        {hx, hy},   // Bottom-Right corner
        {-hx, hy}   // Bottom-Left corner
    };

    // Push the centered vertices into the dynamic array
    for (int i = 0; i < 4; i++)
    {
        LArray_Push(&surf.surface_vectors, &vertices[i]);
    }

    return surf;
}

void CalcBoxVertices(Vector2d dimensions, Vector2d coords_center, Vector2d out_vertices[4])
{
    // Calculate the half-extents
    float hx = dimensions.x / 2.0f;
    float hy = dimensions.y / 2.0f;

    // Assign each corner individually, offset by the center coordinates
    out_vertices[0] = (Vector2d){coords_center.x - hx, coords_center.y - hy}; // Top-Left
    out_vertices[1] = (Vector2d){coords_center.x + hx, coords_center.y - hy}; // Top-Right
    out_vertices[2] = (Vector2d){coords_center.x + hx, coords_center.y + hy}; // Bottom-Right
    out_vertices[3] = (Vector2d){coords_center.x - hx, coords_center.y + hy}; // Bottom-Left
}

// Returns the boxed coords from a collection of vertice vectors with an offset applied to the vertices (e.g. to account for the position of the shape in world space, rather than just the local vertices)
Matrix2x2 CalcAABBCoords_Tight(LArray *vertices, Vector2d vertice_offset)
{
    Matrix2x2 box_coords = {0};
    if (vertices->count < 2)
    {
        return box_coords;
    }
    Vector2d *pts = vertices->items;

    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
    box_coords.col1 = VectorSum_2d(vertice_offset, pts[0]);
    box_coords.col2 = VectorSum_2d(vertice_offset, pts[0]);
    Vector2d vertice = {0};
    for (size_t i = 1; i < vertices->count; i++)
    {
        vertice = VectorSum_2d(vertice_offset, pts[i]);

        box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
        box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

        box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
        box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);
    }
    return box_coords;
}

Vector2d CalcAABBDimensions(LArray *vertices)
{
    Matrix2x2 box_coords = CalcAABBCoords_Tight(vertices, ZERO_VECTOR_2D);

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

// Returns the coordinates of the intersection box between box1 and box2, or an empty box if there is no intersection
Matrix2x2 CalcBoxOverlapWithBox(Matrix2x2 box1, Matrix2x2 box2)
{
    // Calculate the coordinates of the intersection box
    Matrix2x2 intersection;
    intersection.col1.x = fmaxf(box1.col1.x, box2.col1.x);
    intersection.col1.y = fmaxf(box1.col1.y, box2.col1.y);
    intersection.col2.x = fminf(box1.col2.x, box2.col2.x);
    intersection.col2.y = fminf(box1.col2.y, box2.col2.y);

    // Check if there is an intersection
    if (intersection.col1.x < intersection.col2.x && intersection.col1.y < intersection.col2.y)
    {
        return intersection; // Return the coordinates of the intersection box
    }
    else
    {
        // No intersection, return an empty box (could also return a boolean or use a different approach to indicate no intersection)
        return (Matrix2x2){0};
    }
}

// Returns true if shape1 fits within shape2
bool ShapeFitsWithinShape(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset)
{
    if (shape1_vertices->count < 2 && shape2_vertices->count < 2)
    {
        return false;
    }
    Vector2d *pts1 = shape1_vertices->items;
    Vector2d *pts2 = shape2_vertices->items;

    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
    Matrix2x2 box1_coords = CalcAABBCoords_Tight(shape1_vertices, shape1_vertice_offset);
    Matrix2x2 box2_coords = CalcAABBCoords_Tight(shape2_vertices, shape2_vertice_offset);

    // Check if box1 is completely contained within box2
    if (BoxFitsWithinBox(box1_coords, box2_coords))
    {
        return true;
    }
    return false;
}

// Returns the adjusted offset for Box B so it is perfectly centered inside Box A
Vector2d CalcCenteredBoxOffset(Vector2d box_a_dimensions, Vector2d box_b_dimensions)
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

// Translates the vertices so that the minimum extreme point of the shape is at the origin (0,0)
void NormaliseVerticesToLocal(LArray *points)
{
    if (points == NULL || points->count == 0)
        return;

    Vector2d *vertices = (Vector2d *)points->items;

    float min_x = INFINITY;
    float min_y = INFINITY;

    // STEP 1: Find the absolute minimum extreme point of the shape (The AABB Min)
    for (size_t i = 0; i < points->count; i++)
    {
        if (vertices[i].x < min_x)
            min_x = vertices[i].x;
        if (vertices[i].y < min_y)
            min_y = vertices[i].y;
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
    if (points == NULL || points->count == 0)
        return;

    Vector2d *vertices = (Vector2d *)points->items;

    float min_x = INFINITY, max_x = -INFINITY;
    float min_y = INFINITY, max_y = -INFINITY;

    // Find the true maximum and minimum bounds of the raw geometry
    for (size_t i = 0; i < points->count; i++)
    {
        if (vertices[i].x < min_x)
            min_x = vertices[i].x;
        if (vertices[i].x > max_x)
            max_x = vertices[i].x;
        if (vertices[i].y < min_y)
            min_y = vertices[i].y;
        if (vertices[i].y > max_y)
            max_y = vertices[i].y;
    }

    // Calculate the exact center point of this bounding box
    Vector2d mid_point = {
        (min_x + max_x) / 2.0f,
        (min_y + max_y) / 2.0f};

    // Shift all vertices so that the center of the bounding box is exactly (0,0)
    for (size_t i = 0; i < points->count; i++)
    {
        vertices[i].x -= mid_point.x;
        vertices[i].y -= mid_point.y;
    }
}

// Ray Casting Algorithm: PIP Test with vertice offset (to allow for testing against the actual position of the shape in world space, rather than just the local vertices)
bool IsPointInPolygon(Vector2d point, Vector2d *polygon_vertices, Vector2d vertice_offset, int vertice_count)
{
    if (vertice_count < 3)
        return false; // A polygon must have at least 3 vertices

    // Transform the test point into the polygon's local coordinate space
    Vector2d local_point = {
        point.x - vertice_offset.x,
        point.y - vertice_offset.y};

    bool inside = false;

    // Perform PIP test using local coordinates
    for (int i = 0, j = vertice_count - 1; i < vertice_count; j = i++)
    {
        if (((polygon_vertices[i].y > local_point.y) != (polygon_vertices[j].y > local_point.y)) &&
            (local_point.x < (polygon_vertices[j].x - polygon_vertices[i].x) * (local_point.y - polygon_vertices[i].y) /
                                     (polygon_vertices[j].y - polygon_vertices[i].y) +
                                 polygon_vertices[i].x))
        {
            inside = !inside;
        }
    }
    return inside;
}

Vector2d CalcGeometricCentre_FromBox(Matrix2x2 box_coords)
{
    // First check that the provided box coords have valid dimensions (i.e. col2 is greater than col1 in both axes)
    if (box_coords.col2.x < box_coords.col1.x || box_coords.col2.y < box_coords.col1.y)
    {
        printf("WARNING: Invalid box coordinates provided to GetGeometricCentre_FromBox. Returning (0,0) as default value.\n");
        return ZERO_VECTOR_2D; // Invalid box, return (0,0) as a default value
    }
    Vector2d mid = {0};
    float mid_x = (box_coords.col1.x + box_coords.col2.x) / 2;
    float mid_y = (box_coords.col1.y + box_coords.col2.y) / 2;
    mid.x = mid_x;
    mid.y = mid_y;

    return mid;
}

Vector2d CalcGeometricCentre_FromSurface(Surface2d object_surface, Vector2d vertice_offset)
{
    // First check that the provided box coords have valid dimensions (i.e. col2 is greater than col1 in both axes)
    if (object_surface.surface_vectors.items == NULL || object_surface.surface_vectors.count < 1)
    {
        printf("WARNING: Surface vectors provided to GetGeometricCentre_FromSurface are NULL or contain no items. Returning (0,0) as default value.\n");
        return ZERO_VECTOR_2D; // Invalid box, return (0,0) as a default value
    }
    Matrix2x2 box_coords = CalcAABBCoords_Tight(&object_surface.surface_vectors, vertice_offset);
    return CalcGeometricCentre_FromBox(box_coords);
}

// Returns true if shape1 fits within shape2
float ShapeOverlapWithShape(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset)
{
    if (shape1_vertices->count < 2 && shape2_vertices->count < 2)
    {
        return false;
    }
    Vector2d *pts1 = shape1_vertices->items;
    Vector2d *pts2 = shape2_vertices->items;

    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
    Matrix2x2 box1_coords = CalcAABBCoords_Tight(shape1_vertices, shape1_vertice_offset);
    Matrix2x2 box2_coords = CalcAABBCoords_Tight(shape2_vertices, shape2_vertice_offset);

    // Check if box1 is completely contained within box2
    if (BoxFitsWithinBox(box1_coords, box2_coords))
    {
        return true;
    }
    return false;
}

// Returns the Surface chunks of Shape A, including the first Shape A points lying outside Shape B, until the first point of Shape A that lies back inside Shape B.
// This gives us the vertices of the section of Shape A that is spanning outside Shape B, which we can use to calculate the area of this section and therefore how much of Shape A is spanning outside Shape B.
LArray ShapeAVerticesInShapeB(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset)
{
    if (shape1_vertices->count < 1 && shape2_vertices->count < 1)
    {
        return MakeLArray(0, sizeof(Vector2d));
    }
    Vector2d *pts1 = shape1_vertices->items;
    Vector2d *pts2 = shape2_vertices->items;

    Matrix2x2 box1_coords = CalcAABBCoords_Tight(shape1_vertices, shape1_vertice_offset);
    Matrix2x2 box2_coords = CalcAABBCoords_Tight(shape2_vertices, shape2_vertice_offset);

    // Get the segment (another box) of box A that is overlapping with box B
    Matrix2x2 box_coords_overlap = CalcBoxOverlapWithBox(box1_coords, box2_coords);

    // Get the dimensions to determine if the returned box is non-zero
    Vector2d overlap_dims = (Vector2d){(box_coords_overlap.col2.x - box_coords_overlap.col1.x), (box_coords_overlap.col2.y - box_coords_overlap.col1.y)};
    LArray shape1_vertices_in_overlap = MakeLArray(shape1_vertices->count, sizeof(Vector2d));
    if (overlap_dims.x * overlap_dims.y > 0)
    {
        Vector2d box_coords_overlap_center = CalcGeometricCentre_FromBox(box_coords_overlap);
        Surface2d box_overlap_surface = CreateSurface_Rectangular(overlap_dims, box_coords_overlap_center);
        for (size_t i = 0; i < shape1_vertices->count; i++)
        {
            Vector2d shape1_point = VectorSum_2d(((Vector2d *)shape1_vertices->items)[i], shape2_vertice_offset);
            if (IsPointInPolygon(shape1_point, (Vector2d *)box_overlap_surface.surface_vectors.items, box_coords_overlap_center, box_overlap_surface.surface_vectors.count))
            {
                LArray_Push(&shape1_vertices_in_overlap, &shape1_point);
            }
        }
    }

    return shape1_vertices_in_overlap;
    // Loop through Shape B vertices & find Shape A chunks within Shape B
    // Surface2d shape_a_chunk = {0};
    // bool first_before_in = false;
    // bool first_after_out = false;
    // for (size_t i = 0; i < shape2_vertices->count; i++)
    // {
    // }
}