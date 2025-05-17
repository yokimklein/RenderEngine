#include "gaussian_blur.hlsl"

float4 ps_depth_of_field(vs_screen_quad_output input) : SV_Target
{
	float4 blurred_tex_colour = blurred_texture.Sample(sampler_linear, input.tex_coord);
	if (!enable_depth_of_field)
	{
		return blurred_tex_colour;
	}
	
	float4 render_tex_colour = render_texture.Sample(sampler_linear, input.tex_coord);
	
	float depth = depth_texture.Sample(sampler_linear, input.tex_coord);
	float blur_amount = depth * depth_of_field_scale;
	
	float4 colour = lerp(render_tex_colour, blurred_tex_colour, blur_amount);
	return colour;
}