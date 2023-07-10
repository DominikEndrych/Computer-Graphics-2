#include "pch.h"
#include "tutorials.h"
#include "utils.h"
#include "glutils.h"
#include "matrix4x4.h"
#include "color.h"
#include "texture.h"
#include "objloader.h"
#include <OpenImageDenoise/oidn.h>

#include "Camera.h"
#include "Rasterizer.h"

/* OpenGL check state */
bool check_gl( const GLenum error )
{
	if ( error != GL_NO_ERROR )
	{
		//const GLubyte * error_str;
		//error_str = gluErrorString( error );
		//printf( "OpenGL error: %s\n", error_str );

		return false;
	}

	return true;
}

/* glfw callback */
void glfw_callback( const int error, const char * description )
{
	printf( "GLFW Error (%d): %s\n", error, description );
}

/* OpenGL messaging callback */
void GLAPIENTRY gl_callback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, const void * user_param )
{
	printf( "GL %s type = 0x%x, severity = 0x%x, message = %s\n",
		( type == GL_DEBUG_TYPE_ERROR ? "Error" : "Message" ),
		type, severity, message );
}

/* invoked when window is resized */
void framebuffer_resize_callback( GLFWwindow * window, int width, int height )
{
	glViewport( 0, 0, width, height );
}

/* load shader code from the text file */
int LoadShader( const std::string & file_name, std::vector<char> & shader )
{
	FILE * file = fopen( file_name.c_str(), "rt" );

	if ( !file )
	{
		printf( "IO error: File '%s' not found.\n", file_name.c_str() );

		return S_FALSE;
	}

	int result = S_FALSE;

	const size_t file_size = static_cast< size_t >( GetFileSize64( file_name.c_str() ) );

	if ( file_size < 1 )
	{
		printf( "Shader error: File '%s' is empty.\n", file_name.c_str() );
	}
	else
	{
		/* in glShaderSource we don't set the length in the last parameter,
		so the string must be null terminated, therefore +1 and reset to 0 */
		shader.clear();
		shader.resize( file_size + 1 );

		size_t bytes = 0; // number of already loaded bytes

		do
		{
			bytes += fread( shader.data(), sizeof( char ), file_size, file );
		} while ( !feof( file ) && ( bytes < file_size ) );

		if ( !feof( file ) && ( bytes != file_size ) )
		{
			printf( "IO error: Unexpected end of file '%s' encountered.\n", file_name.c_str() );
		}
		else
		{
			printf( "Shader file '%s' loaded successfully.\n", file_name.c_str() );
			result = S_OK;
		}
	}

	fclose( file );
	file = nullptr;

	return result;
}

std::string LoadAsciiFile( const std::string & file_name )
{
	std::ifstream file( file_name, std::ios::in );

	if ( file )
	{
		return ( std::string( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() ) );
	}
	else
	{
		return "";
	}
}

/* check shader for completeness */
GLint CheckShader( const GLenum shader )
{
	GLint status = 0;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );

	printf( "Shader compilation %s.\n", ( status == GL_TRUE ) ? "was successful" : "FAILED" );

	if ( status == GL_FALSE )
	{
		int info_length = 0;
		glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &info_length );
		char * info_log = new char[info_length];
		memset( info_log, 0, sizeof( *info_log ) * info_length );
		glGetShaderInfoLog( shader, info_length, &info_length, info_log );

		printf( "Error log: %s\n", info_log );

		SAFE_DELETE_ARRAY( info_log );
	}

	return status;
}

