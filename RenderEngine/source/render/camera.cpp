#include "camera.h"
#include <reporting/report.h>

#ifdef API_DX12
#include <DirectXMath.h>
using namespace DirectX;
#endif

c_camera::c_camera(const point3d position, const vector3d look, const view_bounds2d resolution, const bounds2d clip_depth, const float field_of_view, const vector3d up)
	: m_view()
	, m_projection()
	, m_look_direction(look)
	, m_position(position)
	, m_up_vector(up)
{
	this->set_clip_depth(clip_depth);
	this->set_field_of_view(field_of_view);
	this->set_resolution(resolution);
}

//void c_camera::update(dword delta_x, dword delta_y)
//{
//	// Update control?
//
//	// Update look vector
//	this->update_look(delta_x, delta_y);
//	this->update_view();
//}

void c_camera::set_resolution(const view_bounds2d resolution)
{
	if (resolution.width > 0 && resolution.height > 0)
	{
		m_resolution = resolution;
	}
	else
	{
		LOG_WARNING(L"Tried to set invalid resolution! Using default (%dx%d)", RENDER_GLOBALS.render_bounds.width, RENDER_GLOBALS.render_bounds.height);
		m_resolution = RENDER_GLOBALS.render_bounds;
	}
	m_aspect_ratio = static_cast<float>(m_resolution.width) / static_cast<float>(m_resolution.height);
	this->update_projection();
}

void c_camera::set_clip_depth(const bounds2d clip_depth)
{
	if (IN_RANGE_BOUNDS(clip_depth, 0.1f, 100000.0f))
	{
		m_clip_depth = clip_depth;
	}
	else
	{
		LOG_WARNING(L"Tried to set invalid clip depth! Using default (%f-%f)", DEFAULT_CLIP_DEPTH.min, DEFAULT_CLIP_DEPTH.max);
		m_clip_depth = DEFAULT_CLIP_DEPTH;
	}
}

void c_camera::set_field_of_view(const float field_of_view)
{
	if (IN_RANGE_INCLUSIVE(field_of_view, 0.0f, 360.0f))
	{
		m_field_of_view = field_of_view;
	}
	else
	{
		LOG_WARNING(L"Tried to set invalid FOV! Using default (%f)", DEFAULT_FOV);
		m_field_of_view = DEFAULT_FOV;
	}
}

void c_camera::update_look(const int32 delta_x, const int32 delta_y)
{
#ifdef API_DX12
	// Sensitivity factor for mouse movement
	constexpr float sensitivity = 0.001f;

	// Apply sensitivity
	const float dx = delta_x * sensitivity; // Yaw change
	const float dy = delta_y * sensitivity; // Pitch change

	// Get the current look direction and up vector
	XMVECTOR look_dir_vec = XMLoadFloat3((XMFLOAT3*)&m_look_direction);
	look_dir_vec = XMVector3Normalize(look_dir_vec);
	XMVECTOR up_vec = XMLoadFloat3((XMFLOAT3*)&m_up_vector);
	up_vec = XMVector3Normalize(up_vec);

	// Calculate the camera's right vector
	XMVECTOR right_vec = XMVector3Cross(up_vec, look_dir_vec);
	right_vec = XMVector3Normalize(right_vec);

	// Rotate the lookDir vector left or right based on the yaw
	look_dir_vec = XMVector3Transform(look_dir_vec, XMMatrixRotationAxis(up_vec, dx));
	look_dir_vec = XMVector3Normalize(look_dir_vec);

	// Rotate the lookDir vector up or down based on the pitch
	look_dir_vec = XMVector3Transform(look_dir_vec, XMMatrixRotationAxis(right_vec, dy));
	look_dir_vec = XMVector3Normalize(look_dir_vec);


	// Re-calculate the right vector after the yaw rotation
	right_vec = XMVector3Cross(up_vec, look_dir_vec);
	right_vec = XMVector3Normalize(right_vec);

	// Re-orthogonalize the up vector to be perpendicular to the look direction and right vector
	up_vec = XMVector3Cross(look_dir_vec, right_vec);
	up_vec = XMVector3Normalize(up_vec);

	// Store the updated vectors back to the class members
	XMStoreFloat3((XMFLOAT3*)&m_look_direction, look_dir_vec);
	//XMStoreFloat3((XMFLOAT3*)&m_up_vector, up_vec); // Causes boxes to rotate - wth?
	
	//LOG_MESSAGE(L"m_look_direction: %f, %f, %f", m_look_direction.i, m_look_direction.j, m_look_direction.k);
	//LOG_MESSAGE(L"m_up_vector: %f, %f, %f", m_up_vector.i, m_up_vector.j, m_up_vector.k);
#else
	#warning NO CODE SET FOR UPDATING CAMERA VIEW ON NON DX12!
#endif
}

