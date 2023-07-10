#include "pch.h"
#include "tutorials.h"
#include "utils.h"
#include "glutils.h"
#include "matrix4x4.h"
#include "color.h"
#include "texture.h"
#include "objloader.h"

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

#include "Rasterizer.h"

// bod zasahu ve ws je vec4 hit_ws = modelMat * in_position_ms
// omega_o_ws = normalize(view_from - (hit_ws.xyz / hit_ws))

// Napsano v hodine na projektoru
/*
int SetIrradianceMap()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE0, tex_irradiance_map_);

	SetSampler(shader_program, 0, "irraciance_map");	// Nazev je ze zdrojaky shaderu (fragment)
}
*/

#pragma pack( push, 1 ) // 1 B alignment
struct GLMaterial
{
	Color3f diffuse; // 3 * 4 B
	GLbyte pad0[4]; // + 4 B = 16 B
	GLuint64 tex_diffuse_handle{ 0 }; // 1 * 8 B
	GLbyte pad1[8]; // + 8 B = 16 B
	Color3f rma; // 3 * 4 B
	GLbyte pad2[4]; // + 4 B = 16 B
	GLuint64 tex_rma_handle{ 0 }; // 1 * 8 B
	GLbyte pad3[8]; // + 8 B = 16 B
	Color3f normal; // 3 * 4 B
	GLbyte pad4[4]; // + 4 B = 16 B
	GLuint64 tex_normal_handle{ 0 }; // 1 * 8 B
	GLbyte pad5[8]; // + 8 B = 16 B
};
#pragma pack( pop )

void CreateBindlessTexture(GLuint& texture, GLuint64& handle, const int width, const int height, const GLvoid* data)
{
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture); // bind empty texture object to the target
	// set the texture wrapping/filtering options
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// copy data from the host buffer
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0); // unbind the newly created texture from the target
	handle = glGetTextureHandleARB(texture); // produces a handle representing the texture in a shader function
	glMakeTextureHandleResidentARB(handle);
}

void Rasterizer::InitMaterials()
{
	GLMaterial* gl_materials = new GLMaterial[_materials.size()];
	int m = 0;
	for (const auto& material : _materials)
	{
		auto tex_diffuse = material.second->texture(Map::kDiffuse);
		if (tex_diffuse)
		{
			GLuint id = 0;
			CreateBindlessTexture(id, gl_materials[m].tex_diffuse_handle, tex_diffuse->width(), tex_diffuse->height(), tex_diffuse->data());
			gl_materials[m].diffuse = Color3f({ 1.0f, 1.0f, 1.0f }); // white diffuse color
		}
		else
		{
			GLuint id = 0;
			GLubyte data[] = { 255, 255, 255, 255 }; // opaque white
			CreateBindlessTexture(id, gl_materials[m].tex_diffuse_handle, 1, 1, data);
			gl_materials[m].diffuse = material.second->value(Map::kDiffuse);	// second?
		}
		m++;
	}
	GLuint ssbo_materials = 0;
	glGenBuffers(1, &ssbo_materials);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_materials);
	const GLsizeiptr gl_materials_size = sizeof(GLMaterial) * _materials.size();
	glBufferData(GL_SHADER_STORAGE_BUFFER, gl_materials_size, gl_materials, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_materials);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

Vector3 Orthogonal(Vector3 normal)
{
	return (abs(normal.x) > abs(normal.z)) ? Vector3(normal.y, -normal.x, 0.0f) : Vector3(0.0f, normal.z, -normal.y);
}

Vector3 GetHemisphereSample(Vector3 normal)
{
	float xi1 = (float)rand() / RAND_MAX;
	float xi2 = (float)rand() / RAND_MAX;

	float x = 2 * cos(2 * M_PI * xi1) * sqrt(xi2 * (1 - xi2));
	float y = 2 * sin(2 * M_PI * xi1) * sqrt(xi2 * (1 - xi2));
	float z = 1 - (2 * xi2);

	Vector3 result = Vector3(x, y, z);

	result.Normalize();
	if (result.DotProduct(normal) < 0)
	{
		result *= -1;
	}

	return result;
}

