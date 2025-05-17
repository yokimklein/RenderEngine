#include "model.h"
#ifdef API_DX12
#include <render/api/directx12/helpers.h>
#endif

c_mesh::c_mesh(c_renderer* const renderer, vertex vertices[], dword vertices_size, dword indices[], dword indices_size)
{
	// TODO: MOVE MOST OF THIS CODE BACK INTO C_MODEL
    const bool geometry_loaded = renderer->create_geometry(vertices, vertices_size, indices, indices_size, &m_resources);
    assert(geometry_loaded);
}

c_mesh::c_mesh(c_renderer* const renderer, const wchar_t* const file_path)
{
	const bool model_loaded = renderer->load_model(file_path, &m_resources);
	assert(model_loaded);
}

c_mesh::~c_mesh()
{
#ifdef API_DX12
	SAFE_RELEASE(m_resources.vertex_buffer);
	SAFE_RELEASE(m_resources.index_buffer);
#endif
}