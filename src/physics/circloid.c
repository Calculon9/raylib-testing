// /**********************************************************************************************
//  *
//     INCLUDES/DEFINITIONS
//  *
//  **********************************************************************************************/
// #include "common/common.h"
// #include "physics/circloid.h"
// #include "physics/newton_object.h"
// #include "collections/dynamic_array.h"

// //----------------------------------------------------------------------------------
// // Module Variables Definition (local)
// //----------------------------------------------------------------------------------




// //----------------------------------------------------------------------------------
// // Functions Definition
// //----------------------------------------------------------------------------------
// Circloid CreateCircloid(float radius, ColourRgba colour, size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration, Surface2d surface)
// {
//    NewtonObject2d newtOb = CreateNewtonObject2d(mass, position, velocity, acceleration, surface);
//    // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
//    Circloid newCirc = {0};
//    newCirc.newtonian_properties = newtOb;
//    newCirc.radius = radius;
//    newCirc.colourRgba = colour;

//    return newCirc;
//    // Initialize momentum based on mass and velocity
// }

// Circloid CreateCircloid_FromObject(NewtonObject2d newtOb, float radius, ColourRgba colour)
// {
//    Circloid newCirc = {0};
//    newCirc.newtonian_properties = newtOb;
//    newCirc.radius = radius;
//    newCirc.colourRgba = colour;

//    return newCirc;
// }

// DynamicArray* GenerateCircloidSurfaceVectors (Circloid *circloid, int vertices) {

//    if(vertices < 0) { 
//       fprintf(stderr, "The provided number of contact vertices, %f, is less than 0. Continuing with 0 vertices instead so expect very inaccurate collisions.");
//       vertices = 0;
//    }
//    DynamicArray *points = NewDynamicArray(vertices, sizeof(Vector2d));

//    //Use radians to define the points on circloid circumference that collectively will describe the shape's surface
//    float angleStep = (2.0 * PI) / vertices;
//    for (int i = 0; i < vertices; i++) {
//         float currentAngle = i * angleStep;
        
//         Vector2d p;
//         //Circle origin position is by default at the center, no need to calculate it
//         p.x = circloid->newtonian_properties.coords_origin.x + circloid->radius * cosf(currentAngle);
//         p.y = circloid->newtonian_properties.coords_origin.y + circloid->radius * sinf(currentAngle);
        
//         Array_Push(points, &p);
//     }

//    return points;
// }

// // void Circloid_GetCollisionObjects(Circloid circloid)
// // {
   
// // }