Vector3 GetCosineHemisphereSample(Vector3 normal)
{
	float xi1 = (float)rand() / RAND_MAX;
	float xi2 = (float)rand() / RAND_MAX;

	float x = cos(2 * M_PI * xi1) * sqrt(1 - xi2);
	float y = sin(2 * M_PI * xi1) * sqrt(1 - xi2);
	float z = sqrt(xi2);

	Vector3 omega_i = Vector3(x, y, z);		// Sample on hemisphere

	//Transformation to correct coordinates
	Vector3 o2 = Orthogonal(normal);
	Vector3 o1 = o2.CrossProduct(normal);
	Matrix3x3 T_rs = Matrix3x3(o1, o2, normal);

	T_rs.Transpose();		// Switch rows and columns

	return T_rs * omega_i;
	//return omega_i;
}

double ConvertRange(double OldValue, double OldMin, double OldMax, double NewMin, double NewMax)
{
	double OldRange = (OldMax - OldMin);
	double NewRange = (NewMax - NewMin);
	double NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin;
	return NewValue;
}

Vector2 c2s(Vector3 d)
{
	float theta = acos(d.z);
	//float phi = atan(d.y / d.x);
	float phi = atan2f(d.y,d.x);

	if (d.y < 0) { phi += 2 * M_PI; }

	float u = phi / 2 * M_PI;
	float v = theta / M_PI;

	return Vector2(u, v);
}

void ComputeIrradianceMap(Texture3f texture)
{
	// TODO: nebud blbej a zeptej se

	int samples = 1000;

	Texture3f newMap = Texture3f(128, 64);

	for (int y = 0; y < newMap.height(); y++)
	{
		for (int x = 0; x < newMap.width(); x++)
		{
			int z = 1;
			Color3f colorSum = Color3f({ 0.0f, 0.0f, 0.0f });
			for (int i = 0; i < samples; i++)
			{
				
				Vector3 position = Vector3((float)x, (float)y, (float)z);
				position.Normalize();

				Vector3 sample = GetCosineHemisphereSample(position);
				sample.Normalize();
				Vector2 spherical = c2s(sample);

				float u = spherical.x;
				float v = spherical.y;
				

				/*
				Vector3 d;
				d.x = sin(y) * cos(x);
				d.y = sin(y) * sin(x);
				//d.z = cos(y);
				d.z = 1.0f;
				d.Normalize();

				Vector3 sample = GetCosineHemisphereSample(d);
				sample.Normalize();
				Vector2 spherical = c2s(sample);
				float u = spherical.x;
				float v = spherical.y;
				*/



				/*
				float theta, phi;
				theta = acosf(position.z);
				phi = atan2f(position.y, position.x) + (position.y < 0 ? 2.0f * M_PI : 0);

				Vector3 normal = Vector3(phi, theta, 1.0f);
				normal.Normalize();

				Vector3 sample = GetCosineHemisphereSample(normal);
				//Vector3 sample = GetHemisphereSample(normal);
				sample.Normalize();
				*/

				/* Only for non-cosine sampling
				float theta_i = acosf(sample.z);
				float probability = cos(theta_i) / (cos(theta_i) / M_PI);
				*/
				
				/*
				float u = 0.5f + (atan2f(sample.x, sample.z));
				float v = 0.5f + (asin(sample.y));
				*/
				
				Color3f color = texture.texel(u, v);
				//color.reverse();
				colorSum += color;

				//colorSum += (color * probability);
			}

			Color3f pixel;
			pixel.data[0] = colorSum.data[0] / (float)samples;
			pixel.data[1] = colorSum.data[1] / (float)samples;
			pixel.data[2] = colorSum.data[2] / (float)samples;

			newMap.set_pixel(x, y, pixel);
		}
	}

	newMap.Save("iradiance.exr");
	printf("Texture saved \n");
	/*
		float theta = acosf(sample.z);
		float phi = atan2f(sample.y, sample.x) + (sample.y < 0 ? 2 * M_PI : 0);

		float u = 1.0f - (phi * 0.5f * (1 / M_PI));
		float v = theta * (1 / M_PI);
	*/



	/*
		Normala co predavat do cosinoveho samplovani ma v sobe souradnice, kterymi se pohybuju v originalni texture
		Bude to teda vypadat nejak takto: (polar, azimut, 1)
		L_i je proste jen nactena mapa
	*/
}

