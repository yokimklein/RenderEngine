#include "scene.h"

c_scene::c_scene()
	: m_lights()
	, m_ambient_light(0.0f, 0.0f, 0.0f, 1.0f)
	, m_post_parameters()
{
	m_objects.reserve(MAXIMUM_SCENE_OBJECTS);
	m_camera = new c_camera({ 0.0f, -0.0f, -4.0f }, { 0.0f, 0.0f, 1.0f });
}

c_scene::~c_scene()
{
	for (c_scene_object* object : m_objects)
	{
		delete object;
	}
	delete m_camera;
}

void c_scene::update(const float delta_time)
{
	for (c_scene_object* object : m_objects)
	{
		object->update(delta_time);
	}
}

void c_scene::setup_for_render(c_renderer* const renderer)
{
	// camera
	m_camera->update_view();

	s_light_properties_cb light_constant_buffer;
	point3d camera_pos = m_camera->get_position();
	light_constant_buffer.m_eye_position = XMFLOAT4(camera_pos.x, camera_pos.y, camera_pos.z, 1.0f);
	light_constant_buffer.m_global_ambient = m_ambient_light;
	for (dword i = 0; i < MAXIMUM_SCENE_LIGHTS; i++)
	{
		light_constant_buffer.m_lights[i] = m_lights[i];
	}
	renderer->set_lights_constant_buffer(light_constant_buffer);

	// objects
	dword index = 0;
	for (c_scene_object* object : m_objects)
	{
		object->setup_for_render(m_camera, renderer, index++);
	}

	// Setup blur parameters for its post processing pass
    renderer->set_post_constant_buffer(m_post_parameters);
}

void c_scene::add_object(c_scene_object* const object)
{
	m_objects.push_back(object);
}
