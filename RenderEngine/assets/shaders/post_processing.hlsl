#include "screen_quad.hlsl"

SamplerState sampler_linear : register(s0);

Texture2D render_texture    : register(t0);
Texture2D depth_texture     : register(t1);
Texture2D blurred_texture   : register(t2);

cbuffer constant_buffer : register(b0)
{
    // Blur parameters
    float blur_x_coverage; // 0.0fmin - 1.0fmax: How much of the screen is blurred along the x axis from the right-hand side of the screen
    float blur_strength; // 0.0fmin - 10.0fmax, blur strength levels
    uint texture_width;
    uint texture_height;
    // 16 byte boundary
    bool enable_blur;
    bool enable_depth_of_field;
    float depth_of_field_scale; // 0.0fmin - 1.0fmax: Minimum depth value before maximum blur kicks in, default 1.0f
    float blur_sharpness; // 0.5fmin - 10.0fmax, default 4.5f
    // 16 byte boundary
    // Colour tint parameters
    bool enable_greyscale;
};

//#define MSAA_SAMPLES 4 // TODO: make this a CB value

// Multi Sampling Anti Aliasing (MSAA)
// - Doesn't work with transparent textures
// - Typical sample values: 2, 4, 8, 16
// - Perf hit, increasing with number of samples

// Fast Approximate Anti Aliasing (FXAA)
// - 'blurs' the pixels
// - Low performance
// - Not the best looking results

// ==BLUR EFFECTS==
// Gaussian blur
// Box blur
// Depth of field blur
// Bloom
// Motion blur (optional, difficult)
// 
// ==ANTI ALIASING==
// MSAA

float4 ps_greyscale(float4 colour)
{
	// Get an average of all 3 colours
    float average_colour = (colour.r + colour.g + colour.b) / 3.0f;
    float4 tex_colour = float4(average_colour, average_colour, average_colour, 1.0f);
    return tex_colour;
}

float4 ps_tex_to_screen(vs_screen_quad_output input) : SV_Target
{
    //float depth = depth_texture.Sample(sampler_linear, input.tex_coord).r;
    //float4 tex_colour = { depth, depth, depth, depth };
    
    float4 tex_colour = render_texture.Sample(sampler_linear, input.tex_coord);
    
    if (enable_greyscale)
    {
        tex_colour = ps_greyscale(tex_colour);
    }
    
    return tex_colour;
}