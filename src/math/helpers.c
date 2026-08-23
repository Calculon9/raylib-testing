#include "math/helpers.h"
#include <time.h>

float GetRandomFloat(float min, float max)
{
    float r = 0.0;

    int r_a = rand();
    int r_b = rand();
    float r_frac = (float)r_a / (float)r_b;
    if (r_frac > 1.0)
        r_frac = 1.0 / r_frac;
    
    r = min + r_frac * (max - min);
    return r;
}

unsigned long CalcHashFromInts(int a, int b)
{
    // Scramble the integer bits so sequential IDs distribute beautifully
    unsigned long hash = (unsigned long)a * 2654435761UL;
    return hash % b;

}

float ClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }

    return value;
}
