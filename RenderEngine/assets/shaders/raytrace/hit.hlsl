#include "common.hlsl"

struct vertex
{
    float4 position : POSITION; // float4 for padding, ignore w value
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0; // How is this padding correctly? TODO: verify struct passing in and out correctly in debugger
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
};
StructuredBuffer<vertex> b_vertex : register(t0);
StructuredBuffer<int> b_indices : register(t1);

[shader("closesthit")] 
void closest_hit(inout hit_info payload, attributes attrib) 
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    //uint vertId = 3 * PrimitiveIndex();
    //float3 hitColor = b_vertex[vertId + 0].color * barycentrics.x + b_vertex[vertId + 1].color * barycentrics.y + b_vertex[vertId + 2].color * barycentrics.z;
    float3 hitColor = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;

    payload.color_and_distance = float4(hitColor, RayTCurrent());
}
