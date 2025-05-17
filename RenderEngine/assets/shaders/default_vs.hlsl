#include "constants.hlsl"

struct vs_input
{
	float4 position : POSITION; // float4 for padding, ignore w value
	float3 normal : NORMAL;
	float2 tex_coord : TEXCOORD0; // How is this padding correctly? TODO: verify struct passing in and out correctly in debugger
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
};

struct vs_output
{
	float4 position_local : SV_POSITION; // Calculated local position XYZW for matrix math
	float4 position_world : POSITION0; // Input world position as XYZ
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float3 normal : NORMAL;
	float2 tex_coord : TEXCOORD0;
};

cbuffer object_cb : register(b0)
{
	float4x4 projection;
	float4x4 view;
	float4x4 world;
};

vs_output vs_main(vs_input input)
{
	vs_output output = (vs_output) 0;
    
	output.position_local = mul(input.position, world);
	output.position_world = output.position_local;
	output.position_local = mul(output.position_local, view);
	output.position_local = mul(output.position_local, projection);
	output.tex_coord = input.tex_coord;
    
    // convert model space normals to world space
    output.tangent = transform_vector_space(input.tangent, (float3x3) world);
    output.binormal = transform_vector_space(input.binormal, (float3x3) world);
    output.normal = transform_vector_space(input.normal, (float3x3) world);
	
	return output;
}