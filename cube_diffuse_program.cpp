#include "cube_diffuse_program.hpp"

#include "compile_program.hpp"
#include "gl_errors.hpp"

CubeDiffuseProgram::CubeDiffuseProgram() {
	program = compile_program(
		"#version 330\n"
		"uniform mat4 object_to_clip;\n"
		"uniform mat3 normal_to_light;\n"
		"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	gl_Position = object_to_clip * Position;\n"
		"	normal = normal_to_light * Normal;\n"
		"	color = Color;\n"
		"}\n"
		,
		"#version 330\n"
		"uniform samplerCube tex;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec3 light = texture(tex, normal).rgb;\n"
		"	vec3 col = color.rgb * light;\n"
		"	fragColor = vec4(pow(col.r,0.45), pow(col.g,0.45), pow(col.b,0.45), color.a);\n"
		"}\n"
	);

	object_to_clip_mat4 = glGetUniformLocation(program, "object_to_clip");
	normal_to_light_mat3 = glGetUniformLocation(program, "normal_to_light");

	glUseProgram(program);

	GLuint tex_samplerCube = glGetUniformLocation(program, "tex");
	glUniform1i(tex_samplerCube, 0);

	glUseProgram(0);

	GL_ERRORS();
}

Load< CubeDiffuseProgram > cube_diffuse_program(LoadTagInit, [](){
	return new CubeDiffuseProgram();
});
