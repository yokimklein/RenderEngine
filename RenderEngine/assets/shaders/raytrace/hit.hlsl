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
//Texture2D texture_roughness : register(t3);
//Texture2D texture_metallic : register(t4);
//Texture2D texture_normal : register(t5);

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

    float mip = 0.0f;
    //float mip = log2(1.0f / RayTCurrent()); // crude approximation
    float3 albedo = texture_albedo.SampleLevel(texture_sampler, uv, mip).rgb;
    payload.color_and_distance = float4(albedo, RayTCurrent());
    
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
