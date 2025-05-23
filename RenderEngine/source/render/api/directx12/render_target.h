#pragma once
#include <types.h>
#include <render/constants.h>

// post processing pass stages in sequential order
enum e_post_processing_passes
{
	_post_processing_default,
	_post_processing_blur_horizontal,
	_post_processing_blur_vertical,
	_post_processing_depth_of_field,

	k_post_processing_passes,
	k_post_processing_final = k_post_processing_passes - 1
};

// render targets in sequential order
enum e_render_targets
{
	// Deferred rendering targets - DO NOT MOVE THIS
	_render_target_deferred = 0,

	_render_target_pbr,

	//_render_target_shading,

	_render_target_texcams,

	// Render target count before post processing passes
	k_default_render_target_count,

	// Reserved targets for post processing passes - DO NOT MOVE THESE
	k_render_target_post_reserved = k_default_render_target_count + k_post_processing_passes - 1,

	k_render_target_count,
	k_render_target_final = k_render_target_count - 1
};
static const wchar_t* const get_render_target_name(const e_render_targets target_type);

enum D3D12_RESOURCE_STATES;
enum e_texture_type;
struct ID3D12Device;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;
class c_descriptor_heap;
class c_shader_input;
class c_shader;
class c_render_target
{
public:
	c_render_target(ID3D12Device* const device, c_shader_input* const shader_input, const e_render_targets target_type);
	~c_render_target();

	void begin_render(ID3D12GraphicsCommandList* const command_list, const dword frame_index, const bool clear_buffers = true);
	void assign_texture(ID3D12Resource* const texture_resource, const e_texture_type texture_index, const dword set_index);
	void begin_draw(ID3D12GraphicsCommandList* const command_list, const c_shader* const shader, const dword set_index);

	inline const c_shader_input* const get_shader_input() const { return m_shader_input; };
	ID3D12Resource* const get_frame_resource(const dword target_index, const dword frame_index) const;
	inline ID3D12Resource* const get_depth_resource(const dword frame_index) const { return m_depth_stencil_buffers[frame_index]; };
	D3D12_GPU_DESCRIPTOR_HANDLE get_texture_handle(const dword texture_index, const dword set_index) const;

private:
	c_packed_enum<e_render_targets, dword, _render_target_deferred, k_render_target_count> m_target_type;

	ID3D12Device* const m_device; // local reference, DO NOT clean this up!

	c_descriptor_heap* m_render_target_view_heap; // Render Target View (RTV) Heap, this is where the render target/back buffers are stored
	ID3D12Resource** m_render_target_buffers; // Render target resources in the RTV heap (These are our back buffer textures), this is an array
	// Render target resources are ordered by TARGET, FRAME_INDEX
	// eg for 2 targets with 2 buffered frames:
	// 0 - Target0, Frame0
	// 1 - Target1, Frame0
	// 2 - Target0, Frame1
	// 3 - Target1, Frame1

	// Depth
	c_descriptor_heap* m_depth_stencil_heap;
	ID3D12Resource* m_depth_stencil_buffers[FRAME_BUFFER_COUNT];
	
	// TODO: use a smart pointer for this
	c_shader_input* const m_shader_input; // local reference, DO NOT clean this up!

	// shader resource views for the render pass (in register order) live here
	c_descriptor_heap* m_shader_texture_heap;
};