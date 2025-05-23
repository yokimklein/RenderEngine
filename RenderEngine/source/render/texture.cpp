#include "texture.h"
#include <render/render.h>
#include <render/material.h>
#ifdef API_DX12
#include <render/api/directx12/helpers.h>
#include <d3d12.h>
#endif

c_render_texture::c_render_texture(c_renderer* const renderer, const wchar_t* const file_path, e_texture_type type = _texture_albedo)
	: m_type(type)
{
	const bool texture_loaded = renderer->load_texture(m_type, file_path, &m_resources);
	assert(texture_loaded);
}

c_render_texture::~c_render_texture()
{
#ifdef API_DX12
	ID3D12Resource* dx12_resource = (ID3D12Resource*)m_resources.resource;
	SAFE_RELEASE(dx12_resource)
#endif
}