int Rasterizer::InitIrradianceMap(const std::string& file_name)
{
	Texture3f irradiance_map = Texture3f(file_name);

	glGenTextures(1, &_tex_irradiance_map);
	glBindTexture(GL_TEXTURE_2D, _tex_irradiance_map);
	if (glIsTexture(_tex_irradiance_map))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// for HDR images use GL_RGB32F or GL_RGB16F as internal format !!!
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
			irradiance_map.width(), irradiance_map.height(), 0,
			GL_RGB, GL_FLOAT, irradiance_map.data());
		//glGenerateMipmap( GL_TEXTURE_2D );
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	return S_OK;
}

int Rasterizer::SetIrradianceMap()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _tex_irradiance_map);

	SetSampler(_shader_program, 0, "irradiance_map");

	return S_OK;
}

int Rasterizer::InitNormalMap(const std::string& file_name)
{
	Texture3u normal_map = Texture3u(file_name);

	glGenTextures(1, &_tex_normal_map);
	glBindTexture(GL_TEXTURE_2D, _tex_normal_map);
	if (glIsTexture(_tex_normal_map))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// for HDR images use GL_RGB32F or GL_RGB16F as internal format !!!
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,
			normal_map.width(), normal_map.height(), 0,
			GL_RGB, GL_UNSIGNED_BYTE, normal_map.data());
		//glGenerateMipmap( GL_TEXTURE_2D );
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	return S_OK;
}

int Rasterizer::SetNormalMap()
{
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, _tex_normal_map);

	SetSampler(_shader_program, 1, "normal_map");

	return S_OK;
}

int Rasterizer::InitAlbedo(const std::string& file_name)
{
	Texture3f albedo = Texture3f(file_name);

	glGenTextures(1, &_tex_albedo);
	glBindTexture(GL_TEXTURE_2D, _tex_albedo);
	if (glIsTexture(_tex_albedo))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// for HDR images use GL_RGB32F or GL_RGB16F as internal format !!!
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
			albedo.width(), albedo.height(), 0,
			GL_RGB, GL_FLOAT, albedo.data());
		//glGenerateMipmap( GL_TEXTURE_2D );
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	return S_OK;
}

int Rasterizer::SetAlbedo()
{
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, _tex_albedo);

	SetSampler(_shader_program, 2, "albedo_map");

	return S_OK;
}

int Rasterizer::InitEnvironmentMap(const std::string& file_name)
{
	Texture3f environment_map = Texture3f(file_name);

	glGenTextures(1, &_tex_environment_map);
	glBindTexture(GL_TEXTURE_2D, _tex_environment_map);
	if (glIsTexture(_tex_environment_map))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// for HDR images use GL_RGB32F or GL_RGB16F as internal format !!!
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
			environment_map.width(), environment_map.height(), 0,
			GL_RGB, GL_FLOAT, environment_map.data());
		//glGenerateMipmap( GL_TEXTURE_2D );
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	return S_OK;
}

int Rasterizer::SetEnvironmentMap()
{
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, _tex_environment_map);

	SetSampler(_shader_program, 3, "environment_map");

	return S_OK;
}

int Rasterizer::InitEnvironmentMapWithLevel()
{
	std::vector<std::string>file_names = {
		"../../../data/maps/lebombo_prefiltered_env_map_001_2048.exr",
		"../../../data/maps/lebombo_prefiltered_env_map_010_1024.exr",
		"../../../data/maps/lebombo_prefiltered_env_map_100_512.exr",
		"../../../data/maps/lebombo_prefiltered_env_map_250_256.exr",
		"../../../data/maps/lebombo_prefiltered_env_map_500_128.exr",
		"../../../data/maps/lebombo_prefiltered_env_map_750_64.exr",
		"../../../data/maps/lebombo_prefiltered_env_map_999_32.exr"
	};

	const GLint max_level = GLint(file_names.size()) - 1; // assume we have a list of images representing different levels of a map

	SetInt(_shader_program, max_level, "max_level");

	glGenTextures(1, &_tex_environment_map);
	glBindTexture(GL_TEXTURE_2D, _tex_environment_map);
	if (glIsTexture(_tex_environment_map)) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_level);
		int width, height;
		GLint level;

		for (level = 0; level < GLint(file_names.size()); ++level) {
			Texture3f prefiltered_env_map = Texture3f(file_names[level]);
			// for HDR images use GL_RGB32F or GL_RGB16F as internal format !!!
			glTexImage2D(GL_TEXTURE_2D, level, GL_RGB32F, prefiltered_env_map.width(), prefiltered_env_map.height(), 0, GL_RGB, GL_FLOAT,
				prefiltered_env_map.data());
			width = prefiltered_env_map.width() / 2;
			height = prefiltered_env_map.height() / 2;
		}
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	return 0;
}

