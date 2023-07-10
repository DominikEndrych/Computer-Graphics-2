#version 450 core

void main( void )
{	
	// Things here are done by default
	
	// I want to save shadow map for debug so I need this
	gl_FragColor = vec4(vec3(gl_FragCoord.z), 1.0f);
}
