#include "constants.hlsl"
#include "screen_quad.hlsl"

SamplerState sampler_linear			: register(s0); // this is actually a static sampler?

Texture2D texture_position			: register(t0);
Texture2D texture_normal			: register(t1);
Texture2D texture_ambient			: register(t2);
Texture2D texture_diffuse			: register(t3);
Texture2D texture_specular			: register(t4);

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

struct lighting_result
{
	float4 diffuse;
	float4 specular;
};

struct ps_deferred_lighting_buffers
{
    float4 diffuse_lighting : SV_Target0;
    float4 specular_lighting : SV_Target1;
    float4 ambient_lighting : SV_Target2;
};

// the lighting equations in this code have been taken from https://www.3dgep.com/texturing-lighting-directx-11/
// with some modifications by David White & Toby Kleinsmiede

float4 do_diffuse(float4 light_colour, float3 normal, float3 vertex_to_light)
{
	float NdotL = max(0, dot(normal, vertex_to_light));
    return light_colour * NdotL;
}

float4 do_specular(float3 normal, float3 pixel_to_eye, float3 light_direction_to_vertex, float specular_power)
{
	float3 light_dir = normalize(-light_direction_to_vertex);
    pixel_to_eye = normalize(pixel_to_eye);

	float4 specular = float4(0, 0, 0, 0);
    
    // Check for self-occlusion (geometric self shadowing) TODO: TEST
    // no specular refelction if angle between eye vector and reflected light vector is > 90
    float cosine_angle = dot(pixel_to_eye, light_dir);
	float angle = acos(cosine_angle);
	if (angle <= 90.0f)
	{
		float light_intensity = saturate(dot(normal, light_dir));
		if (light_intensity > 0.0f)
		{
			float3 reflection = normalize(2 * light_intensity * normal - light_dir);
            specular = pow(saturate(dot(reflection, pixel_to_eye)), specular_power); // 32 = specular power
        }
	}
	return specular;
}

// https://learnwebgl.brown37.net/09_lights/lights_attenuation.html#:~:text=Light%20becomes%20weaker%20the%20further,be%20proportional%20to%201%2Fd.
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

lighting_result do_light(light light, float3 normal, float3 pixel_to_eye, float3 pixel_to_light, float specular_power)
{
    // distance(a, b) is the same as length(a - b)
    float distance_to_light = length(pixel_to_light);
    
    float attenuation;
    if (light.light_type == LIGHT_POINT || light.light_type == LIGHT_SPOT)
    {
        attenuation = do_attenuation(light, distance_to_light);
    }
    else
    {
        attenuation = 1.0f;
    }
    
    float spot_intensity;
    if (light.light_type == LIGHT_SPOT)
    {
        spot_intensity = do_spot_cone(light, pixel_to_light);
    }
    else
    {
        spot_intensity = 1.0f;
    }
    
    float3 light_direction;
    if (light.light_type == LIGHT_DIRECTIONAL)
    {
        // TODO: check signs on these vs original code
        light_direction = -light.direction.xyz;
    }
    else
    {
        light_direction = pixel_to_light;
    }
    
    lighting_result result;
    
    // Geometric self shadowing
    // Skip diffuse + specular if light pos is below base surface (angle between vertex normal & light vector is >90)
    // Always succeed for directional lights
    if (light.light_type == LIGHT_DIRECTIONAL || dot(normal, pixel_to_light) >= 0) // cos(90d) == 0, >90d == <0
    {
        result.diffuse = do_diffuse(light.colour, normal, light_direction) * attenuation * spot_intensity;
        result.specular = do_specular(normal, pixel_to_eye, -light_direction, specular_power) * attenuation * spot_intensity;
    }
    else
    {
        result.diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
        result.specular = float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

	return result;
}

lighting_result compute_lighting(float4 world_position, float3 normal, float specular_power)
{
    // TODO: omnidirectional shadow mapping
    // lighting is currently not occluded by geometry
    // https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-12-omnidirectional-shadow-mapping
    
	lighting_result total_result = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
    float4 pixel_to_eye = eye_position - world_position;
    
    // Repeats code for MAX_LIGHTS amount of times rather than compile an actual loop
	[unroll]
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		if (!lights[i].enabled) 
			continue;
        
		lighting_result result = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
		
        float4 pixel_to_light = lights[i].position - world_position;
        result = do_light(lights[i], normal, pixel_to_eye.xyz, pixel_to_light.xyz, specular_power);
		
        total_result.diffuse += result.diffuse;
		total_result.specular += result.specular;
	}

	total_result.diffuse = saturate(total_result.diffuse);
	total_result.specular = saturate(total_result.specular);

	return total_result;
}