int Rasterizer::InitBRDFMap(const std::string& file_name)
{
	Texture3f BRDF_map = Texture3f(file_name);

	glGenTextures(1, &_tex_BRDF_map);
	glBindTexture(GL_TEXTURE_2D, _tex_BRDF_map);
	if (glIsTexture(_tex_BRDF_map))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// for HDR images use GL_RGB32F or GL_RGB16F as internal format !!!
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
			BRDF_map.width(), BRDF_map.height(), 0,
			GL_RGB, GL_FLOAT, BRDF_map.data());
		//glGenerateMipmap( GL_TEXTURE_2D );
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	return S_OK;
}

int Rasterizer::SetBRDFMap()
{
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, _tex_BRDF_map);

	SetSampler(_shader_program, 4, "BRDFIntMap");

	return S_OK;
}

int Rasterizer::InitRMAMap(const std::string& file_name)
{
	Texture3f RMA_map = Texture3f(file_name);

	glGenTextures(1, &_tex_RMA_map);
	glBindTexture(GL_TEXTURE_2D, _tex_RMA_map);
	if (glIsTexture(_tex_RMA_map))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// for HDR images use GL_RGB32F or GL_RGB16F as internal format !!!
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F,
			RMA_map.width(), RMA_map.height(), 0,
			GL_RGB, GL_FLOAT, RMA_map.data());
		//glGenerateMipmap( GL_TEXTURE_2D );
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	return S_OK;
}

int Rasterizer::SetRMAMap()
{
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, _tex_RMA_map);

	SetSampler(_shader_program, 6, "RMAMap");

	return S_OK;
}

Rasterizer::Rasterizer(int width, int height, double fov_y, Vector3 view_from, Vector3 view_at, int near, int far)
{
	_camera = Camera(width, height, fov_y, view_from, view_at, near, far);
	_window = nullptr;

	srand(time(NULL));	// Random initialization

	//Texture3f background = Texture3f("../../../data/lebombo_4k.exr");
	//ComputeIrradianceMap(background);

}

