#include "math/geometry.h"
#include "math/helpers.h"

LArray CreateVertices_Symmetric(int vertice_count, float radius_x, float radius_y)
{
    if (vertice_count <= 0)
    {
        if (vertice_count < 0)
        {
            fprintf(stderr, "The provided number of contact vertices, %d, is invalid. Returning an empty surface.\n", vertice_count);
        }
        return MakeLArray(0, sizeof(Vector2d));
    }
    LArray points = MakeLArray(vertice_count, sizeof(Vector2d));
    // Equal angular spacing closes a regular polygon after one full turn:
    // angle_step = 2 * pi / vertex_count.
    float angle_step = (2.0f * PI) / vertice_count;

    for (int i = 0; i < vertice_count; i++)
    {
        float current_angle = i * angle_step;
        Vector2d p;
        p.x = radius_x * cosf(current_angle);
        p.y = radius_y * sinf(current_angle);
        LArray_Push(&points, &p);
    }
    CenterVerticesToExtents(&points);
    return points;
}

// Create a centred rotor outline by sampling a smooth radial blade profile.
LArray CreateVertices_Rotor(int blade_count, float radius_x, float radius_y)
{
    const int minimum_blades = 2;
    const int points_per_blade = 6;
    const int maximum_blades = MAX_SHAPE_VERTICES / points_per_blade;

    if (blade_count < minimum_blades || blade_count > maximum_blades ||
        radius_x <= 0.0f || radius_y <= 0.0f)
    {
        return MakeLArray(0, sizeof(Vector2d));
    }

    int vertex_count = blade_count * points_per_blade;
    LArray points = MakeLArray(vertex_count, sizeof(Vector2d));
    if (!points.items)
    {
        return points;
    }

    Vector2d *vert_ptr = (Vector2d *)points.items;
    float sector_step = (2.0f * PI) / (float)blade_count;
    // Each blade spans a fraction of the sector width to leave gaps between blades
    float half_blade_span = sector_step * 0.35f;

    int write_index = 0;
    for (int blade_index = 0; blade_index < blade_count; blade_index++)
    {
        float center_angle = (float)blade_index * sector_step;

        for (int point_index = 0; point_index < points_per_blade; point_index++)
        {
            // Normalize t from -1.0 (left edge of blade) to +1.0 (right edge of blade)
            float t = -1.0f + (2.0f * (float)point_index / (float)(points_per_blade - 1));

            // Curve the radius outwards toward the center tip of the blade (t = 0)
            // Float rounding can make the endpoint cosine slightly negative;
            // clamp it before applying a fractional power to keep vertices finite.
            float blade_lobe = fmaxf(cosf(t * (PI * 0.5f)), 0.0f);
            float radius_scale = 0.25f + (0.75f * powf(blade_lobe, 1.5f));

            float local_angle = center_angle + (t * half_blade_span);

            vert_ptr[write_index].x = radius_x * radius_scale * cosf(local_angle);
            vert_ptr[write_index].y = radius_y * radius_scale * sinf(local_angle);
            write_index++;
        }
    }

    points.count = write_index;
    return points;
}

// Create a centred gear outline by alternating between root and tip radii.
LArray CreateVertices_Gear(int tooth_count, float radius_x, float radius_y)
{
    const int minimum_teeth = 3;
    const int points_per_tooth = 3;
    const int maximum_teeth = MAX_SHAPE_VERTICES / points_per_tooth;

    if (tooth_count < minimum_teeth || tooth_count > maximum_teeth ||
        radius_x <= 0.0f || radius_y <= 0.0f)
    {
        return MakeLArray(0, sizeof(Vector2d));
    }

    int vertex_count = tooth_count * points_per_tooth;
    LArray points = MakeLArray(vertex_count, sizeof(Vector2d));
    if (!points.items)
    {
        return points;
    }

    Vector2d *vert_ptr = (Vector2d *)points.items;
    float tooth_step = (2.0f * PI) / (float)tooth_count;
    float tip_span = tooth_step * 0.18f;
    float root_scale = 0.78f;
    float tip_scale = 1.0f;

    int write_index = 0;
    for (int tooth_index = 0; tooth_index < tooth_count; tooth_index++)
    {
        float center_angle = (float)tooth_index * tooth_step;
        float root_angle = center_angle - (tooth_step * 0.5f);
        float tip_start_angle = center_angle - tip_span;
        float tip_end_angle = center_angle + tip_span;

        vert_ptr[write_index++] = (Vector2d){
            radius_x * root_scale * cosf(root_angle),
            radius_y * root_scale * sinf(root_angle)};
        vert_ptr[write_index++] = (Vector2d){
            radius_x * tip_scale * cosf(tip_start_angle),
            radius_y * tip_scale * sinf(tip_start_angle)};
        vert_ptr[write_index++] = (Vector2d){
            radius_x * tip_scale * cosf(tip_end_angle),
            radius_y * tip_scale * sinf(tip_end_angle)};
    }

    points.count = write_index;
    CenterVerticesToExtents(&points);
    return points;
}