/* create a window and initialize OpenGL context */
int tutorial_1( const int width, const int height )
{
	//Rasterizer rasterizer(width, height, deg2rad(67.38f), Vector3(-40.0f,-52.0f, 15.0f), Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1000.0f);
	Rasterizer rasterizer(width, height, deg2rad(67.38f), Vector3(-50.f, -10.f, 330.f), Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1000.0f);
	//Rasterizer rasterizer(width, height, deg2rad(67.38f), Vector3(100, 0, 0), Vector3(0, 0, 0), 1, 1000);
	rasterizer.InitOpenGL(width, height);
	rasterizer.InitBuffers();
	rasterizer.InitShaders();

	rasterizer.InitIrradianceMap("../../../data/maps/lebombo_irradiance_map.exr");
	//rasterizer.SetIrradianceMap();

	rasterizer.InitNormalMap("../../../data/cube/scuffed-plastic-normal.png");
	//rasterizer.SetNormalMap();

	rasterizer.InitAlbedo("../../../data/cube/scuffed-plastic6-alb.png");
	//rasterizer.SetAlbedo();

	//rasterizer.InitEnvironmentMap("../../../data/maps/lebombo_prefiltered_env_map_001_2048.exr");
	rasterizer.InitEnvironmentMapWithLevel();

	rasterizer.InitBRDFMap("../../../data/maps/brdf_integration_map_ct_ggx.png");

	rasterizer.InitRMAMap("../../../data/cube/plastic_02_rma.png");

	rasterizer.InitShadowDepthbuffer();

	rasterizer.MainLoop();
	/*
	glfwSetErrorCallback( glfw_callback );

	if ( !glfwInit() )
	{
		return( EXIT_FAILURE );
	}

	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 5 );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
	glfwWindowHint( GLFW_SAMPLES, 8 );
	glfwWindowHint( GLFW_RESIZABLE, GL_TRUE );
	glfwWindowHint( GLFW_DOUBLEBUFFER, GL_TRUE );

	GLFWwindow * window = glfwCreateWindow( width, height, "PG2 OpenGL", nullptr, nullptr );
	if ( !window )
	{
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwSetFramebufferSizeCallback( window, framebuffer_resize_callback );
	glfwMakeContextCurrent( window );

	if ( !gladLoadGLLoader( ( GLADloadproc )glfwGetProcAddress ) )
	{
		if ( !gladLoadGL() )
		{
			return EXIT_FAILURE;
		}
	}

	glEnable( GL_DEBUG_OUTPUT );
	glDebugMessageCallback( gl_callback, nullptr );

	printf( "OpenGL %s, ", glGetString( GL_VERSION ) );
	printf( "%s", glGetString( GL_RENDERER ) );
	printf( " (%s)\n", glGetString( GL_VENDOR ) );
	printf( "GLSL %s\n", glGetString( GL_SHADING_LANGUAGE_VERSION ) );

	glEnable( GL_MULTISAMPLE );

	// map from the range of NDC coordinates <-1.0, 1.0>^2 to <0, width> x <0, height>
	glViewport( 0, 0, width, height );
	// GL_LOWER_LEFT (OpenGL) or GL_UPPER_LEFT (DirectX, Windows) and GL_NEGATIVE_ONE_TO_ONE or GL_ZERO_TO_ONE
	glClipControl( GL_UPPER_LEFT, GL_NEGATIVE_ONE_TO_ONE );
	

	// setup vertex buffer as AoS (array of structures)
	GLfloat vertices[] =
	{
		-0.9f, 0.9f, 0.0f,  0.0f, 1.0f, // vertex 0 : p0.x, p0.y, p0.z, t0.u, t0.v
		0.9f, 0.9f, 0.0f,   1.0f, 1.0f, // vertex 1 : p1.x, p1.y, p1.z, t1.u, t1.v
		0.0f, -0.9f, 0.0f,  0.5f, 0.0f  // vertex 2 : p2.x, p2.y, p2.z, t2.u, t2.v
	};
	const int no_vertices = 3;
	const int vertex_stride = sizeof( vertices ) / no_vertices;
	// optional index array
	unsigned int indices[] =
	{
		0, 1, 2
	};

	GLuint vao = 0;
	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );
	GLuint vbo = 0;
	glGenBuffers( 1, &vbo ); // generate vertex buffer object (one of OpenGL objects) and get the unique ID corresponding to that buffer
	glBindBuffer( GL_ARRAY_BUFFER, vbo ); // bind the newly created buffer to the GL_ARRAY_BUFFER target
	glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_STATIC_DRAW ); // copies the previously defined vertex data into the buffer's memory
	// vertex position
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, vertex_stride, 0 );
	glEnableVertexAttribArray( 0 );
	// vertex texture coordinates
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, vertex_stride, ( void * )( sizeof( float ) * 3 ) );
	glEnableVertexAttribArray( 1 );
	GLuint ebo = 0; // optional buffer of indices
	glGenBuffers( 1, &ebo );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ebo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( indices ), indices, GL_STATIC_DRAW );

	GLuint vertex_shader = glCreateShader( GL_VERTEX_SHADER );
	std::vector<char> shader_source;
	if ( LoadShader( "basic_shader.vert", shader_source ) == S_OK )
	{
		const char * tmp = static_cast< const char * >( &shader_source[0] );
		glShaderSource( vertex_shader, 1, &tmp, nullptr );
		glCompileShader( vertex_shader );
	}
	CheckShader( vertex_shader );

	GLuint fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );
	if ( LoadShader( "basic_shader.frag", shader_source ) == S_OK )
	{
		const char * tmp = static_cast< const char * >( &shader_source[0] );
		glShaderSource( fragment_shader, 1, &tmp, nullptr );
		glCompileShader( fragment_shader );
	}
	CheckShader( fragment_shader );

	GLuint shader_program = glCreateProgram();
	glAttachShader( shader_program, vertex_shader );
	glAttachShader( shader_program, fragment_shader );
	glLinkProgram( shader_program );
	// TODO check linking
	glUseProgram( shader_program );

	glPointSize( 10.0f );
	glLineWidth( 1.0f );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	// main loop
	while ( !glfwWindowShouldClose( window ) )
	{
		glClearColor( 0.2f, 0.3f, 0.3f, 1.0f ); // state setting function
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ); // state using function

		GLint viewport[4];
		glGetIntegerv( GL_VIEWPORT, viewport );
		Matrix4x4 P = Matrix4x4();
		//P.set( 0, 0, float( std::min( viewport[2], viewport[3] ) ) / viewport[2] );
		//P.set( 1, 1, float( std::min( viewport[2], viewport[3] ) ) / viewport[3] );
		P.set( 0, 0, 100 * 2.0f / viewport[2] );
		P.set( 1, 1, 100 * 2.0f / viewport[3] );

		//Testing Camera class
		Camera camera(640, 480, deg2rad(45.0f), Vector3(100, 110, 230), Vector3(0, 0, 0), 3, 5);
		Matrix4x4 projection = camera.ProjectionMatrix();

		SetMatrix4x4( shader_program, P.data(), "P" );
		//SetMatrix4x4(shader_program, projection.data(), "P");

		glBindVertexArray( vao );

		glDrawArrays( GL_POINTS, 0, 3 );
		glDrawArrays( GL_LINE_LOOP, 0, 3 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		//glDrawElements( GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0 ); // optional - render from an index buffer

		glfwSwapBuffers( window );
		glfwPollEvents();
	}

	glDeleteShader( vertex_shader );
	glDeleteShader( fragment_shader );
	glDeleteProgram( shader_program );

	glDeleteBuffers( 1, &vbo );
	glDeleteVertexArrays( 1, &vao );

	glfwTerminate();

	*/
	return EXIT_SUCCESS;
}

