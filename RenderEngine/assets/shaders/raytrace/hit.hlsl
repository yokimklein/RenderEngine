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
Texture2D texture_albedo : register(t2, space0);
Texture2D texture_roughness : register(t3, space0);
Texture2D texture_metallic : register(t4, space0);
Texture2D texture_normal : register(t5, space0);

RaytracingAccelerationStructure scene_bvh : register(t6);

float4 compute_lighting_pbr_rt(float4 world_position, float3 normal, float4 albedo, float roughness, float metallic)
{
    float3 viewing_direction = eye_position.xyz - world_position.xyz;
    float4 Lo = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Repeats code for MAX_LIGHTS amount of times rather than compile an actual loop
	[unroll]
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (!lights[i].enabled) 
            continue;
        
        // Cast a shadow ray to check whether this light is occluded by geometry between itself and the ray hit position
        RayDesc shadow_ray;
        shadow_ray.Origin = world_position.xyz;// + normal * EPSILON; // offset to avoid self-intersection
        shadow_ray.Direction = normalize(lights[i].position.xyz - world_position.xyz);
        float distance_to_light = length(lights[i].position.xyz - world_position.xyz);
        shadow_ray.TMin = EPSILON;
        shadow_ray.TMax = distance_to_light - EPSILON; // Avoid hitting light itself or overshooting
        shadow_ray_payload shadow_payload = { true }; // $TODO: How the $!?$ does this fix the occlusion return???
        // Fire shadow ray
        TraceRay
        (
            scene_bvh,
            RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
            0xFF,   /*InstanceInclusionMask*/
            1,      /*RayContributionToHitGroupIndex*/
            0,      /*MultiplierForGeometryContributionToHitGroupIndex*/
            1,      /*MissShaderIndex "shadow_ray_miss"*/
            shadow_ray,
            shadow_payload
        );
        
        //if (shadow_payload.is_hit)
        //    return float4(1.0f, 0.0f, 0.0f, 1.0f);
        //return float4(1.0f, 1.0f, 1.0f, 1.0f);
        
        // If occluded, skip this light's direct contribution
        if (shadow_payload.is_hit)
            continue;
        
        float3 light_direction = lights[i].position.xyz - world_position.xyz;
        float3 halfway_vector = normalize(viewing_direction + light_direction);
        float4 radiance = do_light(lights[i], normal, light_direction.xyz);
        
        // cook torrance specular BRDF
        // f_cooktorrance = DFG / ( 4 * (outgoing_direction dot normal) * (incoming_direction dot normal))
        float D = normal_distribution_function(normal, halfway_vector, roughness);
        float4 F = fresnel_equation(viewing_direction, halfway_vector, albedo, metallic); // F = the specular contribution of any light that hits the surface
        float G = geometry_function(normal, viewing_direction, light_direction, roughness);
        
        // ks is the reflected light ratio (specular)
        float4 kS = F;
        // kd is the refracted light ratio (diffuse)
        float4 kD = float4(1.0f, 1.0f, 1.0f, 1.0f) - kS;
        kD *= 1.0f - metallic;
        // kd + ks ALWAYS = 1.0 to ensure energy conservation
        
        float4 numerator = D * F * G;
        float denominator = 4.0f * max(dot(normal, viewing_direction), 0.0f) * max(dot(normal, light_direction), 0.0f) + EPSILON;
        float4 specular = numerator / denominator;
        
        float NdotL = max(dot(normal, light_direction), 0.0);
        Lo += (kD * lambertian_diffuse(albedo) + specular) * radiance * NdotL;
    }
    //return float4(1.0f, 1.0f, 1.0f, 1.0f);

    // Temp ambient light source
    float4 ambient = global_ambient * albedo;
    float4 colour = ambient + Lo;
	
    // Gamma correction
    colour = colour / (colour + float4(1.0f, 1.0f, 1.0f, 1.0f));
    colour = pow(colour, float4(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
    
    return colour;
}

[shader("anyhit")]
void shadow_ray_hit(inout shadow_ray_payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // $TODO: This is likely never being invoked
    payload.is_hit = true;
    IgnoreHit(); // skip further traversal
}

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
    
    float3 tangent = normalize(v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z);
    float3 binormal = normalize(v0.binormal * bary.x + v1.binormal * bary.y + v2.binormal * bary.z);
    float3 normal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    
    //binormal = normalize(cross(normal, tangent)); // enforce orthogonality

    float4 world_position;
    world_position.xyz = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    world_position.w = 0.0f;
    
    // $TODO: better mip level calculation
    uint num_mips;
    uint texture_width;
    uint height;
    float distance = length(world_position.xyz - eye_position.xyz);
    float3 ray_dir = normalize(WorldRayDirection());
    float NdotV = abs(dot(ray_dir, normal));
    float footprint = max(distance * NdotV, 1.0f);

    // Estimate mip based on projected size
    float mip = log2((float) texture_width / footprint);
    mip = clamp(mip, 0.0f, num_mips - 1.0f);
    
    float4 albedo = texture_albedo.SampleLevel(texture_sampler, uv, mip);
    float4 roughness = texture_roughness.SampleLevel(texture_sampler, uv, mip);
    float4 metallic = texture_metallic.SampleLevel(texture_sampler, uv, mip);
    float4 normal_map = texture_normal.SampleLevel(texture_sampler, uv, mip);
    // 'decompress' the range of the normal value from (0, +1) to (-1, +1)
    normal_map = normal_map * 2.0 - 1.0f;
	// convert tangent space normal to world space
    float3x3 tangent_frame = float3x3(tangent, binormal, normal);
    normal_map = float4(transform_vector_space(normal_map.rgb, tangent_frame), 1);
    
    payload.color_and_distance.rgb = compute_lighting_pbr_rt(world_position, normal_map.rgb, albedo, roughness.r, metallic.r);
    payload.color_and_distance.a = RayTCurrent();
}