void c_camera::update_view()
{
#ifdef API_DX12
	// Calculate the look-at point based on the position and look direction
	//XMVECTOR pos_vec = XMLoadFloat3((XMFLOAT3*)&m_position);
	//XMVECTOR look_dir_vec = XMLoadFloat3((XMFLOAT3*)&m_look_direction);
	//XMVECTOR look_at_point = pos_vec + look_dir_vec; // This is the new look-at point

	// Update the view matrix to look from the camera's position to the look-at point
	//XMStoreFloat4x4((XMFLOAT4X4*)&m_view, XMMatrixLookAtLH(pos_vec, look_at_point, XMLoadFloat3((XMFLOAT3*)&m_up_vector)));
	XMStoreFloat4x4((XMFLOAT4X4*)&m_view, XMMatrixLookToLH(XMLoadFloat3((XMFLOAT3*)&m_position), XMLoadFloat3((XMFLOAT3*)&m_look_direction), XMLoadFloat3((XMFLOAT3*)&m_up_vector)));
#else
#warning NO CODE SET FOR UPDATING CAMERA VIEW ON NON DX12!
#endif
}

void c_camera::update_projection()
{
#ifdef API_DX12
	XMStoreFloat4x4
	(
		(XMFLOAT4X4*)&m_projection,
		XMMatrixPerspectiveFovLH
		(
			XMConvertToRadians(m_field_of_view), m_aspect_ratio, m_clip_depth.min, m_clip_depth.max
		)
	);
#else
	#warning NO CODE SET FOR UPDATING CAMERA PROJECTION ON NON DX12!
#endif
}

void c_camera::move_forward(const float distance)
{
	// Get the normalized forward vector (camera's look direction)
	XMVECTOR forwardVec = XMVector3Normalize(XMLoadFloat3((XMFLOAT3*)&m_look_direction));
	XMVECTOR posVec = XMLoadFloat3((XMFLOAT3*)&m_position);

	// Move in the direction the camera is facing
	XMStoreFloat3((XMFLOAT3*)&m_position, XMVectorMultiplyAdd(XMVectorReplicate(distance), forwardVec, posVec));
}

void c_camera::strafe_left(const float distance)
{
	// Get the current look direction and up vector
	XMVECTOR lookDirVec = XMLoadFloat3((XMFLOAT3*)&m_look_direction);
	XMVECTOR upVec = XMLoadFloat3((XMFLOAT3*)&m_up_vector);

	// Calculate the right vector (side vector of the camera)
	XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(upVec, lookDirVec));
	XMVECTOR posVec = XMLoadFloat3((XMFLOAT3*)&m_position);

	// Move left by moving opposite to the right vector
	XMStoreFloat3((XMFLOAT3*)&m_position, XMVectorMultiplyAdd(XMVectorReplicate(-distance), rightVec, posVec));
}

void c_camera::move_up(const float distance)
{
	// Get the current look direction and up vector
	XMVECTOR lookDirVec = XMLoadFloat3((XMFLOAT3*)&m_look_direction);
	XMVECTOR upVec = XMLoadFloat3((XMFLOAT3*)&m_up_vector);

	// Calculate the right vector (side vector of the camera)
	XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(upVec, lookDirVec));
	XMVECTOR posVec = XMLoadFloat3((XMFLOAT3*)&m_position);

	XMVECTOR forwardOrientedUp = XMVector3Normalize(XMVector3Cross(rightVec, lookDirVec));

	// Move in the direction the camera is facing
	XMStoreFloat3((XMFLOAT3*)&m_position, XMVectorMultiplyAdd(XMVectorReplicate(-distance), forwardOrientedUp, posVec));
}

void c_camera::move_backward(const float distance)
{
	// Call MoveForward with negative distance to move backward
	this->move_forward(-distance);
}

void c_camera::strafe_right(const float distance)
{
	// Call StrafeLeft with negative distance to move right
	this->strafe_left(-distance);
}

void c_camera::move_down(const float distance)
{
	this->move_up(-distance);
}
