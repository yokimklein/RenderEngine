#include "constants.hlsl"
#include "post_processing.hlsl"

// https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/

// Enums
#define BLUR_HORIZONTAL 0
#define BLUR_VERTICAL 1

// https://en.wikipedia.org/wiki/Normal_distribution
// In a true normal distribution, only 0.00034% of all samples will fall outside +/-6sigma
#define SIX_SIGMA 6 // 4.5 also seems to be standard in certain applications
#define BLUR_RADIUS 5       // pixel blur radius
#define BLUR_RADIUS_MIN 2   // minimum blur radius

// standard_deviation = 1sigma
// pixel_offset = pixel offset from centre of distribution
float gaussian_weight(float sigma, int pixel_offset)
{
    // original gaussian function: f(x) = a * exp(-(x^2 / 2 * sigma^2))
    //float x = pixel_offset;
    //float weight = exp(-(pow(x, 2) / (2 * pow(sigma, 2))));
    
    // optimised gaussian function: reduces operations & limits slow divisions
    float x = pixel_offset / sigma;
    float weight = exp(-x * x * blur_sharpness); // 0.5f-10.0f, default 4.5
    
    return weight;
}

float4 new_gaussian_blur(vs_screen_quad_output input, uint direction)
{
    // Skip blur beyond coverage boundary
    if (!enable_blur || input.tex_coord.x > blur_x_coverage)
    {
        return render_texture.Sample(sampler_linear, input.tex_coord);
    }
    
    // clamp this to minimum radius of 2, otherwise a scale of 0 will cause the shader to fail
    float blur_radius = max(BLUR_RADIUS * blur_strength, BLUR_RADIUS_MIN);
    int total_samples = blur_radius * 2 + 1; // diameter + centre
    
    float total_weight = 0.0f;
    float4 total_weighted_samples = 0.0f;
    
    [loop]
    for (int sample_index = 0; sample_index < total_samples; sample_index++)
    {
        // pixel offset, starts negative at leftmost radii, increments each loop towards rightmost offset
        float pixel_offset = sample_index - blur_radius; // Distance from center of filter
        
        float2 sample_texcoord = input.tex_coord;
        if (direction == BLUR_HORIZONTAL)
        {
            float pixel_size = 1.0f / texture_width; // size in texcoords, the screen quad is -1.0 to 1.0
            sample_texcoord += float2(pixel_offset * pixel_size, 0.0f); // TODO: pixel size removed from this, possible point of failure
        }
        else if (direction == BLUR_VERTICAL)
        {
            float pixel_size = 1.0f / texture_height; // Ditto above
            sample_texcoord += float2(0.0f, pixel_offset * pixel_size); // Ditto above
        }
        
        float4 tex_sample = render_texture.Sample(sampler_linear, sample_texcoord);
	
        float weight = gaussian_weight(blur_radius, pixel_offset);
        total_weight += weight;
        total_weighted_samples += weight * tex_sample;
    }

    float4 output_colour = total_weighted_samples / total_weight;
    return output_colour;
}

float4 ps_gaussian_blur_horiz(vs_screen_quad_output input) : SV_Target
{
    return new_gaussian_blur(input, BLUR_HORIZONTAL);
}

float4 ps_gaussian_blur_vert(vs_screen_quad_output input) : SV_Target
{
    return new_gaussian_blur(input, BLUR_VERTICAL);
}

/* OLD CODE - BLUR STRENGTH WAS AN INT FROM 0-4
// Bell curve distribution
// https://stackoverflow.com/questions/12296372/calculating-offset-of-linear-gaussian-blur-based-on-variable-amount-of-weights
// Pascal triangle rows 10, 14, 18, 22, 16 used, cutting off final 2 values
// Row 10: 252, 210, 120, 45
// Row 14: 3432, 3003, 2002, 1001, 364, 91
// Row 18: 48620, 43758, 31824, 18564, 8568, 3060, 816, 153
// Row 22: 705432, 646646, 497420, 319770, 170544, 74613, 26334, 7315, 1540, 231
// Row 26: 10400600, 9657700, 7726160, 5311735, 3124550, 1562275, 657800, 230230, 65780, 14950, 2600, 325
static const float kernel_offsets_arr[5][6] =
{
    { 0.0f, 1.3636363636f, 0.0f,          0.0f,          0.0f,          0.0f          },
    { 0.0f, 1.4000000000f, 3.2666666667f, 0.0f,          0.0f,          0.0f          },
    { 0.0f, 1.4210526316f, 3.3157894737f, 5.2105263158f, 0.0f,          0.0f          },
    { 0.0f, 1.4347826087f, 3.3478260870f, 5.2608695652f, 7.1739130435f, 0.0f          },
    { 0.0f, 1.4444444444f, 3.3703703704f, 5.2962962963f, 7.2222222222f, 9.1481481481f }
};
static const float kernel_weights_arr[5][6] =
{
    { 0.2514970060f, 0.3293413174f, 0.0f,          0.0f,          0.0f,          0.0f          },
    { 0.2098569157f, 0.3060413355f, 0.0834658188f, 0.0f,          0.0f,          0.0f          },
    { 0.1854974705f, 0.2883642496f, 0.1035153716f, 0.0147879102f, 0.0f,          0.0f          },
    { 0.1681899397f, 0.2727695817f, 0.1169012493f, 0.0240679043f, 0.0021112197f, 0.0f          },
    { 0.1549811418f, 0.2590399085f, 0.1257105438f, 0.0330817221f, 0.0044108963f, 0.0002615156f }
};
static const int kernel_counts[] = { 2, 3, 4, 5, 6 }; // radii

float4 gaussian_blur(vs_output input, uint direction)
{
    float3 tex_colour;
    
    // TODO: calculate weight matrix here based on radius
    // TODO: look into https://gpuopen.com/learn/sampling-normal-gaussian-distribution-gpus/
    
    // Blur right side of the screen first
    if (input.tex_coord.x > blur_x_coverage) // TODO: send this information via cb
    {
        tex_colour = render_texture.Sample(sampler_linear, input.tex_coord).rgb * kernel_weights_arr[blur_strength][0];
    
        [unroll]
        for (int i = 1; i < kernel_counts[blur_strength]; ++i)
        {
            float2 offsets_direction = { 0.0f, 0.0f };
            if (direction == BLUR_HORIZONTAL)
                offsets_direction = float2(kernel_offsets_arr[blur_strength][i], 0.0f);
            else if (direction == BLUR_VERTICAL)
                offsets_direction = float2(0.0f, kernel_offsets_arr[blur_strength][i]);
            
            float2 offset = offsets_direction / texture_width;
        
            tex_colour += render_texture.Sample(sampler_linear, input.tex_coord + offset).rgb * kernel_weights_arr[blur_strength][i];
            tex_colour += render_texture.Sample(sampler_linear, input.tex_coord - offset).rgb * kernel_weights_arr[blur_strength][i];
        }
    }
    else
    {
        tex_colour = render_texture.Sample(sampler_linear, input.tex_coord).rgb;
    }
    
    return float4(tex_colour, 1.0f);
}
*/