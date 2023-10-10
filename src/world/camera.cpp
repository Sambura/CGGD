#define _USE_MATH_DEFINES

#include "camera.h"

#include "utils/error_handler.h"

#include <math.h>

/*
	In our world y direction is up, x and z are planar
	our coordinate system is right-handed
*/
using namespace cg::world;

cg::world::camera::camera() : theta(0), phi(0), height(1080), width(1920),
							  aspect_ratio(1920.f / 1080.f), field_of_view(1.04719f),
							  z_near(0.001f), z_far(100), position(float3{0, 0, 0}) {}

cg::world::camera::~camera() {}

constexpr float deg2rad = static_cast<float>(M_PI) / 180.f;

void cg::world::camera::set_position(float3 in_position) { position = in_position; }
void cg::world::camera::set_theta(float in_theta) { theta = in_theta * deg2rad; }
void cg::world::camera::set_phi(float in_phi) { phi = in_phi * deg2rad; }
void cg::world::camera::set_field_of_view(float in_aov) { field_of_view = in_aov * deg2rad; }
void cg::world::camera::set_z_near(float in_z_near) { z_near = in_z_near; }
void cg::world::camera::set_z_far(float in_z_far) { z_far = in_z_far; }
void cg::world::camera::set_height(float in_height) {
	height = in_height;
	aspect_ratio = width / height;
}
void cg::world::camera::set_width(float in_width) {
	width = in_width;
	aspect_ratio = width / height;
}

// Returns a matrix that transforms vectors from the world basis
// to the camera space. (idk what is in w coordinate tho)
const float4x4 cg::world::camera::get_view_matrix() const {
	// these are camera's xyz vectors
	// And yes, for some reason our camera's look direction 
	// and z-axis are opposites ¯\_(ツ)_/¯
	float3 z_axis = -get_forward();
	float3 x_axis = cross(float3{ 0, 1, 0 }, z_axis);
	float3 y_axis = cross(z_axis, x_axis);

	return float4x4 { 
		{x_axis.x, y_axis.x, z_axis.x, 0}, 
		{x_axis.y, y_axis.y, z_axis.y, 0}, 
		{x_axis.z, y_axis.z, z_axis.z, 0}, 
		{-dot(x_axis, position), -dot(y_axis, position), -dot(z_axis, position), 1}
	};
}

#ifdef DX12
const DirectX::XMMATRIX cg::world::camera::get_dxm_view_matrix() const
{
	// TODO Lab: 3.08 Implement `get_dxm_view_matrix`, `get_dxm_projection_matrix`, and `get_dxm_mvp_matrix` methods of `camera`
	return  DirectX::XMMatrixIdentity();
}

const DirectX::XMMATRIX cg::world::camera::get_dxm_projection_matrix() const
{
	// TODO Lab: 3.08 Implement `get_dxm_view_matrix`, `get_dxm_projection_matrix`, and `get_dxm_mvp_matrix` methods of `camera`
	return  DirectX::XMMatrixIdentity();
}

const DirectX::XMMATRIX camera::get_dxm_mvp_matrix() const
{
	// TODO Lab: 3.08 Implement `get_dxm_view_matrix`, `get_dxm_projection_matrix`, and `get_dxm_mvp_matrix` methods of `camera`
	return  DirectX::XMMatrixIdentity();
}
#endif

// Returns matrix for 2D projection transformation
// Transformed vector: { x, y, z, w }
// x, y - projected coordinates
// z - distance to camera metric: 
//			- < 0: too near, clipped
// 			- [0..1]: from nearest to farthest
//			- > 1: too far, clipped 
//		this metric is highly non-linear
// w - magic number, you should divide x, y, z by it
const float4x4 cg::world::camera::get_projection_matrix() const
{
	float f = 1.f / tanf(field_of_view / 2);
	return float4x4 {
		{ f / aspect_ratio, 0, 0, 0 },
		{ 0, f, 0, 0 },
		{ 0, 0, z_far / (z_near - z_far), -1 },
		{ 0, 0, (z_far * z_near) / (z_near - z_far), 0 }
	};
}

// The current world position of the camera
const float3 cg::world::camera::get_position() const { return position; }

// The direction vector of where camera is looking at
// This is a unit vector
const float3 cg::world::camera::get_forward() const {
	return float3{ sin(theta) * cos(phi), sin(phi), -cos(theta) * cos(phi) };
}

// The vector to the right of the camera, when looking in the forward
// direction. This is a unit vector. *Might* break when camera looks straight
// up or down.
const float3 cg::world::camera::get_right() const {
	return cross(get_forward(), float3{ 0, 1, 0 });
}

// The vector from the top of the camera. Is perpendicular to both
// forward and right vectors. This is a unit vector
const float3 cg::world::camera::get_up() const {
	return cross(get_right(), get_forward());
}

// This is camera's `yaw`, in radians. If theta == 0, camera looks in negative z direction.
// if theta == PI / 2, camera looks in positive x direction, and so on.
const float camera::get_theta() const { return theta; }

// This is camera's `pitch`, in radians. If phi == 0, camera looks parallel to the ground
// if phi == PI / 2, camera looks straigt up. if phi == -PI / 2 - camera looks down.
const float camera::get_phi() const { return phi; }

// Camera's near clipping plane distance
const float camera::get_z_near() const { return z_near; }

// Camera's far clipping plane distance
const float camera::get_z_far() const { return z_far; }

// Camera's aspect ratio (width / height)
const float camera::get_aspect_ratio() const { return aspect_ratio; }

// Camera's field of view, in radians
const float camera::get_fov() const { return field_of_view; }

// Camera's viewport width (units?)
const float camera::get_width() const { return width; }

// Camera's viewport (units?)
const float camera::get_height() const { return height; }
