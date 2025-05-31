//#include "constants.hlsl"
#include "screen_quad.hlsl"
#include "lighting.hlsl"

SamplerState sampler_linear : register(s0); // this is actually a static sampler?

Texture2D texture_position : register(t0);
Texture2D texture_normal : register(t1);
Texture2D texture_albedo : register(t2);
Texture2D texture_roughness : register(t3);
Texture2D texture_metallic : register(t4);

float4 lambertian_diffuse(float4 albedo)
{
    // (lambertian diffuse) - less realistic than other methods, but cheap enough to work well in real-time
    float4 f_lambert = albedo / PI;
    return f_lambert;
}

float normal_distribution_function(float3 normal, float3 halfway_vector, float roughness)
{
    // (D)normal_distribution_function = approximates the amount the surface's microfacets are aligned to the halfway vector, influenced by the
    //                                   roughness of the surface
    
    // Trowbridge-Reitz GGX
    float a = roughness * roughness;
    float a2 = a * a; // Lighting supposedly looks 'more correct' squaring the roughness
    float NdotH = max(dot(normal, halfway_vector), 0.0f);
    float NdotH2 = NdotH * NdotH;
	
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
	
    return nom / denom;
}

float4 fresnel_equation(float3 viewing_direction, float3 halfway_vector, float4 albedo, float4 metallic)
{
    // (F)fresnel_equation = describes the ratio of the surface reflection at different surface angles
    
    // Fresnel-Schlick
    // This approximation is only really defined for dielectric (non-metallic) surfaces
    // It calculates the base reflectivity (F0) using something call the indices of refraction (IOR)
    // This doesn't hold for conductor (metallic) surfaces, which would require a different Fresnel equation
    
    // As this is inconvenient, we can further approximate by precomputing the surface's
    // response at normal incidence (F0) at a 0 degree angle as if looking directly onto the surface
    // This value can be interpolated based on the viewing angle such that we can use the same equation for both metals & non-metals
    
    // Surface responses at normal incidence (base reflectivity) can be found in online databases: http://refractiveindex.info/
    
    // In the metallic PBR workflow, surfaces are defined with a 'metalness' RGB parameter between 0.0 - 1.0
    // This linearly interpolates between a dialectric base reflectivity value at 0.0 and the surface colour
    // (no diffuse reflections, all refracted light absorbed for metals) at 1.0 for metallic surfaces
    // A base reflectivity of 0.04 holds for most dielectrics and produces physically plausible results without having to author an additional surface parameter
    const float4 base_reflectivity = float4(0.04f, 0.04f, 0.04f, 0.04f);
    float4 F0 = lerp(base_reflectivity, albedo, metallic);
    float cos_theta = max(dot(halfway_vector, viewing_direction), 0.0f);
    return F0 + (1.0f - F0) * pow(clamp(1.0 - cos_theta, 0.0f, 1.0f), 5.0f);
}

float geometry_schlick_ggx(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0; // Lighting supposedly looks 'more correct' squaring the roughness

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

// Returns 0.0f-1.0f depending on the amount of microfacet self-shadowing
float geometry_function(float3 normal, float3 viewing_direction, float3 light_direction, float roughness)
{
    // (G)geometry_function = describes the self-shadowing property of the microfacets (when a surface is relatively rough, the surface's microfacets
    //                        can overshadow other microfacets reducing the light the surface reflects)
    
    // Schlick-GGX + Smith's method
    float NdotV = max(dot(normal, viewing_direction), 0.0f);
    float NdotL = max(dot(normal, light_direction), 0.0f);
    float ggx1 = geometry_schlick_ggx(NdotV, roughness);
    float ggx2 = geometry_schlick_ggx(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float4 compute_lighting_pbr(float4 world_position, float3 normal, float4 albedo, float roughness, float metallic)
{
    float3 viewing_direction = eye_position.xyz - world_position.xyz;
    float4 Lo = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Repeats code for MAX_LIGHTS amount of times rather than compile an actual loop
	[unroll]
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (!lights[i].enabled) 
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

    // Temp ambient light source
    float4 ambient = float4(0.03f, 0.03f, 0.03f, 0.03f) * albedo;
    float4 colour = ambient + Lo;
	
    // Gamma correction
    colour = colour / (colour + float4(1.0f, 1.0f, 1.0f, 1.0f));
    colour = pow(colour, float4(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
   
    return colour;
}

// Direct lighting
float4 ps_deferred_pbr(vs_screen_quad_output input) : SV_Target0
{
    float4 normal = texture_normal.Sample(sampler_linear, input.tex_coord);
    float4 world_position = texture_position.Sample(sampler_linear, input.tex_coord);
    float4 albedo = texture_albedo.Sample(sampler_linear, input.tex_coord);
    float roughness = texture_roughness.Sample(sampler_linear, input.tex_coord).r; // TODO: make these textures single channel
    float metallic = texture_metallic.Sample(sampler_linear, input.tex_coord).r;
	
	// Lighting done in world space
    float4 lighting = compute_lighting_pbr(world_position, normal.rgb, albedo, roughness, metallic);
    lighting.a = 1.0f;
    
    return lighting;
}
