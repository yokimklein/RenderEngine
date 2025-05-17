struct vs_screen_quad_input
{
	float4 position : POSITION; // float4 for padding, ignore w value
	float2 tex_coord : TEXCOORD;
};

struct vs_screen_quad_output
{
	float4 position : SV_POSITION;
	float2 tex_coord : TEXCOORD;
};

vs_screen_quad_output vs_screen_quad(vs_screen_quad_input input)
{
	vs_screen_quad_output output = (vs_screen_quad_output)0;
	output.position = input.position;
	output.tex_coord = input.tex_coord;
	return output;
}