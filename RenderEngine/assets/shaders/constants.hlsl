// Maths
#define EPSILON 0.0001f

// Transform vector from tangent space to world space.
float3 transform_vector_space(float3 vec, float3x3 space_frame)
{
    float3 transformed_vector = normalize(mul(vec, space_frame));
    return transformed_vector;
}