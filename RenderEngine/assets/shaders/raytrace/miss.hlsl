#include "common.hlsl"

[shader("miss")]
void miss(inout hit_info payload : SV_RayPayload)
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);

    float ramp = launchIndex.y / dims.y;
    payload.color_and_distance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
}