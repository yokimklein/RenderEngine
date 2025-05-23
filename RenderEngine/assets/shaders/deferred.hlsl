#include "default_vs.hlsl"

SamplerState sampler_linear : register(s0); // this is actually a static sampler?

Texture2D texture_albedo	: register(t0);
Texture2D texture_roughness	: register(t1);
Texture2D texture_metallic	: register(t2);
Texture2D texture_normal	: register(t3);

struct material_data
{
	float4 albedo;
							        //----------------------------------- (16 byte boundary)
    float roughness;
    float metallic;
	bool use_albedo_texture;
	bool use_roughness_texture;
							        //------------------------------------(16 byte boundary)
	bool use_metallic_texture;
	bool use_normal_texture;
	bool render_texture;
	float padding;
							        //----------------------------------- (16 byte boundary)
};

cbuffer material_cb : register(b1)
{
	material_data material;
};

struct ps_deferred_gbuffers
{
	float4 albedo			: SV_Target0;
	float4 roughness		: SV_Target1;
	float4 metallic			: SV_Target2;
	float4 normal			: SV_Target3;
	float4 position			: SV_Target4;
};

float4 ps_albedo(vs_output input)
{
    // Retrieve colour from material/diffuse map
	float4 albedo;
    if (material.use_albedo_texture)
	{
		// Convert sRGB to linear
        albedo = pow(texture_albedo.Sample(sampler_linear, input.tex_coord), 2.2f);
        
        // hack to remove masks from textures - TODO: proper transparency
		// TODO: adjust this to work with deferred
		if (albedo.a == 0.0f)
		{
			discard;
		}
	}
	else
	{
		albedo = float4(1, 1, 1, 1);
	}
	return albedo;
}

float4 ps_roughness(vs_output input)
{
    // Retrieve material roughness/texture specular
    float4 roughness;
    if (material.use_roughness_texture)
    {
        roughness = texture_roughness.Sample(sampler_linear, input.tex_coord);
    }
    else
    {
        roughness = material.roughness;
    }
    return roughness;
}

float4 ps_metallic(vs_output input)
{
    // Retrieve material metallic/texture specular
    float4 metallic;
    if (material.use_metallic_texture)
    {
        metallic = texture_metallic.Sample(sampler_linear, input.tex_coord);
    }
    else
    {
        metallic = material.metallic;
    }
    return metallic;
}

float4 ps_normal(vs_output input)
{
    // Retrieve vertex normal/normal map
	float4 normal;
	if (material.use_normal_texture)
	{
        // retrieve normal in tangent space from normal map texture
		normal = texture_normal.Sample(sampler_linear, input.tex_coord);
		// 'decompress' the range of the normal value from (0, +1) to (-1, +1)
        normal = normal * 2.0 - 1.0f;
		// convert tangent space normal to world space
        float3x3 tangent_frame = float3x3(input.tangent, input.binormal, input.normal);
        normal = float4(transform_vector_space(normal.rgb, tangent_frame), 1);
    }
	else
	{
        // use world normal from model
		normal = float4(input.normal, 1);
    }
	
	return normal;
}

ps_deferred_gbuffers ps_deferred(vs_output input)
{
    ps_deferred_gbuffers gbuffers;
    
	gbuffers.albedo = ps_albedo(input);
    gbuffers.roughness = ps_roughness(input);
    gbuffers.metallic = ps_metallic(input);
	gbuffers.normal = ps_normal(input);
	gbuffers.position = input.position_world;
    
	return gbuffers;
}