#include "object.h"
#include <reporting/report.h>

// default constructor
c_scene_object::c_scene_object()
	: m_transform()
	, m_model(nullptr)
	, m_material()
	, m_has_update_function(false)
{
}

c_scene_object::c_scene_object(const char* name, c_mesh* model, c_material* material, const point3d position, const point3d rotation, const point3d scale)
	: m_name(name)
	, m_transform()
	, m_model(model)
	, m_material(material)
	, m_has_update_function(false)
{
	//if (!c_asset_manager::get()->retrieve_render_model(&m_render_model, rm_dir))
	//{
	//	LOG_WARNING(L"failed to retrieve render model %ls", rm_dir);
	//	m_render_model = nullptr;
	//}
	m_transform.set_position(position);
	m_transform.set_rotation_euler(rotation);
	m_transform.set_scale(scale);
}

c_scene_object::~c_scene_object()
{
	//if (m_render_model != nullptr)
	//	m_render_model->unreference();
	// TODO: UNREFERENCE RATHER THAN DELETE IN CASE BEING USED ELSEWHERE
	if (m_model != nullptr)
		delete m_model;
	if (m_material != nullptr)
		delete m_material;
}

void c_scene_object::update(const float delta_time)
{
	// For attaching movement scripts
	if (m_has_update_function)
	{
		(m_update_function)();
	}
}

void c_scene_object::setup_for_render(c_camera* const camera, c_renderer* const renderer, const dword index)
{
#if API_DIRECTX
	s_object_cb cbuffer;
	cbuffer.m_view = camera->get_view();
	cbuffer.m_projection = camera->get_projection();
	cbuffer.m_world = m_transform.build_matrix();

	// must transpose wvp matrix for the gpu
	XMMATRIX transposed = XMLoadFloat4x4((XMFLOAT4X4*)&cbuffer.m_view);
	transposed = XMMatrixTranspose(transposed);
	XMStoreFloat4x4((XMFLOAT4X4*)&cbuffer.m_view, transposed);
	transposed = XMLoadFloat4x4((XMFLOAT4X4*)&cbuffer.m_projection);
	transposed = XMMatrixTranspose(transposed);
	XMStoreFloat4x4((XMFLOAT4X4*)&cbuffer.m_projection, transposed);
	transposed = XMLoadFloat4x4((XMFLOAT4X4*)&cbuffer.m_world);
	transposed = XMMatrixTranspose(transposed);
	XMStoreFloat4x4((XMFLOAT4X4*)&cbuffer.m_world, transposed);

	// copy our ConstantBuffer instance to the mapped constant buffer resource
	renderer->set_object_constant_buffer(cbuffer, index);

	// copy material data to constant buffer
	s_material_properties_cb material_cb;
	material_cb.m_material = m_material->m_properties;
	renderer->set_material_constant_buffer(material_cb, index);
#endif
}

void c_scene_object::add_update_function(std::function<void()> func)
{
	m_has_update_function = true;
	m_update_function = func;
}
