#include "common.hlsl"

// $TODO - jitter rays
#define SAMPLES_PER_PIXEL 4

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> g_output : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure scene_bvh : register(t0);

cbuffer object_cb : register(b0)
{
    float4x4 projection;
    float4x4 projection_inverse;
    float4x4 view;
    float4x4 view_inverse;
	// unused world matrix, doesn't get updated to the current object in the raytracing pipeline so don't use this
    float4x4 unused;
	// theoretically though InstanceID() provides the instance ID of the object which could be used to index into an array if this is ever needed
};

[shader("raygeneration")] 
void ray_gen()
{
	// Get the location within the dispatched 2D grid of work items
	// (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = DispatchRaysDimensions().xy;
    //float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
	
    float2 xy = DispatchRaysIndex().xy + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    float4 viewSpacePosition = mul(float4(screenPos, 1, 1), projection_inverse);
    viewSpacePosition /= viewSpacePosition.w;
	
    float3 worldSpacePosition = mul(float4(viewSpacePosition.xyz, 1.0), view_inverse).xyz;
    float3 cameraPosition = mul(float4(0, 0, 0, 1), view_inverse).xyz;
    float3 rayDirection = normalize(worldSpacePosition - cameraPosition);
	
    float3 colour = float3(0.0f, 0.0f, 0.0f);
	
	// Initialize the ray payload
	hit_info payload;
	payload.color_and_distance = float4(0, 0, 0, 0);
	
	// Define a ray, consisting of origin, direction, and the min-max distance values
    RayDesc ray;
	
    ray.Origin = cameraPosition;
    ray.Direction = rayDirection;
	
    ray.TMin = 0;
    ray.TMax = 100000;
	
	// Trace the ray
	TraceRay
	(
		// Parameter name: AccelerationStructure
		// Acceleration structure
		scene_bvh,
		
		// Parameter name: RayFlags
		// Flags can be used to specify the behavior upon hitting a surface
		RAY_FLAG_NONE,
		
		// Parameter name: InstanceInclusionMask
		// Instance inclusion mask, which can be used to mask out some geometry to this ray by
		// and-ing the mask with a geometry mask. The 0xFF flag then indicates no geometry will be
		// masked
		0xFF,
		
		// Parameter name: RayContributionToHitGroupIndex
		// Depending on the type of ray, a given object can have several hit groups attached
		// (ie. what to do when hitting to compute regular shading, and what to do when hitting
		// to compute shadows). Those hit groups are specified sequentially in the SBT, so the value
		// below indicates which offset (on 4 bits) to apply to the hit groups for this ray. In this
		// sample we only have one hit group per object, hence an offset of 0.
		0,
		
		// Parameter name: MultiplierForGeometryContributionToHitGroupIndex
		// The offsets in the SBT can be computed from the object ID, its instance ID, but also simply
		// by the order the objects have been pushed in the acceleration structure. This allows the
		// application to group shaders in the SBT in the same order as they are added in the AS, in
		// which case the value below represents the stride (4 bits representing the number of hit
		// groups) between two consecutive objects.
		0,
		
		// Parameter name: MissShaderIndex
		// Index of the miss shader to use in case several consecutive miss shaders are present in the
		// SBT. This allows to change the behavior of the program when no geometry have been hit, for
		// example one to return a sky color for regular rendering, and another returning a full
		// visibility value for shadow rays. This sample has only one miss shader, hence an index 0
		0,
		
		// Parameter name: Ray
		// Ray information to trace
		ray,
		
		// Parameter name: Payload
		// Payload associated to the ray, which will be used to communicate between the hit/miss
		// shaders and the raygen
		payload
	);
    colour += payload.color_and_distance.rgb;
	// end loop
	
    //colour /= SAMPLES_PER_PIXEL;
    g_output[launchIndex] = float4(colour, 1.0f);
}
