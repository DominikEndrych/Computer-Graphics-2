#version 450 core

// vertex attributes
layout ( location = 0 ) in vec4 in_position_ms;
layout ( location = 1 ) in vec3 in_normal_ms;
layout ( location = 2 ) in vec3 in_tangent_ms;
layout ( location = 3 ) in vec2 in_texcoord;

// uniform variables
uniform mat4 P; // Model View Projection
uniform mat4 MN;
uniform mat4 M;
uniform vec3 view_from;

uniform mat4 mlp; // Shadow matrix

//Output
out vec3 unified_normal_ws;
out vec3 unified_tangent_ws;
out vec2 texcoord;
out vec3 frag_position_ws;

out vec3 view_from_ws;	// Camera position
out vec3 position_lcs;	// Shadows

void main( void )
{
	gl_Position = P * in_position_ms; // model-space -> clip-space
	vec4 tmp = MN * vec4( in_normal_ms.xyz, 0.f );
	unified_normal_ws = normalize( tmp.xyz);

	tmp = MN * vec4(in_tangent_ms.xyz, 0.0f);
	unified_tangent_ws = normalize(tmp.xyz);

	texcoord = vec2(in_texcoord.x, 1.0f - in_texcoord.y);	// Because of OpenGL

	// Camera position
	//tmp = M * vec4(view_from.xyz, 0.0f);
	view_from_ws = view_from;
	tmp = M * in_position_ms;
	frag_position_ws = tmp.xyz / tmp.w;
	//view_from_ws = normalize(tmp.xyz);

	tmp = mlp * vec4( in_position_ms.xyz, 1.0f );
	position_lcs = tmp.xyz / tmp.w;
}