/* colors */
int tutorial_2()
{
	Color3f color = Color3f::black;

	return 0;
}

/* LDR textures */
int tutorial_3( const std::string & file_name )
{
	Texture texture = Texture3u( file_name ); // gamma compressed sRGB LDR image
	Color3u pixel1 = texture.pixel( texture.width() / 2, texture.height() / 2 );
	Color3u pixel2 = texture.texel( 0.5f, 0.5f );

	return 0;
}

/* HDR textures */
int tutorial_4( const std::string & file_name )
{
	Texture3f image = Texture3f( file_name ); // linear HDR image

	//void * ptr = image.data(); // how to get the raw pointer

	// how to access individual pixels in the image
	for ( int y = 0; y < image.height(); ++y )
	{
		for ( int x = 0; x < image.width(); ++x )
		{
			auto color = image.pixel( x, y );
			color.reverse(); // ex. BGR->RGB
			image.set_pixel( x, y, color );
		}
	}

	// how to save the modified image
	image.Save( file_name + ".exr" );

	return 0;
}

/* vectors */
int tutorial_5()
{
	Vector3 normal = Vector3( 1, 2, 3 );
	normal.Normalize();

	return 0;
}

/* matrices */
int tutorial_6()
{
	Vector3 x, y, z = Vector3( 1, 2, 3 );
	Matrix3x3 Rx = Matrix3x3( x, y, z );
	Matrix3x3 Rxt = Rx.Transpose();

	return 0;
}