LArray CreateVertices_Irregular(int vertice_count, float min_radius, float max_radius)
{
    if (vertice_count <= 0)
    {
        if (vertice_count < 0)
        {
            fprintf(stderr, "The provided number of contact vertices, %d, is invalid. Returning an empty surface.\n", vertice_count);
        }
        return MakeLArray(0, sizeof(Vector2d));
    }
    LArray points = MakeLArray(vertice_count, sizeof(Vector2d));
    // Use the same angular scaffold as a regular polygon, then vary each
    // radius to create an irregular but ordered, star-shaped outline.
    float angle_step = (2.0f * PI) / vertice_count;
    float radius_rand;
    for (int i = 0; i < vertice_count; i++)
    {
        radius_rand = GetRandomFloat(min_radius, max_radius);
        float current_angle = i * angle_step;
        Vector2d p;
        p.x = radius_rand * cosf(current_angle);
        p.y = radius_rand * sinf(current_angle);
        LArray_Push(&points, &p);
    }
    CenterVerticesToExtents(&points);
    return points;
}
// 4----3
// |    |
// 1----2
Surface2d CreateSurface_Rectangular(Vector2d dimensions)
{
    Surface2d surf = {0};
    surf.surface_vectors = MakeLArray(4, sizeof(Vector2d));

    // Half-extents convert centre-based dimensions into corner offsets:
    // half_width = width / 2 and half_height = height / 2.
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

void CalcBoxVertices(Vector2d dimensions, Vector2d anchor_position, Vector2d *out_vertices)
{
    // A centred box has corners at centre +/- half of each dimension.
    float hx = dimensions.x / 2.0f;
    float hy = dimensions.y / 2.0f;

    // Assign each corner individually, offset by the center coordinates
    out_vertices[0] = (Vector2d){anchor_position.x - hx, anchor_position.y - hy}; // Top-Left
    out_vertices[1] = (Vector2d){anchor_position.x + hx, anchor_position.y - hy}; // Top-Right
    out_vertices[2] = (Vector2d){anchor_position.x + hx, anchor_position.y + hy}; // Bottom-Right
    out_vertices[3] = (Vector2d){anchor_position.x - hx, anchor_position.y + hy}; // Bottom-Left
}

// Computes the tight axis-aligned bounding box of a set of points.
// Returns a zero box when there are fewer than 2 points.
Matrix2x2 AABB2d_FromPoints(const Vector2d *points, int point_count)
{
    Matrix2x2 box_coords = {0};
    if (point_count < 2 || !points)
    {
        return box_coords;
    }

    // Initialise with the first point so the bounds are data-driven rather than
    // defaulting to zero (which could be an unintentional extreme).
    box_coords.col1 = points[0];
    box_coords.col2 = points[0];
    for (int i = 1; i < point_count; i++)
    {
        Vector2d p = points[i];

        if (p.x < box_coords.col1.x)
            box_coords.col1.x = p.x;
        if (p.x > box_coords.col2.x)
            box_coords.col2.x = p.x;

        if (p.y < box_coords.col1.y)
            box_coords.col1.y = p.y;
        if (p.y > box_coords.col2.y)
            box_coords.col2.y = p.y;
    }
    return box_coords;
}

// Builds the min/max representation used by the AABB helpers from an origin and size.
Matrix2x2 AABB2d_FromOriginDimensions(Vector2d origin, Vector2d dimensions)
{
    // The maximum corner is min corner + extent; this representation avoids
    // storing a separate centre or half-size for broad-phase comparisons.
    return (Matrix2x2){
        .col1 = origin,
        .col2 = VectorSum_2d(origin, dimensions)};
}

// Tests inclusive AABB overlap so boxes that only touch at an edge retain the
// broad-phase behavior used by the physics system.
bool AABB2d_Overlaps(Matrix2x2 box1, Matrix2x2 box2)
{
    // Two axis-aligned boxes overlap exactly when their intervals overlap on
    // both axes. The inclusive comparisons keep edge contact as a candidate.
    return box1.col1.x <= box2.col2.x && box1.col2.x >= box2.col1.x &&
           box1.col1.y <= box2.col2.y && box1.col2.y >= box2.col1.y;
}

// Tests whether the candidate box is fully contained by the container box.
bool AABB2d_Contains(Matrix2x2 container, Matrix2x2 box)
{
    return box.col1.x >= container.col1.x && box.col2.x <= container.col2.x &&
           box.col1.y >= container.col1.y && box.col2.y <= container.col2.y;
}

// Returns the boxed coords from a collection of vertice vectors with an offset applied to the vertices (e.g. to account for the position of the shape in world space, rather than just the local vertices)
Matrix2x2 CalcAABBCoords_Tight(Vector2d *vertices, int vertice_count, Vector2d vertice_offset)
{
    if (vertice_count < 2 || !vertices)
    {
        return (Matrix2x2){0};
    }

    // Apply the offset during the bounds scan so we avoid allocating a
    // temporary translated vertex array.
    Matrix2x2 box_coords = {0};
    box_coords.col1 = VectorSum_2d(vertice_offset, vertices[0]);
    box_coords.col2 = box_coords.col1;

    for (int i = 1; i < vertice_count; i++)
    {
        Vector2d p = VectorSum_2d(vertice_offset, vertices[i]);

        if (p.x < box_coords.col1.x)
            box_coords.col1.x = p.x;
        if (p.x > box_coords.col2.x)
            box_coords.col2.x = p.x;

        if (p.y < box_coords.col1.y)
            box_coords.col1.y = p.y;
        if (p.y > box_coords.col2.y)
            box_coords.col2.y = p.y;
    }

    return box_coords;
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

    // Cast a horizontal ray from the point. Each edge that crosses the ray
    // toggles the inside flag; an odd crossing count means inside, while an
    // even count means outside. The x expression below is the edge's linear
    // interpolation at the test point's y coordinate.
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

// Returns true if shape1 fits within shape2
// float ShapeOverlapWithShape(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset)
// {
//     if (shape1_vertices->count < 2 && shape2_vertices->count < 2)
//     {
//         return false;
//     }
//     Vector2d *pts1 = shape1_vertices->items;
//     Vector2d *pts2 = shape2_vertices->items;

//     // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
//     Matrix2x2 box1_coords = CalcAABBCoords_Tight(shape1_vertices, shape1_vertice_offset);
//     Matrix2x2 box2_coords = CalcAABBCoords_Tight(shape2_vertices, shape2_vertice_offset);

//     // Check if box1 is completely contained within box2
//     if (BoxFitsWithinBox(box1_coords, box2_coords))
//     {
//         return true;
//     }
//     return false;
// }

// Returns the Surface chunks of Shape A, including the first Shape A points lying outside Shape B, until the first point of Shape A that lies back inside Shape B.
// This gives us the vertices of the section of Shape A that is spanning outside Shape B, which we can use to calculate the area of this section and therefore how much of Shape A is spanning outside Shape B.
// LArray ShapeAVerticesInShapeB(LArray *shape1_vertices, LArray *shape2_vertices, Vector2d shape1_vertice_offset, Vector2d shape2_vertice_offset)
// {
//     if (shape1_vertices->count < 1 && shape2_vertices->count < 1)
//     {
//         return MakeLArray(0, sizeof(Vector2d));
//     }
//     Vector2d *pts1 = shape1_vertices->items;
//     Vector2d *pts2 = shape2_vertices->items;

//     Matrix2x2 box1_coords = CalcAABBCoords_Tight(shape1_vertices, shape1_vertice_offset);
//     Matrix2x2 box2_coords = CalcAABBCoords_Tight(shape2_vertices, shape2_vertice_offset);

//     // Get the segment (another box) of box A that is overlapping with box B
//     Matrix2x2 box_coords_overlap = CalcBoxOverlapWithBox(box1_coords, box2_coords);

//     // Get the dimensions to determine if the returned box is non-zero
//     Vector2d overlap_dims = (Vector2d){(box_coords_overlap.col2.x - box_coords_overlap.col1.x), (box_coords_overlap.col2.y - box_coords_overlap.col1.y)};
//     LArray shape1_vertices_in_overlap = MakeLArray(shape1_vertices->count, sizeof(Vector2d));
//     if (overlap_dims.x * overlap_dims.y > 0)
//     {
//         Vector2d box_coords_overlap_center = CalcGeometricCentre_FromBox(box_coords_overlap);
//         Surface2d box_overlap_surface = CreateSurface_Rectangular(overlap_dims, box_coords_overlap_center);
//         for (size_t i = 0; i < shape1_vertices->count; i++)
//         {
//             Vector2d shape1_point = VectorSum_2d(((Vector2d *)shape1_vertices->items)[i], shape2_vertice_offset);
//             if (IsPointInPolygon(shape1_point, (Vector2d *)box_overlap_surface.surface_vectors.items, box_coords_overlap_center, box_overlap_surface.surface_vectors.count))
//             {
//                 LArray_Push(&shape1_vertices_in_overlap, &shape1_point);
//             }
//         }
//     }

//     return shape1_vertices_in_overlap;
//     // Loop through Shape B vertices & find Shape A chunks within Shape B
//     // Surface2d shape_a_chunk = {0};
//     // bool first_before_in = false;
//     // bool first_after_out = false;
//     // for (size_t i = 0; i < shape2_vertices->count; i++)
//     // {
//     // }
// }
