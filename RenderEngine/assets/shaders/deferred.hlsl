#include "default_vs.hlsl"

SamplerState sampler_linear : register(s0); // this is actually a static sampler?

Texture2D texture_diffuse	: register(t0);
Texture2D texture_specular	: register(t1);
Texture2D texture_normal	: register(t2);

struct material_data
{
	float4 emissive;
							        //----------------------------------- (16 byte boundary)
	float4 ambient;
							        //------------------------------------(16 byte boundary)
	float4 diffuse;
							        //----------------------------------- (16 byte boundary)
	float4 specular;
							        //----------------------------------- (16 byte boundary)
	float specular_power;
	bool use_diffuse_texture;
	bool use_specular_texture;
	bool use_normal_texture;
							        //----------------------------------- (16 byte boundary)
	bool render_texture;
	float3 padding;
							        //----------------------------------- (16 byte boundary)
};

cbuffer material_cb : register(b1)
{
	material_data material;
};

struct ps_deferred_gbuffers
{
	float4 albedo			: SV_Target0;
	float4 specular			: SV_Target1;
	float4 normal			: SV_Target2;
	float4 position			: SV_Target3;
	float4 emissive			: SV_Target4;
	float4 ambient			: SV_Target5;
	float4 diffuse			: SV_Target6;
};

float4 ps_albedo(vs_output input)
{
    // Retrieve colour from material/diffuse map
	float4 albedo;
	if (material.use_diffuse_texture)
	{
		albedo = texture_diffuse.Sample(sampler_linear, input.tex_coord);
        
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

float4 ps_specular(vs_output input)
{
    // Retrieve material specular/texture specular
	float4 specular;
	if (material.use_specular_texture)
	{
		specular = texture_specular.Sample(sampler_linear, input.tex_coord);
	}
	else
	{
		specular = material.specular;
	}
	
	// Squeeze specular power down to fit within 8 bits
    specular.a = material.specular_power / 128.0f;
	
	return specular;
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
	gbuffers.specular = ps_specular(input);
	gbuffers.normal = ps_normal(input);
	gbuffers.position = input.position_world;
	gbuffers.emissive = material.emissive;
	gbuffers.ambient = material.ambient;
	gbuffers.diffuse = material.diffuse;
    
	return gbuffers;
}