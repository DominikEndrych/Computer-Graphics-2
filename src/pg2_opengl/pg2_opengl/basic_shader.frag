#version 450 core

// outputs
layout ( location = 0 ) out vec4 FragColor;

in vec3 unified_normal_ws;
in vec3 unified_tangent_ws;
in vec2 texcoord;
in vec3 view_from_ws;	// Camera
in vec3 position_lcs;	//Shadows
in vec3 frag_position_ws;

layout (location = 0) uniform sampler2D irradiance_map;
layout (location = 1) uniform sampler2D normal_map;
layout (location = 2) uniform sampler2D albedo_map;
layout (location = 3) uniform sampler2D environment_map;
layout (location = 4) uniform sampler2D BRDFIntMap;
layout (location = 6) uniform sampler2D RMAMap;

uniform int max_level;

layout (location = 5) uniform sampler2D shadow_map; // light's shadow map

float PI = 3.1415926535897932384626433832795;

vec3 unified_bitangent_ws;

/*
float atan2(in float y, in float x)
{
    bool s = (abs(x) > abs(y));
    return mix(PI/2.0 - atan(x,y), atan(y,x), s);
}
*/

vec2 c2s(vec3 d)
{
	float theta = acos(d.z);
	float phi = atan(d.y / d.x);

	if(d.y < 0) {phi += 2*PI;}

	float u = phi / 2*PI;
	float v = theta / PI;

	return vec2( u, v );
}

// gamma = 2.2f
// exposure = 1.5f
vec3 ToneMapping(vec3 color, const float gamma, const float exposure )
{
	color *= exposure;
	color = color / (color + vec3( 1.0f ) );
	color = pow(color, vec3(1.0f / gamma));
	return color;
}

void main( void )
{	
	//FragColor = vec4( 0.9f, 0.5f, 0.1f, 1.0f );

	// Irradince puvodni
	/*
	vec2 uv = c2s( unified_normal_ws );
	const vec3 irradiance = texture( irradiance_map, uv ).rgb;
	*/

	vec3 albedo = vec3(0.15f, 0.15f, 0.15f);
    //FragColor = vec4( albedo * irradiance, 1.0f );	// albedo + irradiance map

	unified_bitangent_ws = normalize(cross(unified_normal_ws, unified_tangent_ws));	// Bitangent

	// TBN matrix
	mat3x3 TBN = (mat3x3(unified_tangent_ws,unified_bitangent_ws,unified_normal_ws));

	// Local normal
	vec3 bump = texture(normal_map, texcoord).bgr;
	vec3 n_ls = normalize((2.0f * bump) - vec3(1.0f));

	// New world space normal
	vec3 normal_ws = TBN * n_ls;
	//vec3 normal_ws = unified_normal_ws;

	if(dot(view_from_ws - frag_position_ws, normal_ws) < 0.f)
	{
		normal_ws *= -1;
	}

	vec3 albedo_green= vec3(0, 1.0f, 0);

	// Draw normals with bumps
	vec3 n_rgb = normalize((normal_ws * 0.5f + vec3(0.5f)));
	//vec3 n_rgb = (normal_ws + vec3(1.0f)) / 2.0f;

	vec3 albedoColor = texture(albedo_map, texcoord).rgb;

	//FragColor = vec4 (albedoColor * n_rgb, 1.0f);				// Draw albedo with normal map
	//FragColor = vec4 (n_rgb, 1.0f);						// Draw normal map

	//FragColor = vec4 (albedo_green, 1.0f);	

	vec2 uv = c2s( normal_ws );
	const vec3 irradiance = texture( irradiance_map, uv ).rgb;

	//FragColor = vec4( albedoColor * (irradiance / PI), 1.0f );	// Green albedo + irradiance map
	//FragColor = vec4 (albedo_green* n_rgb, 1.0f);			// Green albedo with normal map
    
	// Shadows
	// convert LCS's range <-1, 1> into TC's range <0, 1>
	vec2 a_tc = ( position_lcs.xy + vec2( 1.0f ) ) * 0.5f;
	float depth = texture( shadow_map, a_tc ).r;
	// (pseudo)depth was rescaled from <-1, 1> to <0, 1>
	depth = depth * 2.0f - 1.0f; // we need to do the inverse
	float bias = 0.001f;
	float shadow = (( depth + bias >= position_lcs.z ) ? 1.0f : 0.25f ); // 0.25f represents the amount of shadowin

	// PBR

	vec3 rma = texture(RMAMap, texcoord).rgb;

	vec3 L_r_D = albedoColor * (irradiance / PI);

	// flip
	// dot(normal, omega_o) < 0 do flip

	vec3 omega_o = normalize(view_from_ws - frag_position_ws);	// Direction from camera to fragment

	vec3 omega_i = normalize(reflect(-omega_o, normal_ws));		// Reflection

	float F0 = 0.05f;
	//rma = vec3(0.f);
	float metalness = rma.x;
	float roughness = rma.y;
	//float metalness = 0.0f;
	//float roughness = 0.0f;
	float alpha = pow(roughness,2);

	float cos_theta_o = dot(normal_ws, omega_o);
	float k_s = F0 + (1.0f - F0) * pow((1.0f - cos_theta_o),5);	// Fresnel

	float k_d = (1.0f-k_s)*(1.0f-metalness);

	//vec3 L_r_S = texture( environment_map, c2s(omega_i) ).rgb; // tady max level
	vec3 L_r_S = textureLod( environment_map, c2s(omega_i), roughness * max_level).rgb;
	
	vec3 brdf = texture(BRDFIntMap, vec2(cos_theta_o, alpha)).rgb;
	float s = brdf.x;
	float b = brdf.y;

	vec3 L_o = vec3(( k_d* L_r_D + ( k_s * s + b ) * L_r_S )) * rma.z;
	FragColor = vec4(ToneMapping(L_o, 2.2f, 1.5f), 1.0f);
	//FragColor = vec4(( k_d* L_r_D + ( k_s * s + b ) * L_r_S ), 1.0f) * shadow;
	//FragColor = vec4(normal_ws, 1.0f);
	FragColor = vec4(L_r_D, 1.0f) * shadow;

	// Complete PBR
	/*
		L_o = output
		k_D = 
		f0 = plast/kov, 0.005
		F = klasický fresnel vzorec
		omega_o = smer od kamery do bodu kde jsem
		(s,b) = (red, green) z BRDF mapy
		mtallness = ze souboru
		L_r_D = albedo * irradiance
		L_r_S = PrefEnvMap (odrazy), omega_i je funkce reflect(-omega_o)
		k_S = ??
		brdf_integr = ??

	*/
	//vec3 L_o = ( k_D * L_r_D + ( k_S * brdf_integr.x + brdf_integr.y ) * L_r_S ) * ao * shadow;

	//FragColor = vec4(L_r_D, 1.0f) * shadow;
	//FragColor = vec4(L_r_D, 1.0f);

	// Drawing unified normals
	//vec3 n_unified_rgb = (unified_normal_ws + vec3(1,1,1)) / 2;
	//FragColor = vec4 ( n_unified_rgb, 1.0f );
	
}
