#include <stdlib.h>
#include "memory/cmemory.h"
#include "math/cvectors.h"

// Sum all Vector2 in a contiguous array, return by value (Stack)
Vector2d Vector2dSumArray(Vector2d *array, size_t count) { 
    Vector2d result = {0.0f, 0.0f};

    if (array == NULL) return result;

    for (size_t i = 0; i < count; i++) {
        result.x += array[i].x; // Use . because it's a contiguous array of structs
        result.y += array[i].y;
    }
    
    return result;
}

// Sum all Vector2 in a contiguous array, return dynamic allocation (Heap)
Vector2d* Vector2dSumArrayDynamic(Vector2d *array, size_t count) { 
    // We only need to allocate ONE Vector2 to hold the result, not a whole array!
    Vector2d *presult = AllocateArray(1, sizeof(Vector2d));

    // Calculate the sum using our stack function to avoid duplicating logic
    *presult = Vector2dSumArray(array, count);
    
    return presult;
}

// Sum all Vector3 in the array, return by value (Stack).
Vector3d Vector3dSumArray (Vector3d *array, size_t count) { 
    Vector3d result = {0.0f, 0.0f, 0.0f};

    if (array == NULL) return result;

    for (size_t i = 0; i < count; i++) {
        result.x += array[i].x; // Use . because it's a contiguous array of structs
        result.y += array[i].y;
        result.z += array[i].z;
    }
    
    return result;
};

// Sum all Vector3 in a contiguous array, return dynamic allocation (Heap)
Vector3d* Vector3dSumArrayDynamic(Vector3d *array, size_t count) { 
    // We only need to allocate ONE Vector3 to hold the result, not a whole array!
    Vector3d *presult = AllocateArray(1, sizeof(Vector3d));

    // Calculate the sum using our stack function to avoid duplicating logic
    *presult = Vector3dSumArray(array, count);
    
    return presult;
}