#include "common.hlsl"
#include "..\pbr.hlsl"

struct vertex
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
};
StructuredBuffer<vertex> b_vertex : register(t0);
StructuredBuffer<int> b_indices : register(t1);

SamplerState texture_sampler : register(s0);
Texture2D texture_albedo : register(t2, space1);
Texture2D texture_roughness : register(t3, space1);
Texture2D texture_metallic : register(t4, space1);
Texture2D texture_normal : register(t5, space1);

[shader("closesthit")] 
void closest_hit(inout hit_info payload, attributes attrib) 
{    
    uint primitiveIndex = PrimitiveIndex();
    uint i0 = b_indices[primitiveIndex * 3 + 0];
    uint i1 = b_indices[primitiveIndex * 3 + 1];
    uint i2 = b_indices[primitiveIndex * 3 + 2];

    vertex v0 = b_vertex[i0];
    vertex v1 = b_vertex[i1];
    vertex v2 = b_vertex[i2];

    float3 bary = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    float2 uv = v0.tex_coord * bary.x + v1.tex_coord * bary.y + v2.tex_coord * bary.z;
    
    float3 tangent = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;
    float3 binormal = v0.binormal * bary.x + v1.binormal * bary.y + v2.binormal * bary.z;
    float3 normal = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;

    float mip = 0.0f;
    //float mip = log2(1.0f / RayTCurrent()); // crude approximation
    
    float4 albedo = texture_albedo.SampleLevel(texture_sampler, uv, mip);
    float4 roughness = texture_roughness.SampleLevel(texture_sampler, uv, mip);
    float4 metallic = texture_metallic.SampleLevel(texture_sampler, uv, mip);
    float4 normal_map = texture_normal.SampleLevel(texture_sampler, uv, mip);
    // 'decompress' the range of the normal value from (0, +1) to (-1, +1)
    normal_map = normal_map * 2.0 - 1.0f;
	// convert tangent space normal to world space
    float3x3 tangent_frame = float3x3(tangent, binormal, normal);
    normal_map = float4(transform_vector_space(normal_map.rgb, tangent_frame), 1);
    
    payload.color_and_distance = float4(albedo.rgb, RayTCurrent());
    
    //payload.color_and_distance = float4(attrib.bary.x, attrib.bary.y, 1.0 - attrib.bary.x - attrib.bary.y, RayTCurrent());
    //uv.y = 1.0 - uv.y;
    //if (bary.x > 0.99f)
    
    //    payload.color_and_distance = float4(v0.tex_coord, 0, 1);
    //else if (bary.y > 0.99f)
    //    payload.color_and_distance = float4(v1.tex_coord, 0, 1);
    //else if (bary.z > 0.99f)
    //    payload.color_and_distance = float4(v2.tex_coord, 0, 1);
    
    //float3 albedo = float3(1, 1, 1);
    //payload.color_and_distance = float4(uv, 0.0f, 1.0f);
}
