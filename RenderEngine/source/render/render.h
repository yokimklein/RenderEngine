#pragma once
#include <types.h>
#ifdef PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <wincodec.h>
#endif
#include <render/constants.h>
#include <render/material.h>

// Root constant used for values which change frequently and require fast access
// Float constants have a size limit of 4 bytes (1 double word)
struct s_object_cb
{
	matrix4x4 m_projection;
	matrix4x4 m_view;
	matrix4x4 m_world;
};

struct s_material_properties_cb
{
	s_material m_material;
};

enum e_light_type
{
	_light_point,
	_light_spot,
	_light_direcitonal,

	k_light_type_count
};
const char* const get_light_name(const e_light_type light_type);

struct s_light
{
	s_light()
		: m_position(0.0f, 0.0f, 0.0f, 1.0f)
		, m_direction(0.0f, 0.0f, 1.0f, 0.0f)
		, m_colour(1.0f, 1.0f, 1.0f, 1.0f)
		, m_spot_angle(deg2rad(45.0f))
		, m_constant_attenuation(1.0f)
		, m_linear_attenuation(0.0f)
		, m_quadratic_attenuation(0.235f)
		, m_light_type(_light_point)
		, m_enabled(0)
		, m_padding()
	{
	}

	vector4d m_position;
	//----------------------------------- (16 byte boundary)
	vector4d m_direction;
	//----------------------------------- (16 byte boundary)
	colour_rgba m_colour;
	//----------------------------------- (16 byte boundary)
	float m_spot_angle;
	float m_constant_attenuation;
	float m_linear_attenuation;
	float m_quadratic_attenuation;
	//----------------------------------- (16 byte boundary)
	dword m_light_type;
	dword m_enabled;
	// Add some padding to make this struct size a multiple of 16 bytes.
	dword m_padding[2];
	//----------------------------------- (16 byte boundary)
};  // Total: 80 bytes ( 5 * 16 )


struct s_light_properties_cb
{
	s_light_properties_cb()
		: m_eye_position(0, 0, 0, 1)
		, m_global_ambient()
		, m_lights()
	{}

	vector4d m_eye_position;
	//----------------------------------- (16 byte boundary)
	vector4d m_global_ambient;
	//----------------------------------- (16 byte boundary)
	s_light m_lights[MAX_LIGHTS]; // 80 * 1 bytes
};  // Total: 112 bytes

struct s_post_parameters_cb
{
	s_post_parameters_cb()
		: blur_x_coverage(1.0f)
		, blur_strength(5.0f)
		, texture_width(RENDER_GLOBALS.render_bounds.width)
		, texture_height(RENDER_GLOBALS.render_bounds.height)
		, enable_blur(false)
		, enable_depth_of_field(false)
		, depth_of_field_scale(0.5f)
		, blur_sharpness(4.5f)
		, enable_greyscale(false)
	{}

	float blur_x_coverage; // 0.0fmin - 1.0fmax: How much of the screen is blurred along the x axis from the right-hand side of the screen
	float blur_strength; // 0.0fmin - 10.0fmax, blur strength levels
	dword texture_width;
	dword texture_height;
	//----------------------------------- (16 byte boundary)
	dword enable_blur;
	dword enable_depth_of_field;
	float depth_of_field_scale;
	float blur_sharpness;
	//----------------------------------- (16 byte boundary)
	dword enable_greyscale;
};

enum e_texture_type;
enum e_shader_input;
struct s_texture_resources;
struct s_geometry_resources;
struct s_shader_resources;
class c_scene;
class c_renderer
{
public:
	c_renderer();

	virtual bool initialise(const HWND hWnd) = 0;
	virtual void render_frame(c_scene* const scene, dword fps_counter) = 0;
	virtual void set_object_constant_buffer(const s_object_cb& cbuffer, const dword object_index) = 0;
	virtual void set_material_constant_buffer(const s_material_properties_cb& cbuffer, const dword object_index) = 0;
	virtual void set_lights_constant_buffer(const s_light_properties_cb& cbuffer) = 0;
	virtual void set_post_constant_buffer(const s_post_parameters_cb& cbuffer) = 0;
	virtual bool wait_for_previous_frame() = 0;
	virtual bool load_texture(const e_texture_type texture_type, const wchar_t* const file_path, s_texture_resources* const out_resources) = 0;
	virtual bool create_geometry(vertex vertices[], dword vertices_size, dword indices[], dword indices_size, s_geometry_resources* const out_resources) = 0;
	virtual bool load_model(const wchar_t* const file_path, s_geometry_resources* const out_resources) = 0;
	virtual bool create_shader(const wchar_t* vs_path, const char* vs_name, const wchar_t* ps_path, const char* ps_name, const e_shader_input input_type, s_shader_resources* out_resources) = 0;
	virtual qword get_gbuffer_textureid(e_gbuffers gbuffer_type) const = 0;

protected:
	s_object_cb m_object_cb; // cb per object
	s_material_properties_cb m_material_properties_cb;
	s_light_properties_cb m_light_properties_cb;
	s_post_parameters_cb m_blur_parameters_cb;
};