/* loading scene graph from obj file */
int Rasterizer::LoadMesh(const std::string& file_name, std::vector<Vertex> &verticesData)
{
	SceneGraph scene;
	//MaterialLibrary materials;

	LoadOBJ(file_name, scene, _materials, false);

	// Added this for data
	auto normal_tex = std::make_shared<Texture3u>("../../../data/cube/scuffed-plastic-normal.png");
	_materials["white_plastic"]->set_texture(Map::kNormal, normal_tex);

	auto rma_tex = std::make_shared<Texture3u>("../../../data/cube/plastic_02_rma.png");
	_materials["white_plastic"]->set_texture(Map::kRMA, rma_tex);

	//materials["white_plastic"]->set_texture(Map::kRMA, normal_tex);		// jenom pro jistotu, tohle jsem tam mel, ale asi je to blbe

	/*
	// build continuous array for GL_TRIANGLES_ADJACENCY primitive mode
	struct Vertex
	{
		Vector3 position; // vertex position 
		Vector3 normal; // vertex normal
		Vector3 color; // vertex color
		Vector2 texture_coord; // vertex texture coordinate
		Vector3 tangent; // vertex tangent
		int material_index; // material index
	};
	*/

	struct TriangleWithAdjacency
	{
		std::array<Vertex, 6> vertices;
	} dst_triangle;

	std::vector<TriangleWithAdjacency> triangles;

	for (SceneGraph::iterator iter = scene.begin(); iter != scene.end(); ++iter)
	{
		const std::string& node_name = iter->first;
		const auto& node = iter->second;

		const auto& mesh = std::static_pointer_cast<Mesh>(node);

		if (mesh)
		{
			for (Mesh::iterator iter = mesh->begin(); iter != mesh->end(); ++iter)
			{
				const auto& src_triangle = Triangle3i(**iter);
				std::shared_ptr<Material> material = iter.triangle_material();
				const int material_index = int(std::distance(std::begin(_materials), _materials.find(material->name())));

				//printf("Triangle:\n");

				for (int i = 0; i < 3; ++i)
				{
					verticesData.push_back({ 
						src_triangle.position(i), 					
						src_triangle.normal(i), 
						src_triangle.tangent(i),
						Vector2(src_triangle.texture_coord(i).x, src_triangle.texture_coord(i).y)
						});

					//dst_triangle.vertices[i * 2].position = src_triangle.position(i);
					//dst_triangle.vertices[i * 2].position.Print();

					//dst_triangle.vertices[i * 2].texture_coord = Vector2(src_triangle.texture_coord(i).x, src_triangle.texture_coord(i).y);

					/*
					dst_triangle.vertices[i * 2].normal = src_triangle.normal(i);
					dst_triangle.vertices[i * 2].color = Vector3(1.0f, 1.0f, 1.0f);
					dst_triangle.vertices[i * 2].texture_coord = Vector2(src_triangle.texture_coord(i).x, src_triangle.texture_coord(i).y);
					dst_triangle.vertices[i * 2].tangent = src_triangle.tangent(i);
					dst_triangle.vertices[i * 2].material_index = material_index;
					*/

					//dst_triangle.vertices[i * 2 + 1].position = src_triangle.adjacent_vertex_position(i).value_or(Vector3());
					/*
					dst_triangle.vertices[i * 2 + 1].normal = Vector3();
					dst_triangle.vertices[i * 2 + 1].color = Vector3();
					dst_triangle.vertices[i * 2 + 1].texture_coord = Vector2();
					dst_triangle.vertices[i * 2 + 1].tangent = Vector3();
					dst_triangle.vertices[i * 2 + 1].material_index = -1;
					*/
				}

				triangles.push_back(dst_triangle);


				//printf("Adjacent vertices:\n");
				/*
				for (int i = 0; i < 3; ++i)
				{
					std::optional<Vector3> av = src_triangle.adjacent_vertex_position(i);
					if (av.has_value())
					{
						//av.value().Print();
					}
					else
					{
						//printf("-\n");
					}
				}
				*/
			}
		}
	}

	return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Rasterizer* handler = reinterpret_cast<Rasterizer*>(glfwGetWindowUserPointer(window));

	if (handler)
	{
		// Move backward
		if (key == GLFW_KEY_S && action == GLFW_REPEAT)
		{
			handler->MoveCameraForward(-2.0f);
		}

		// Move forward
		if (key == GLFW_KEY_W && action == GLFW_REPEAT)
		{
			handler->MoveCameraForward(2.0f);
		}

		if (key == GLFW_KEY_D && action == GLFW_REPEAT)
		{
			handler->MoveCameraSide(2.0f);
		}

		if (key == GLFW_KEY_A && action == GLFW_REPEAT)
		{
			handler->MoveCameraSide(-2.0f);
		}

		// Rotate right
		if (key == GLFW_KEY_LEFT && action == GLFW_REPEAT)
		{
			handler->RotateCamera(0.01f, .0f);
		}

		// Rotate left
		if (key == GLFW_KEY_RIGHT && action == GLFW_REPEAT)
		{
			handler->RotateCamera(-0.01f, .0f);
		}

		if (key == GLFW_KEY_UP && action == GLFW_REPEAT)
		{
			handler->RotateCamera(.0f, 0.01f);
		}

		if (key == GLFW_KEY_DOWN && action == GLFW_REPEAT)
		{
			handler->RotateCamera(.0f, -0.01f);
		}

		if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_REPEAT)
		{
			handler->MoveCameraUp(2.0f);
		}

		if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_REPEAT)
		{
			handler->MoveCameraUp(-2.0f);
		}

		// ESC to close the window
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, true);
		}
		
	}
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	Rasterizer* handler = reinterpret_cast<Rasterizer*>(glfwGetWindowUserPointer(window));
	if (handler)
	{
		float horizontal = xpos - (handler->CameraWidth() / 2);
		float vertical = ypos - (handler->CameraHeight() / 2);
		float speed = 0.01;

		handler->RotateCamera(-horizontal * speed, -vertical * speed);
	}
}


