#pragma once
#include <types.h>
#include <render/constants.h>
#include <d3d12.h> // TODO: reduce reliance on this

enum e_constant_buffers
{
	// deferred render pass
	_deferred_constant_buffer_object,
	_deferred_constant_buffer_materials,
	k_deferred_constant_buffer_count,

	_lighting_constant_buffer_lights = 0,
	k_lighting_constant_buffer_count,

	_texcam_constant_buffer_object = 0,
	k_texcam_constant_buffer_count,

	// post processing render pass
	_post_constant_buffer = 0,
	k_post_constant_buffer_count,

	_raytrace_constant_buffer = 0,
	k_raytrace_cosntant_buffer_count
};
static const wchar_t* const get_constant_buffer_name(const e_render_pass render_pass, const e_constant_buffers buffer_type);

//enum D3D12_SHADER_VISIBILITY;
//struct ID3D12Device;
//struct ID3D12Resource;
class c_constant_buffer
{
public:
	c_constant_buffer(ID3D12Device5* const device, const e_render_pass render_pass,
		const e_constant_buffers buffer_type, const dword buffer_struct_size, const D3D12_SHADER_VISIBILITY visibility);
	~c_constant_buffer();

	void set_data(const void* const buffer, const dword frame_index, const dword sub_index);

	inline const D3D12_SHADER_VISIBILITY get_visibility() const { return m_visibility; };
	D3D12_GPU_VIRTUAL_ADDRESS get_gpu_address(const dword frame_index, const dword buffer_index);

	const dword get_buffer_size() const { return m_buffer_aligned_size; };

private:
	const dword m_buffer_struct_size;
	const dword m_buffer_aligned_size;
	const D3D12_SHADER_VISIBILITY m_visibility;
	ID3D12Resource* m_upload_buffers[FRAME_BUFFER_COUNT];
	ubyte* m_gpu_address[FRAME_BUFFER_COUNT];
};