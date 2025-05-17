#pragma once
#include <types.h>
#include <render/constants.h>

// corresponds to which register index the texture gets passed to in the shader
enum e_texture_type
{
	// Default render pass textures
	_texture_diffuse = 0,
	_texture_specular,
	_texture_normal, // a normal map is in TANGENT SPACE: up is (0, 0, 1), relative to surface
	k_default_textures_count,

	// Post processing render pass textures
	_texture_render_target = 0,
	_texture_depth_buffer,
	_texture_blurred_target,
	k_post_textures_count,

	// Lighting render pass textures
	_texture_lighting_position = 0,
	_texture_lighting_normal,
	_texture_lighting_ambient,
	_texture_lighting_diffuse,
	_texture_lighting_specular,
	k_lighting_textures_count,

	// Shading render pass textures
	_texture_shading_ambient_lighting = 0,
	_texture_shading_diffuse_lighting,
	_texture_shading_specular_lighting,
	_texture_shading_albedo,
	_texture_shading_emissive,
	k_shading_textures_count,

	// Texcam render pass
	_texture_cam_render_target = 0,
	k_texcam_textures_count
};

struct s_texture_resources
{
#ifdef API_DX12
	// TODO: forward declare
	void* resource; // ID3D12Resource*
#endif
};

class c_material;
class c_renderer;
class c_render_texture
{
public:
	c_render_texture(c_renderer* const renderer, const wchar_t* const file_path, e_texture_type type);
	~c_render_texture();

	const s_texture_resources* const get_resources() { return &m_resources; };
	const e_texture_type get_type() { return m_type; };

private:
	e_texture_type m_type;
	s_texture_resources m_resources;
};