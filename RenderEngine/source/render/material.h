#pragma once
#include <types.h>

// Ensure this is aligned properly for constant buffer
struct s_material
{
	s_material()
		: m_albedo(1.0f, 1.0f, 1.0f, 1.0f)
		, m_roughness(1.0f)
		, m_metallic(1.0f)
		, m_use_albedo_texture(false)
		, m_use_roughness_texture(false)
		, m_use_metallic_texture(false)
		, m_use_normal_texture(false)
		, m_render_texture(false)
		, m_padding()
	{}

	vector4d m_albedo;
	//--------------------------- (16 byte boundary)
	float m_roughness;
	float m_metallic;
	dword m_use_albedo_texture;
	dword m_use_roughness_texture;
	//--------------------------- (16 byte boundary)
	//--------------------------- (16 byte boundary)
	dword m_use_metallic_texture;
	dword m_use_normal_texture;
	dword m_render_texture;
	float m_padding[1];
	//--------------------------- (16 byte boundary)
};

enum e_texture_type;
class c_render_texture;
class c_renderer;
struct s_texture_resources;
class c_material
{
public:
	c_material(c_renderer* const renderer, const dword maximum_textures);
	~c_material();

	void assign_texture(c_render_texture* const texture);

	const dword get_maximum_textures() const { return m_maximum_textures; };
	c_render_texture* const get_texture(const dword index) const;

	s_material m_properties;

private:
	// linked to e_texture_type counts
	const dword m_maximum_textures;
	dword m_texture_count;
	c_render_texture** m_textures;
	// TODO: c_shader
};