/* loading scene graph from obj file */
int tutorial_7( const std::string & file_name )
{
	SceneGraph scene;
	MaterialLibrary materials;

	LoadOBJ( file_name, scene, materials );

	// build continuous array for GL_TRIANGLES_ADJACENCY primitive mode
	struct Vertex
	{
		Vector3 position; /* vertex position */
		Vector3 normal; /* vertex normal */
		Vector3 color; /* vertex color */
		Vector2 texture_coord; /* vertex texture coordinate */
		Vector3 tangent; /* vertex tangent */
		int material_index; /* material index */
	};

	struct TriangleWithAdjacency
	{
		std::array<Vertex, 6> vertices;
	} dst_triangle;

	std::vector<TriangleWithAdjacency> triangles;

	for ( SceneGraph::iterator iter = scene.begin(); iter != scene.end(); ++iter )
	{
		const std::string & node_name = iter->first;
		const auto & node = iter->second;

		const auto & mesh = std::static_pointer_cast< Mesh >( node );

		if ( mesh )
		{
			for ( Mesh::iterator iter = mesh->begin(); iter != mesh->end(); ++iter )
			{
				const auto & src_triangle = Triangle3i( **iter );
				std::shared_ptr<Material> material = iter.triangle_material();
				const int material_index = int( std::distance( std::begin( materials ), materials.find( material->name() ) ) );

				printf( "Triangle:\n" );

				for ( int i = 0; i < 3; ++i )
				{
					dst_triangle.vertices[i * 2].position = src_triangle.position( i );
					dst_triangle.vertices[i * 2].position.Print();
					dst_triangle.vertices[i * 2].normal = src_triangle.normal( i );
					dst_triangle.vertices[i * 2].color = Vector3( 1.0f, 1.0f, 1.0f );
					dst_triangle.vertices[i * 2].texture_coord = Vector2( src_triangle.texture_coord( i ).x, src_triangle.texture_coord( i ).y );
					dst_triangle.vertices[i * 2].tangent = src_triangle.tangent( i );
					dst_triangle.vertices[i * 2].material_index = material_index;

					dst_triangle.vertices[i * 2 + 1].position = src_triangle.adjacent_vertex_position( i ).value_or( Vector3() );
					dst_triangle.vertices[i * 2 + 1].normal = Vector3();
					dst_triangle.vertices[i * 2 + 1].color = Vector3();
					dst_triangle.vertices[i * 2 + 1].texture_coord = Vector2();
					dst_triangle.vertices[i * 2 + 1].tangent = Vector3();
					dst_triangle.vertices[i * 2 + 1].material_index = -1;
				}

				triangles.push_back( dst_triangle );


				printf( "Adjacent vertices:\n" );

				for ( int i = 0; i < 3; ++i )
				{
					std::optional<Vector3> av = src_triangle.adjacent_vertex_position( i );
					if ( av.has_value() )
					{
						av.value().Print();
					}
					else
					{
						printf( "-\n" );
					}
				}
			}
		}
	}

	return 0;
}

int tutorial_8()
{
	Texture3f image = Texture3f( "../../../data/denoise/color_2spp.exr" ); // linear HDR image
	const int width = image.width();
	const int height = image.height();

	void * colorPtr = image.data(); // how to get the raw pointer

	void * albedoPtr = colorPtr; // TODO
	void * normalPtr = colorPtr; // TODO

	Texture3f output = Texture3f( width, height );
	void * outputPtr = output.data();

	// Create an Intel Open Image Denoise device
	OIDNDevice device = oidnNewDevice( OIDN_DEVICE_TYPE_DEFAULT );
	oidnCommitDevice( device );

	// Create a filter for denoising a beauty (color) image using optional auxiliary images too
	OIDNFilter filter = oidnNewFilter( device, "RT" ); // generic ray tracing filter
	oidnSetSharedFilterImage( filter, "color", colorPtr,
		OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0 ); // beauty
	oidnSetSharedFilterImage( filter, "albedo", albedoPtr,
		OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0 ); // auxiliary
	oidnSetSharedFilterImage( filter, "normal", normalPtr,
		OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0 ); // auxiliary
	oidnSetSharedFilterImage( filter, "output", outputPtr,
		OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0 ); // denoised beauty
	oidnSetFilter1b( filter, "hdr", true ); // beauty image is HDR
	oidnCommitFilter( filter );

	// Filter the image
	oidnExecuteFilter( filter );

	// Check for errors
	const char * errorMessage;
	if ( oidnGetDeviceError( device, &errorMessage ) != OIDN_ERROR_NONE )
		printf( "Error: %s\n", errorMessage );

	// Cleanup
	oidnReleaseFilter( filter );
	oidnReleaseDevice( device );

	output.Save( "../../../data/denoise/output.exr" );

	return 0;
}
