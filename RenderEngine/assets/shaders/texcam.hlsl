#include "default_vs.hlsl"

SamplerState sampler_linear : register(s0); // this is actually a static sampler?

Texture2D texture_render_view : register(t0);

// Simple diffuse passthrough, used by render target texture material
float4 ps_sample_texture(vs_output input) : SV_Target
{
	float4 tex_colour = texture_render_view.Sample(sampler_linear, input.tex_coord);
	return tex_colour;
}