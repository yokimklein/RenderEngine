#pragma once
#include <types.h>
#include <render/render.h>
#ifdef API_DX12
#include <d3d12.h>
#endif

struct s_geometry_resources
{
#ifdef API_DX12
	ID3D12Resource* vertex_buffer; // GPU memory
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	dword vertex_count;
	ID3D12Resource* index_buffer; // GPU memory
	D3D12_INDEX_BUFFER_VIEW index_buffer_view;
	dword index_count;
#endif
};

class c_mesh
{
public:
	c_mesh(c_renderer* const renderer, vertex vertices[], dword vertices_size, dword indices[], dword indices_size);
	c_mesh(c_renderer* const renderer, const wchar_t* const file_path);
	~c_mesh();

	const s_geometry_resources* const get_resources() const { return &m_resources; };

private:
	s_geometry_resources m_resources;
};