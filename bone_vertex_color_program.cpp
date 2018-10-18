#include "bone_vertex_color_program.hpp"

#include "compile_program.hpp"

#ifndef STR
#define STR2(X) #X
#define STR(X) STR2(X)
#endif

BoneVertexColorProgram::BoneVertexColorProgram() {
	program = compile_program(
		"#version 330\n"
		"uniform mat4 object_to_clip;\n"
		"uniform mat4x3 object_to_light;\n"
		"uniform mat3 normal_to_light;\n"
		"uniform mat4x3 bones[" STR( BONE_LIMIT ) "];\n"
		"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec4 BoneWeights;\n"
		"in uvec4 BoneIndices;\n"
		"out vec3 position;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	vec3 blended_Position = \n"
		"		( BoneWeights.x * bones[ BoneIndices.x ]\n"
		"		+ BoneWeights.y * bones[ BoneIndices.y ]\n"
		"		+ BoneWeights.z * bones[ BoneIndices.z ]\n"
		"		+ BoneWeights.w * bones[ BoneIndices.w ] ) * Position;\n"
		"	vec3 blended_Normal = \n"
		"		( BoneWeights.x * mat3(bones[ BoneIndices.x ])\n"
		"		+ BoneWeights.y * mat3(bones[ BoneIndices.y ])\n"
		"		+ BoneWeights.z * mat3(bones[ BoneIndices.z ])\n"
		"		+ BoneWeights.w * mat3(bones[ BoneIndices.w ]) ) * Normal;\n" //<-- note: not correct if bones do scaling
		"	gl_Position = object_to_clip * vec4(blended_Position, 1.0);\n"
		"	position = object_to_light * vec4(blended_Position, 1.0);\n"
		"	normal = normal_to_light * blended_Normal;\n"
		"	color = Color;\n"
		"}\n"
		,
		"#version 330\n"
		"uniform vec3 sun_direction;\n"
		"uniform vec3 sun_color;\n"
		"uniform vec3 sky_direction;\n"
		"uniform vec3 sky_color;\n"
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec3 total_light = vec3(0.0, 0.0, 0.0);\n"
		"	vec3 n = normalize(normal);\n"
		"	{ //sky (hemisphere) light:\n"
		"		vec3 l = sky_direction;\n"
		"		float nl = 0.5 + 0.5 * dot(n,l);\n"
		"		total_light += nl * sky_color;\n"
		"	}\n"
		"	{ //sun (directional) light:\n"
		"		vec3 l = sun_direction;\n"
		"		float nl = max(0.0, dot(n,l));\n"
		"		total_light += nl * sun_color;\n"
		"	}\n"
		"	fragColor = vec4(color.rgb * total_light, color.a);\n"
		"}\n"
	);

	object_to_clip_mat4 = glGetUniformLocation(program, "object_to_clip");
	object_to_light_mat4x3 = glGetUniformLocation(program, "object_to_light");
	normal_to_light_mat3 = glGetUniformLocation(program, "normal_to_light");

	bones_mat4x3_array = glGetUniformLocation(program, "bones");

	sun_direction_vec3 = glGetUniformLocation(program, "sun_direction");
	sun_color_vec3 = glGetUniformLocation(program, "sun_color");
	sky_direction_vec3 = glGetUniformLocation(program, "sky_direction");
	sky_color_vec3 = glGetUniformLocation(program, "sky_color");
}

Load< BoneVertexColorProgram > bone_vertex_color_program(LoadTagInit, [](){
	return new BoneVertexColorProgram();
});
