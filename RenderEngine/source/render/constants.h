#pragma once
#include <types.h>

enum e_render_pass
{
	_render_pass_deferred,
	_render_pass_pbr,
	_render_pass_texcam,
	_render_pass_post_processing,

	_render_pass_raytrace,

	k_render_pass_count
};

enum e_gbuffers
{
	_gbuffer_albedo,
	_gbuffer_roughness,
	_gbuffer_metallic,
	_gbuffer_normal,
	_gbuffer_position,
	//_gbuffer_depth,

	k_gbuffer_count
};
static const char* const get_gbuffer_name(const e_gbuffers buffer_type);

enum e_light_buffers
{
	//_light_buffer_diffuse,
	//_light_buffer_specular,
	//_light_buffer_ambient,

	k_light_buffer_count
};

struct s_render_globals
{
#ifdef PLATFORM_WINDOWS
	qword hwnd;
#endif
	point2d window_pos;
	view_bounds2d render_bounds;
};
extern s_render_globals RENDER_GLOBALS;

void init_render_globals(qword hwnd);
void update_render_globals();

constexpr wchar_t WINDOW_CLASS_NAME[] = L"Render Engine";
constexpr wchar_t WINDOW_TITLE[] = L"Render Engine";
constexpr dword FRAME_BUFFER_COUNT = 2; // Triple buffering
constexpr colour_rgba CLEAR_COLOUR = { 0.0f, 0.2f, 0.4f, 1.0f };
constexpr dword MAX_LIGHTS = 10;
// maximum preallocated groups of textures in a render target descriptor heap (m_shader_texture_heap)
// max textures per group corresponds to max counts in e_texture_type
// TODO: this isn't fantastic on memory and isn't dynamic, but its easy to implement for now
constexpr dword MAXIMUM_TEXTURE_SETS = 64;