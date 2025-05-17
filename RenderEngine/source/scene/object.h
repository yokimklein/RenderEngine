#pragma once
#include <types.h>
#include <render/camera.h>
#include <render/render.h>
#include <render/model.h>
#include <functional>
#include <render/material.h>

// An instance of a loaded render model with a matrix
// Can have a single render model used by multiple scene objects
class c_scene_object
{
public:
	c_scene_object();
	c_scene_object
	(
		const char* name,
		c_mesh* model,
		c_material* material,
		const point3d position = WORLD_ORIGIN,
		const point3d rotation = DEFAULT_EULER_ROTATION,
		const point3d scale = DEFAULT_SCALE
	);
	~c_scene_object();

	void update(const float delta_time);
	void setup_for_render(c_camera* const camera, c_renderer* const renderer, const dword index);
	void add_update_function(std::function<void()> func);

	const c_mesh* const get_model() const { return m_model; };
	c_material* const get_material() { return m_material; };
	const c_material* const get_material() const { return m_material; };

	const char* m_name;
	c_transform m_transform;

private:
	// TODO: USE SMART POINTERS FOR THESE SO MULTIPLE OBJECTS CAN SHARE AND TRACK REFERENCES W/O DELETING IN USE RESOURCES
	// TODO: multiple meshes w/ their own material in c_model?
	c_mesh* m_model;
	c_material* m_material;
	bool m_has_update_function;
	std::function<void()> m_update_function; // Allows you to attach code to control the object
};