int Rasterizer::InitOpenGL(int width, int height)
{
	glfwSetErrorCallback(glfw_callback);

	if (!glfwInit())
	{
		return(EXIT_FAILURE);
	}


	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 8);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);

	_window = glfwCreateWindow(width, height, "PG2 OpenGL", nullptr, nullptr);
	if (!_window)
	{
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwSetFramebufferSizeCallback(_window, framebuffer_resize_callback);
	glfwMakeContextCurrent(_window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		if (!gladLoadGL())
		{
			return EXIT_FAILURE;
		}
	}

	glEnable(GL_DEBUG_OUTPUT);

	//glEnable(GL_DEPTH_TEST);	// Added this for depth
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthRangef(0.0f, 1.0f);

	glDebugMessageCallback(gl_callback, nullptr);

	printf("OpenGL %s, ", glGetString(GL_VERSION));
	printf("%s", glGetString(GL_RENDERER));
	printf(" (%s)\n", glGetString(GL_VENDOR));
	printf("GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	glEnable(GL_MULTISAMPLE);

	// map from the range of NDC coordinates <-1.0, 1.0>^2 to <0, width> x <0, height>
	glViewport(0, 0, width, height);
	// GL_LOWER_LEFT (OpenGL) or GL_UPPER_LEFT (DirectX, Windows) and GL_NEGATIVE_ONE_TO_ONE or GL_ZERO_TO_ONE
	glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);

	glfwSetKeyCallback(_window, key_callback);

	glfwSetCursorPosCallback(_window, cursor_position_callback);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwSetWindowUserPointer(_window, this);

	return EXIT_SUCCESS;
}

void Rasterizer::InitBuffers()
{
	std::vector<Vertex> verticesData;

	std::string piece = "../../../data/cube/piece_02.obj";
	std::string avenger = "../../../data/avenger/6887_allied_avenger_gi2.obj";
	std::string avenger_pg1 = "../../../data/avenger_pg1/6887_allied_avenger.obj";
	std::string enclosure = "../../../data/enclosure/enclosure.obj";

	LoadMesh(avenger, verticesData);		// Load mesh from file and save vertices

	// setup vertex buffer as AoS (array of structures)
	/*
	GLfloat vertices[] =
	{
		-0.9f, 0.9f, 0.0f,  0.0f, 1.0f, // vertex 0 : p0.x, p0.y, p0.z, t0.u, t0.v
		0.9f, 0.9f, 0.0f,   1.0f, 1.0f, // vertex 1 : p1.x, p1.y, p1.z, t1.u, t1.v
		0.0f, -0.9f, 0.0f,  0.5f, 0.0f  // vertex 2 : p2.x, p2.y, p2.z, t2.u, t2.v
	};
	*/
	//const int no_vertices = 3;
	const int no_vertices = verticesData.size();
	_no_vertices = no_vertices;
	//const int vertex_stride = sizeof(vertices) / no_vertices;
	const int vertex_stride = sizeof(Vertex);
	// optional index array
	/*
	unsigned int indices[] =
	{
		0, 1, 2
	};
	*/

	_vao = 0;
	glGenVertexArrays(1, &_vao);
	glBindVertexArray(_vao);
	_vbo = 0;
	glGenBuffers(1, &_vbo); // generate vertex buffer object (one of OpenGL objects) and get the unique ID corresponding to that buffer
	glBindBuffer(GL_ARRAY_BUFFER, _vbo); // bind the newly created buffer to the GL_ARRAY_BUFFER target
	glBufferData(GL_ARRAY_BUFFER, vertex_stride * no_vertices, verticesData.data(), GL_STATIC_DRAW); // copies the previously defined vertex data into the buffer's memory
	// vertex position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_stride, 0);
	glEnableVertexAttribArray(0);
	// vertex texture coordinates
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_stride, (void*)(sizeof(float) * 3));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_stride, (void*)offsetof(struct Vertex, normal));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, vertex_stride, (void*)offsetof(struct Vertex, tangent));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, vertex_stride, (void*)offsetof(struct Vertex, texture_coord));
	glEnableVertexAttribArray(3);
	/*
	GLuint ebo = 0; // optional buffer of indices
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Vertex) * verticesData.size(), verticesData.data(), GL_STATIC_DRAW);
	*/
}