ps_deferred_lighting_buffers ps_deferred_lighting(vs_screen_quad_output input)
{
    ps_deferred_lighting_buffers result;
    
    float4 specular_mat = texture_specular.Sample(sampler_linear, input.tex_coord);
    float specular_power = specular_mat.a * 128.0f; // unpack specular power
    specular_mat = float4(specular_mat.rgb, 0.0f);
    
	float4 normal = texture_normal.Sample(sampler_linear, input.tex_coord);
	float4 world_position = texture_position.Sample(sampler_linear, input.tex_coord);
	float4 ambient_mat = texture_ambient.Sample(sampler_linear, input.tex_coord);
	float4 diffuse_mat = texture_diffuse.Sample(sampler_linear, input.tex_coord);
	
	// Lighting done in world space
    lighting_result lighting = compute_lighting(world_position, normal.rgb, specular_power);
    
    result.diffuse_lighting = float4(diffuse_mat.rgb * lighting.diffuse.rgb, 1.0f);
    result.specular_lighting = float4(specular_mat.rgb * lighting.specular.rgb, 1.0f);
    result.ambient_lighting = float4(ambient_mat.rgb * global_ambient.rgb, 1.0f);
    
    return result;
}

/*
float4 ps_forward_lighting(vs_output input)
{
    // Retrieve colour from material/diffuse map
    float4 albedo;
    if (mat.use_diffuse_texture)
    {
        albedo = texture_diffuse.Sample(sampler_linear, input.tex_coord);
        
        // hack to remove masks from textures - TODO: proper transparency
        if (albedo.a == 0.0f)
        {
            discard;
        }
        
    }
    else
    {
        albedo = float4(1, 1, 1, 1);
    }
    
    // Retrieve vertex normal/normal map
    float4 normal;
    if (mat.use_normal_texture)
    {
        // retrieve normal in tangent space from normal map texture & expand the range of the normal value from (0, +1) to (-1, +1)
        normal = texture_normal.Sample(sampler_linear, input.tex_coord) * 2.0 - 1.0f;
    }
    else
    {
        // use model normal (in tangent space)
        normal = float4(input.normal_ts, 0);
    }
    
    // Retrieve material specular/texture specular
    float4 material_specular;
    if (mat.use_specular_texture)
    {
        material_specular = texture_specular.Sample(sampler_linear, input.tex_coord);
    }
    else
    {
        material_specular = mat.specular;
    }
    
    // Lighting
    // convert vectors to tangent space (doing this in the pixel shader causes lights to follow vertex seams)
    float3 vertex_to_eye_ts = tangent_space_to_world(input.vertex_to_eye, input.tangent_frame);
    float3 vertex_to_light_ts = tangent_space_to_world(input.vertex_to_light, input.tangent_frame);
    lighting_result lighting = compute_lighting(input.position_world, normal.rgb, vertex_to_eye_ts, vertex_to_light_ts);
    
    float4 emissive = mat.emissive;
    float4 ambient = mat.ambient * global_ambient;
    float4 diffuse = mat.diffuse * lighting.diffuse;
    float4 specular = material_specular * lighting.specular;
    
    float4 final_colour = (emissive + ambient + diffuse + specular) * albedo;
    return final_colour;
}
*/