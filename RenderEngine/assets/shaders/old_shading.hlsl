#include "constants.hlsl"
#include "screen_quad.hlsl"

SamplerState sampler_linear : register(s0); // this is actually a static sampler?

Texture2D texture_ambient_lighting : register(t0);
Texture2D texture_diffuse_lighting : register(t1);
Texture2D texture_specular_lighting : register(t2);
Texture2D texture_albedo : register(t3);
Texture2D texture_emissive : register(t4);

// combines each lighting pass
float4 ps_deferred_shading(vs_screen_quad_output input) : SV_TARGET0
{
	float4 albedo = texture_albedo.Sample(sampler_linear, input.tex_coord);
	float4 emissive = texture_emissive.Sample(sampler_linear, input.tex_coord);
	float4 ambient_lighting = texture_ambient_lighting.Sample(sampler_linear, input.tex_coord);
	float4 diffuse_lighting = texture_diffuse_lighting.Sample(sampler_linear, input.tex_coord);
	float4 specular_lighting = texture_specular_lighting.Sample(sampler_linear, input.tex_coord);
	
	float4 final_colour = (emissive + ambient_lighting + diffuse_lighting + specular_lighting) * albedo;
	return final_colour;
}