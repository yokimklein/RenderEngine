#include "material.h"
#include <reporting/report.h>
#ifdef API_DX12
#include <render/api/directx12/renderer.h>
#include <render/api/directx12/helpers.h>
#endif
#include <render/texture.h>

c_material::c_material(c_renderer* const renderer, const dword maximum_textures)
	: m_properties()
	, m_textures()
	, m_maximum_textures(maximum_textures)
	, m_texture_count(0)
{
	m_textures = new c_render_texture*[m_maximum_textures];
}

c_material::~c_material()
{
	for (dword i = 0; i < m_maximum_textures; i++)
	{
		if (m_textures[i] != nullptr)
		{
			delete m_textures[i];
		}
	}
	delete[] m_textures;
}

void c_material::assign_texture(c_render_texture* const texture)
{
	const bool texture_invalid = texture == nullptr;
	assert(!texture_invalid);
	if (texture_invalid)
	{
		LOG_WARNING(L"texture was nullptr!");
		return;
	}

	const bool max_textures_reached = !IN_RANGE_COUNT(m_texture_count, 0, m_maximum_textures);
	assert(!max_textures_reached);
	if (max_textures_reached)
	{
		LOG_WARNING(L"could not assign texture m_textures is already full!");
		return;
	}

	const dword texture_index = m_texture_count;
	m_textures[texture_index] = texture;
	m_texture_count += 1;
}

c_render_texture* const c_material::get_texture(const dword index) const
{
	const bool texture_type_invalid = !IN_RANGE_COUNT(index, 0, m_maximum_textures);
	assert(!texture_type_invalid);
	if (texture_type_invalid)
	{
		LOG_WARNING(L"texture index [%d] was out of range of maximum [%d]! returning nullptr", index, m_maximum_textures);
		return nullptr;
	}

	c_render_texture* texture = m_textures[index];
	const bool texture_invalid = texture == nullptr;
	assert(!texture_invalid);
	if (texture_invalid)
	{
		LOG_WARNING(L"texture at index [%d] was nullptr! returning nullptr", index);
		return nullptr;
	}

	return texture;
}
