#pragma once
#include "vector3.h"
#include "matrix4x4.h"

class Camera
{
public:
	Camera();
	Camera(int width, int height, double fov_y, Vector3 view_from, Vector3 view_at, int near, int far);
	void Update();

	#pragma region GET Methods
	Matrix4x4 ViewMatrix() { return _view_matrix; }
	Matrix4x4 ModelMatrix() { return _model_matrix; }
	Matrix4x4 ProjectionMatrix() { return _projection_matrix; }
	Matrix4x4 MVPMatrix() { return _projection_matrix * _view_matrix * _model_matrix; }
	Matrix4x4 MVnMatrix();
	Vector3 ViewFrom() { return _view_from; }
	int width() { return _width; }
	int height() { return _height; }
	#pragma endregion

	void MoveFrom(Vector3 amount) { _view_from = Vector3(_view_from.x + amount.x, _view_from.y + amount.y, _view_from.z + amount.z); Update(); }

	// Testing methods
	void MoveForward(float amount);
	void MoveSide(float amount);
	void MoveUp(float amount);
	void Rotate(float yawSpeed, float pitchSpeed);

private:
	int _width;
	int _height;
	double _fov;								// Field of view
	Vector3 _view_from;							// Origin
	Vector3 _view_at;							// Target
	Vector3 _vector_up = Vector3(0,0,1);		// Up direction
	Matrix4x4 _view_matrix;
	Matrix4x4 _model_matrix;
	Matrix4x4 _projection_matrix;
	Matrix4x4 _mvp_matrix;
	int _near;									// Near plane (n in presentation)
	int _far;									// Far plane

	Matrix4x4 _d_matrix;

	Matrix4x4 CreateViewMatrix();
	Matrix4x4 CreateModelMatrix();
	Matrix4x4 CreateProjectionMatrix();
};

/* MVP matice se dela jako p * V * M !!!! */

