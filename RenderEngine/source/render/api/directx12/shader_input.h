#pragma once
#include <types.h>
#include <d3d12.h> // TODO: reduce reliance on this type

enum e_shader_input
{
	_input_deferred,
	_input_pbr,
	//_input_shading,
	_input_texcam,

	_input_post_processing,

	k_shader_input_count
};

class c_constant_buffer;
enum e_constant_buffers;
class c_shader_input
{
public:
	c_shader_input(ID3D12Device* const device, const dword textures_count,
		c_constant_buffer* const constant_buffers[], const dword constant_buffer_count,
		const D3D12_INPUT_ELEMENT_DESC input_desc[], const dword input_element_count,
		const D3D12_DESCRIPTOR_RANGE texture_ranges[], const dword texture_range_count,
		const DXGI_FORMAT render_target_formats[], const dword render_target_count,
		const bool use_depth_buffer, const D3D12_COMPARISON_FUNC depth_comparison_func = D3D12_COMPARISON_FUNC_LESS);
	~c_shader_input();

	inline ID3D12RootSignature* const get_root_signature() const { return m_root_signature; };
	inline const D3D12_INPUT_LAYOUT_DESC get_input_layout() const { return m_input_layout; };
	inline c_constant_buffer* const get_constant_buffer(const e_constant_buffers buffer_index) const { return m_constant_buffers[buffer_index]; };
	inline const DXGI_FORMAT get_render_target_format(const dword render_target_index) const
	{ 
		assert(IN_RANGE_COUNT(render_target_index, 0, m_render_target_count));
		return m_render_target_formats[render_target_index];
	};

	const dword m_constant_buffer_count;
	const dword m_root_parameter_count;
	const dword m_textures_root_index; // Additional texture ranges need to be sequential from this starting root parameter index
	const dword m_texture_count;
	const dword m_render_target_count;

	const bool m_uses_depth_buffer;
	const D3D12_COMPARISON_FUNC m_depth_comparison_func;

private:
	ID3D12RootSignature* m_root_signature; // Defines what resources are bound to the graphics pipeline
	D3D12_INPUT_LAYOUT_DESC m_input_layout; // TODO: INVESTIGATE WHETHER DANGLING POINTER LEFT IN HERE
	c_constant_buffer** m_constant_buffers; // array of pointers of count m_constant_buffer_count
	DXGI_FORMAT* const m_render_target_formats; // array of formats
};