void Rasterizer::InitShaders()
{
	// Standart shaders
	_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	std::vector<char> shader_source;
	if (LoadShader("basic_shader.vert", shader_source) == S_OK)
	{
		const char* tmp = static_cast<const char*>(&shader_source[0]);
		glShaderSource(_vertex_shader, 1, &tmp, nullptr);
		glCompileShader(_vertex_shader);
	}
	CheckShader(_vertex_shader);

	_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	if (LoadShader("basic_shader.frag", shader_source) == S_OK)
	{
		const char* tmp = static_cast<const char*>(&shader_source[0]);
		glShaderSource(_fragment_shader, 1, &tmp, nullptr);
		glCompileShader(_fragment_shader);
	}
	CheckShader(_fragment_shader);

	
	// Shadow shaders
	_shadow_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	if (LoadShader("shadow_shader.vert", shader_source) == S_OK)
	{
		const char* tmp = static_cast<const char*>(&shader_source[0]);
		glShaderSource(_shadow_vertex_shader, 1, &tmp, nullptr);
		glCompileShader(_shadow_vertex_shader);
	}
	CheckShader(_shadow_vertex_shader);

	_shadow_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	if (LoadShader("shadow_shader.frag", shader_source) == S_OK)
	{
		const char* tmp = static_cast<const char*>(&shader_source[0]);
		glShaderSource(_shadow_fragment_shader, 1, &tmp, nullptr);
		glCompileShader(_shadow_fragment_shader);
	}
	CheckShader(_shadow_fragment_shader);
	

	// Shader program
	_shader_program = glCreateProgram();
	glAttachShader(_shader_program, _vertex_shader);
	glAttachShader(_shader_program, _fragment_shader);
	glLinkProgram(_shader_program);

	// Shadow program
	_shadow_program = glCreateProgram();
	glAttachShader(_shadow_program, _shadow_vertex_shader);
	glAttachShader(_shadow_program, _shadow_fragment_shader);
	glLinkProgram(_shadow_program);

	// TODO check linking
	glUseProgram(_shader_program);

	glPointSize(10.0f);
	glLineWidth(1.0f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Rasterizer::InitShadowDepthbuffer() // must be called before we enter the main render loop
{
	glGenTextures(1, &_tex_shadow_map); // texture to hold the depth values from the light's perspective
	glBindTexture(GL_TEXTURE_2D, _tex_shadow_map);
	// GL_DEPTH_COMPONENT ... each element is a single depth value. The GL converts it to floating point, multiplies by the signed scale
	// factor GL_DEPTH_SCALE, adds the signed bias GL_DEPTH_BIAS, and clamps to the range [0, 1] – this will be important later
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, _shadow_width, _shadow_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float color[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // areas outside the light's frustum will be lit
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
	glBindTexture(GL_TEXTURE_2D, 0);
	glGenFramebuffers(1, &_fbo_shadow_map); // new frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, _fbo_shadow_map);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _tex_shadow_map, 0); // attach the texture as depth
	//glDrawBuffer(GL_NONE); // we dont need any color buffer during the first pass

	// Lines for saving texture
	GLuint rbo_color;
	glGenRenderbuffers(1, &rbo_color);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo_color);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8,
		_shadow_width, _shadow_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_RENDERBUFFER, rbo_color);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // bind the default framebuffer back
}



void Rasterizer::MainLoop()
{
	glfwSetCursorPos(_window, _camera.width() / 2, _camera.height() / 2);

	bool firstPass = true;
	// main loop
	while (!glfwWindowShouldClose(_window))
	{
		glfwSetCursorPos(_window, _camera.width() / 2, _camera.height() / 2);
		//Camera light(_camera.width(), _camera.height(), deg2rad(67.38f), Vector3(-50.f, -50.f, 330.f), Vector3(0, 0, 0), 1, 25);
		Camera light(_camera.width(), _camera.height(), deg2rad(67.38f), Vector3(10.f, 10.f, 330.f), Vector3(0, 0, 0), 20, 300);

		// --- first pass ---
		// set the shadow shader program and the viewport to match the size of the depth map
		glUseProgram(_shadow_program);
		glViewport(0, 0, _shadow_width, _shadow_height);
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo_shadow_map);
		glClear(GL_DEPTH_BUFFER_BIT);
		// set up the light source through the MLP matrix
		//Matrix4x4 mlp = BuildMLPMatrix();
		Matrix4x4 mlp = light.MVPMatrix();
		SetMatrix4x4(_shadow_program, mlp.data(), "mlp");
		// draw the scene
		glBindVertexArray(_vao);
		//glDrawArrays(GL_TRIANGLES, 0, no_triangles_ * 3);
		//glDrawArrays(GL_TRIANGLES, 0, _no_vertices * 3);
		glDrawArrays(GL_TRIANGLES, 0, _no_vertices);
		glBindVertexArray(0);
		// set back the main shader program and the viewport
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, _camera.width(), _camera.height());

		/*
		// Saving shadow map
		Texture4f shadow_map(_shadow_width, _shadow_height);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, _shadow_width, _shadow_height,
			GL_BGRA, GL_FLOAT, shadow_map.data());
		shadow_map.Save("shadow_map.exr");
		*/

		// Standart shaders
		glUseProgram(_shader_program);
		SetIrradianceMap();
		SetNormalMap();
		SetAlbedo();
		SetEnvironmentMap();
		SetBRDFMap();
		SetRMAMap();

		// --- second pass ---
		// everything is the same except this line
		SetMatrix4x4(_shader_program, mlp.data(), "mlp");
		// and also don't forget to set the sampler of the shadow map before entering rendering loop
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, _tex_shadow_map);
		SetSampler(_shader_program, 5, "shadow_map");

		glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // state setting function
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); // state using function

		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		Matrix4x4 P = Matrix4x4();
		//P.set( 0, 0, float( std::min( viewport[2], viewport[3] ) ) / viewport[2] );
		//P.set( 1, 1, float( std::min( viewport[2], viewport[3] ) ) / viewport[3] );
		P.set(0, 0, 100 * 2.0f / viewport[2]);
		P.set(1, 1, 100 * 2.0f / viewport[3]);

		//Testing Camera class
		//Camera camera(640, 480, deg2rad(45.0f), Vector3(5, 3, 7), Vector3(0, 0, 0), 1, 5);
		Matrix4x4 projection = _camera.MVPMatrix();
		Matrix4x4 MVn = _camera.MVnMatrix();
		Matrix4x4 M = _camera.ModelMatrix();

		Vector3 camera_position = _camera.ViewFrom();
		GLfloat camPos[] = { camera_position.x, camera_position.y, camera_position.z };

		//SetMatrix4x4(_shader_program, P.data(), "P");
		SetMatrix4x4(_shader_program, projection.data(), "P");
		SetMatrix4x4(_shader_program, MVn.data(), "MN");
		SetMatrix4x4(_shader_program, M.data(), "M");
		SetVector3(_shader_program, camera_position.data.data(), "view_from");

		/*
		* fov_y = 0.33f
		* n = 20
		* f = 300
		* mlp = p_l . view_L . model
		*/

		glBindVertexArray(_vao);

		//glDrawArrays(GL_POINTS, 0, 0);
		//glDrawArrays(GL_LINE_LOOP, 0, _no_vertices);
		glDrawArrays(GL_TRIANGLES, 0, _no_vertices);
		//glDrawElements( GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0 ); // optional - render from an index buffer

		glfwSwapBuffers(_window);
		glfwPollEvents();
	}

	glDeleteShader(_vertex_shader);
	glDeleteShader(_fragment_shader);

	glDeleteShader(_shadow_vertex_shader);
	glDeleteShader(_shadow_fragment_shader);

	glDeleteProgram(_shadow_program);
	glDeleteProgram(_shader_program);

	glDeleteBuffers(1, &_vbo);
	glDeleteVertexArrays(1, &_vao);

	glfwTerminate();
}