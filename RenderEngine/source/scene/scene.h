#pragma once
#include <types.h>
#include <render/camera.h>
#include <render/render.h>
#include <scene/object.h>
#include <vector>

constexpr dword MAXIMUM_SCENE_OBJECTS = 10;
constexpr dword MAXIMUM_SCENE_LIGHTS = MAX_LIGHTS;
//constexpr dword MAXIMUM_SCENE_CAMERAS = 1;

// Scene or 'world' containing multiple objects, lights and cameras
class c_scene
{
public:
	c_scene();
	~c_scene();

	void update(const float delta_time);
	void setup_for_render(c_renderer* const renderer);
	void add_object(c_scene_object* const object);
	std::vector<c_scene_object*>* const get_objects() { return &m_objects; };

	s_light m_lights[MAXIMUM_SCENE_LIGHTS];
	colour_rgba m_ambient_light;
	// TODO: move this to camera
	s_post_parameters_cb m_post_parameters;

	c_camera* m_camera;

private:
	std::vector<c_scene_object*> m_objects;
	//dword m_active_camera;
	//dword m_camera_count;
	//c_camera* m_cameras[MAXIMUM_SCENE_CAMERAS];
};