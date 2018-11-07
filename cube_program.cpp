#include "cube_program.hpp"

#include "compile_program.hpp"
#include "gl_errors.hpp"

CubeProgram::CubeProgram() {
	program = compile_program(
		"#version 330\n"
		"uniform mat4 object_to_clip;\n"
		"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
		"in vec3 TexCoord;\n"
		"out vec3 texCoord;\n"
		"void main() {\n"
		"	gl_Position = object_to_clip * Position;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
		,
		"#version 330\n"
		"uniform samplerCube tex;\n"
		"in vec3 texCoord;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec3 col = texture(tex, texCoord).rgb;\n"
		"	fragColor = vec4(pow(col.r,0.45), pow(col.g,0.45), pow(col.b,0.45), 1.0);\n"
		"}\n"
	);

	object_to_clip_mat4 = glGetUniformLocation(program, "object_to_clip");

	glUseProgram(program);

	GLuint tex_samplerCube = glGetUniformLocation(program, "tex");
	glUniform1i(tex_samplerCube, 0);

	glUseProgram(0);

	GL_ERRORS();
}

Load< CubeProgram > cube_program(LoadTagInit, [](){
	return new CubeProgram();
});
