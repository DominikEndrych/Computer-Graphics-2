#pragma once
#include "Camera.h"
#include "vector3.h"
#include <string>

// build continuous array for GL_TRIANGLES_ADJACENCY primitive mode
struct Vertex
{
	Vector3 position; // vertex position 
	Vector3 normal; // vertex normal
	Vector3 tangent; // vertex tangent
	Vector2 texture_coord; // vertex texture coordinate
	/*
	Vector3 color; // vertex color
	int material_index; // material index
	*/
};

class Rasterizer
{
public:
	Rasterizer(int width, int height, double fov_y, Vector3 view_from, Vector3 view_at, int near, int far);

	// OpenGL initialization and main loop
	int InitOpenGL(int width, int height);
	int LoadMesh(const std::string& file_name, std::vector<Vertex>& verticesData);
	void InitBuffers();
	void InitShaders();
	void InitShadowDepthbuffer();
	void MainLoop();

	// Irradiance map methods
	int InitIrradianceMap(const std::string& file_name);
	int SetIrradianceMap();

	//Normal map methods
	int InitNormalMap(const std::string& filename);
	int SetNormalMap();

	// Albedo
	int InitAlbedo(const std::string& filename);
	int SetAlbedo();

	// Environment map
	int InitEnvironmentMap(const std::string& filename);
	int SetEnvironmentMap();
	int InitEnvironmentMapWithLevel();

	// BRDF map
	int InitBRDFMap(const std::string& filename);
	int SetBRDFMap();

	// RMA
	int InitRMAMap(const std::string& filename);
	int SetRMAMap();

	void InitMaterials();

	void MoveCamera(Vector3 view_from_move) { _camera.MoveFrom(view_from_move); }

	//Testing methods
	void MoveCameraForward(float amount) { _camera.MoveForward(amount); }
	void MoveCameraSide(float amount) { _camera.MoveSide(amount); }
	void MoveCameraUp(float amount) { _camera.MoveUp(amount); }
	void RotateCamera(float yawSpeed, float pitchSpeed) { _camera.Rotate(yawSpeed, pitchSpeed); }

	float CameraWidth() { return _camera.width(); }
	float CameraHeight() { return _camera.height(); }

	// Cursor
	void LockCursor(bool state) { _lockCursor = state; }
	bool IsCursorLocked() { return _lockCursor; }
	
private:
	Camera _camera;
	bool _lockCursor = false;

	int _no_vertices;
	GLuint _tex_irradiance_map;
	GLuint _tex_normal_map;
	GLuint _tex_albedo;
	GLuint _tex_environment_map;
	GLuint _tex_BRDF_map;
	GLuint _tex_RMA_map;

	// Initialization variables
	GLFWwindow* _window;
	GLuint _vao;
	GLuint _vbo;
	GLuint _shader_program;
	GLuint _vertex_shader;
	GLuint _fragment_shader;
	MaterialLibrary _materials;

	// Shadow mapping
	int _shadow_width{ 1024 };
	int _shadow_height{ _shadow_width };
	GLuint _fbo_shadow_map{ 0 };
	GLuint _tex_shadow_map{ 0 };
	GLuint _shadow_program{ 0 };
	GLuint _shadow_vertex_shader;
	GLuint _shadow_fragment_shader;
};

