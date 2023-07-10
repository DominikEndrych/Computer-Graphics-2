#version 450 core

// vertex attributes
layout ( location = 0 ) in vec4 in_position_ms;

// uniform variables
// Projection (P_l)*Light (V_l)*Model (M) matrix
uniform mat4 mlp;

void main( void )
{
	gl_Position = mlp * in_position_ms;
}
