#include "shader.h"
#include <render/render.h>
#ifdef API_DX12
#include <render/api/directx12/helpers.h>
#include <d3d12.h>
#endif

c_shader::c_shader(c_renderer* const renderer, const wchar_t* vs_path, const char* vs_name, const wchar_t* ps_path, const char* ps_name, const e_shader_input input_type)
	: m_resources()
{
	renderer->create_shader(vs_path, vs_name, ps_path, ps_name, input_type, &m_resources);
}

c_shader::~c_shader()
{
#ifdef API_DX12
	ID3DBlob* dx12_vertex_shader = (ID3DBlob*)m_resources.vertex_shader;
	ID3DBlob* dx12_pixel_shader = (ID3DBlob*)m_resources.pixel_shader;
	ID3D12PipelineState* dx12_pipeline_state = (ID3D12PipelineState*)m_resources.pixel_shader;
	SAFE_RELEASE(dx12_vertex_shader);
	SAFE_RELEASE(dx12_pixel_shader);
	SAFE_RELEASE(dx12_pipeline_state);
#endif
}
