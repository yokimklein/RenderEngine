#pragma once
#include <types.h>

// Ensure this is aligned properly for constant buffer
struct s_material
{
	s_material()
		: m_emissive(0.0f, 0.0f, 0.0f, 1.0f)
		, m_ambient(1.0f, 1.0f, 1.0f, 1.0f)
		, m_diffuse(1.0f, 1.0f, 1.0f, 1.0f)
		, m_specular(0.19999996f, 0.19999996f, 0.19999996f, 1.0f)
		, m_specular_power(7.843137f)
		, m_use_diffuse_texture(false)
		, m_use_specular_texture(false)
		, m_use_normal_texture(false)
		, m_render_texture(false)
		//, m_padding()
	{}

	vector4d m_emissive;
	//--------------------------- (16 byte boundary)
	vector4d m_ambient;
	//--------------------------- (16 byte boundary)
	vector4d m_diffuse;
	//--------------------------- (16 byte boundary)
	vector4d m_specular;
	//--------------------------- (16 byte boundary)
	float    m_specular_power;
	dword    m_use_diffuse_texture;
	dword    m_use_specular_texture;
	dword    m_use_normal_texture;
	//--------------------------- (16 byte boundary)
	dword    m_render_texture;
	float    m_padding[3];
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