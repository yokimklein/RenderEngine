#include "constants.hlsl"

// Light type enum (unused for now)
#define LIGHT_POINT 0       // A positional light that emits light evenly in all directions
#define LIGHT_SPOT 1        // A positional light that emits light in a specific direction
#define LIGHT_DIRECTIONAL 2 // A directional light source only defines a direction but does not have a position (it is considered to be infinitely far away)

// Must match c++ macro
#define MAX_LIGHTS 10

// Struct size must be a multiple of 16 bytes for alignment
struct light
{
    float4 position;
										//----------------------------------- (16 byte boundary)
    float4 direction; // Spot/Directional lights: this property defines the direction the light is pointing
										//----------------------------------- (16 byte boundary)
    float4 colour;
										//----------------------------------- (16 byte boundary)
    float spot_angle; // The angle of the spotlight cone in radians
    float constant_attenuation;
    float linear_attenuation;
    float quadratic_attenuation;
										//----------------------------------- (16 byte boundary)
    int light_type;
    bool enabled;
    int2 padding;
										//----------------------------------- (16 byte boundary)
};

cbuffer lights_cb : register(b0)
{
    float4 eye_position;
										//----------------------------------- (16 byte boundary)
    float4 global_ambient;
										//----------------------------------- (16 byte boundary)
    light lights[MAX_LIGHTS];
};

// the lighting equations in this code have been taken from https://www.3dgep.com/texturing-lighting-directx-11/
// with some modifications by David White & Toby Kleinsmiede

float do_attenuation(light light, float distance_to_light)
{
    // Attenuation is proportional to 1/distance^2 in the real world
    // Quadratic attenuation causes light to 'fall off' very quickly, so we're also including constant and linear attenuation
    
    // Constant
    float attenuation_distance = light.constant_attenuation; // 1.0f
    // Linear
    attenuation_distance += light.linear_attenuation * distance_to_light;
    // Quadratic
    attenuation_distance += light.quadratic_attenuation * (distance_to_light * distance_to_light);
    
    // Epsilon minimum value to prevent divisions by zero
    float attenuation = 1.0f / max(attenuation_distance, EPSILON);

    return attenuation;
}

float do_spot_cone(light light, float3 pixel_to_light)
{
    float min_cos = cos(light.spot_angle);
    float max_cos = (min_cos + 1.0f) / 2.0f;
    // Normalise pixel to light as light direciton is already normalized
    float cos_angle = dot(light.direction.xyz, -normalize(pixel_to_light));
    return smoothstep(min_cos, max_cos, cos_angle);
}

// Returns radiance
float4 do_light(light light, float3 normal, float3 light_direction)
{
    // distance(a, b) is the same as length(a - b)
    float distance_to_light = length(light_direction);
    
    float attenuation = 1.0f;;
    if (light.light_type == LIGHT_POINT || light.light_type == LIGHT_SPOT)
    {
        attenuation = do_attenuation(light, distance_to_light);
    }
    
    float spot_intensity = 1.0f;
    if (light.light_type == LIGHT_SPOT)
    {
        spot_intensity = do_spot_cone(light, light_direction);
    }
    
    //float3 light_direction = light_direction;
    if (light.light_type == LIGHT_DIRECTIONAL)
    {
        // TODO: check signs on these vs original code
        light_direction = -light.direction.xyz;
    }
    
    float4 radiance = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Geometric self shadowing
    // Skip diffuse + specular if light pos is below base surface (angle between vertex normal & light vector is >90)
    // Always succeed for directional lights
    if (light.light_type == LIGHT_DIRECTIONAL || dot(normal, light_direction) >= 0) // cos(90d) == 0, >90d == <0
    {
        //float NdotL = max(0, dot(normal, light_direction));
        return light.colour * attenuation * spot_intensity; // * NdotL;
    }

    return radiance;
}