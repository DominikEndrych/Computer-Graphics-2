#include "pch.h"
#include "Camera.h"

Camera::Camera()
{
	return;
}

Camera::Camera(int width, int height, double fov_y, Vector3 view_from, Vector3 view_at, int near, int far)
{
	_width = width;
	_height = height;
	_fov = fov_y;
	_view_from = view_from;
	_view_at = view_at;
	_near = near;
	_far = far;

	Update();
}

void Camera::Update()
{
	_view_matrix = CreateViewMatrix();
	_model_matrix = CreateModelMatrix();
	_projection_matrix = CreateProjectionMatrix();
	//printf("%f, %f, %f \n", _view_from.x, _view_from.y, _view_from.z);
}

Matrix4x4 Camera::CreateViewMatrix()
{
	Vector3 z_e = (_view_from - _view_at) / ((_view_from - _view_at).L2Norm());		// Z axis
	z_e.Normalize();

	Vector3 x_e = _vector_up.CrossProduct(z_e) / (_vector_up.CrossProduct(z_e)).L2Norm();		// X axis
	x_e.Normalize();

	Vector3 y_e = z_e.CrossProduct(x_e);			// Y axis
	y_e.Normalize();

	Matrix4x4 view_result = Matrix4x4(x_e, y_e, z_e, _view_from);
	view_result.EuclideanInverse();		// Matrix needs to be inverted first
	return view_result;
}

Matrix4x4 Camera::CreateModelMatrix()
{
	Matrix4 model_result = Matrix4x4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	return model_result;
}

Matrix4x4 Camera::CreateProjectionMatrix()
{
	double w = (2.0f * _near) * tan(_fov / 2.0f);
	//double h = (2 * _near) * tan(_fov / 2);	// using fov_y = fov_x

	double aspect = (double)_width / (double)_height;
	double h = w / aspect;


	Matrix4x4 normMatrix = Matrix4x4(
		2/w, 0, 0, 0,
		0, 2/h, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
		);

	double a = (_near + _far) / (_near - _far);
	double b = (2 * _near * _far) / (_near - _far);

	Matrix4x4 D_matrix = Matrix4x4(
		_near, 0, 0, 0,
		0, _near, 0, 0,
		0, 0, a, b,
		0, 0, -1, 0
	);
	
	_d_matrix = normMatrix;

	Matrix4x4 projection_result = normMatrix * D_matrix;
	return projection_result;
}

Matrix4x4 Camera::MVnMatrix()
{
	// Identity for now
	Matrix4x4 viewMatrix = Matrix4x4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	//Matrix4x4 MVn = _model_matrix * viewMatrix;
	Matrix4x4 MVn = _model_matrix;
	MVn.EuclideanInverse();
	MVn.Transpose();

	return MVn;
}

void Camera::MoveForward(float amount)
{
	Vector3 ds = _view_at - _view_from;
	ds.Normalize();
	ds *= amount;

	_view_from += ds;
	_view_at += ds;

	Update();
}

void Camera::MoveSide(float amount)
{
	Vector3 view_dir = _view_at - _view_from;
	view_dir.Normalize();

	Vector3 side_dir = view_dir.CrossProduct(_vector_up);
	side_dir.Normalize();

	side_dir *= amount;

	_view_from += side_dir;
	_view_at += side_dir;

	Update();
}

void Camera::MoveUp(float amount)
{
	_view_from.z += amount;
	_view_at.z += amount;
	Update();
}

void Camera::Rotate(float yawSpeed, float pitchSpeed)
{
	Vector3 view_dir = _view_at - _view_from;
	view_dir.Normalize();
	float yaw = atan2f(view_dir.y, view_dir.x) - M_PI_2;
	float pitch = acosf(-view_dir.z) - M_PI_2;

	yaw += yawSpeed;
	pitch += pitchSpeed;
	
	Matrix3x3 R_z = Matrix3x3(
		cos(yaw), -sin(yaw), 0,
		sin(yaw), cos(yaw), 0,
		0, 0, 1
	);

	Matrix3x3 R_x = Matrix3x3(
		1, 0, 0,
		0, cos(pitch), -sin(pitch),
		0, sin(pitch), cos(pitch)
	);

	Vector3 new_view_dir = R_z * R_x * Vector3(0, 1, 0);
	new_view_dir.Normalize();
	_view_at = _view_from + new_view_dir;

	Update();

}
