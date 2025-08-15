#include "screen_quad.hlsl"
#include "pbr.hlsl"

SamplerState sampler_linear : register(s0); // this is actually a static sampler?

Texture2D texture_position : register(t0);
Texture2D texture_normal : register(t1);
Texture2D texture_albedo : register(t2);
Texture2D texture_roughness : register(t3);
Texture2D texture_metallic : register(t4);

// Direct lighting
float4 ps_deferred_pbr(vs_screen_quad_output input) : SV_Target0
{
    float4 normal = texture_normal.Sample(sampler_linear, input.tex_coord);
    float4 world_position = texture_position.Sample(sampler_linear, input.tex_coord);
    float4 albedo = texture_albedo.Sample(sampler_linear, input.tex_coord);
    float roughness = texture_roughness.Sample(sampler_linear, input.tex_coord).r; // TODO: make these textures single channel
    float metallic = texture_metallic.Sample(sampler_linear, input.tex_coord).r;
	
	// Lighting done in world space
    float4 lighting = compute_lighting_pbr(world_position, normal.rgb, albedo, roughness, metallic);
    lighting.a = 1.0f;
    
    return lighting;
}
