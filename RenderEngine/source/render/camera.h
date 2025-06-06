#pragma once
#include <types.h>
#include <render/render.h>

constexpr float DEFAULT_FOV = 70.f;
constexpr bounds2d DEFAULT_CLIP_DEPTH = { 0.1f, 1000.0f };

class c_camera
{
public:
	c_camera
	(
		const point3d position = WORLD_ORIGIN,
		const vector3d look = WORLD_ORIGIN,
		const view_bounds2d resolution = RENDER_GLOBALS.render_bounds,
		const bounds2d clip_depth = DEFAULT_CLIP_DEPTH,
		const float field_of_view = DEFAULT_FOV,
		const vector3d up = GLOBAL_UP
	);

	//void update(dword delta_x, dword delta_y);
	void update_look(const int32 delta_x, const int32 delta_y);
	void update_view();

	void set_resolution(const view_bounds2d resolution);
	void set_clip_depth(const bounds2d clip_depth);
	void set_field_of_view(const float field_of_view);

	void set_position(const point3d position) { m_position = position; };
	void set_direction(const point3d direction) { m_look_direction = direction; };

	inline const matrix4x4 get_view() const { return m_view; };
	const matrix4x4 get_projection() const { return m_projection; };
	const point3d get_position() const { return m_position; };
	const point3d get_look_direction() const { return m_look_direction; };
	const view_bounds2d get_resolution() const { return m_resolution; };

	void move_forward(const float distance);
	void strafe_left(const float distance);
	void move_backward(const float distance);
	void strafe_right(const float distance);
	void move_up(const float distance);
	void move_down(const float distance);

private:
	void update_projection();

	matrix4x4 m_view;
	matrix4x4 m_projection;

	view_bounds2d m_resolution;
	float m_aspect_ratio;
	float m_field_of_view;
	bounds2d m_clip_depth; // near, far

	vector3d m_look_direction;
	point3d m_position;
	vector3d m_up_vector;
};