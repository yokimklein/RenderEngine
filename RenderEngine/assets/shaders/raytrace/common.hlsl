// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct hit_info
{
    float4 color_and_distance;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct attributes
{
    float2 bary;
};

struct shadow_ray_payload
{
    bool is_hit;
};