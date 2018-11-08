#include "cube_reflect_program.hpp"

#include "compile_program.hpp"
#include "gl_errors.hpp"

CubeReflectProgram::CubeReflectProgram() {
	program = compile_program(
		"#version 330\n"
		"uniform mat4 object_to_clip;\n"
		"uniform mat4x3 object_to_light;\n"
		"uniform mat3 normal_to_light;\n"
		"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"out vec3 position;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	gl_Position = object_to_clip * Position;\n"
		"	position = object_to_light * Position;\n"
		"	normal = normal_to_light * Normal;\n"
		"	color = Color;\n"
		"}\n"
		,
		"#version 330\n"
		"uniform samplerCube diffuse_tex;\n" //blurry world
		"uniform samplerCube reflect_tex;\n" //shiny world
		"uniform vec3 eye;\n" //eye position in lighting space
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec3 n = normalize(normal);\n"
		"	vec3 v = normalize(eye-position);\n"
		"	vec3 light = texture(diffuse_tex, n).rgb;\n"
		"	vec3 col;\n"

		//refract:
		//"	col = texture(reflect_tex, refract(-v, n, 1.0 / 1.325)).rgb;\n"

		//pure mirror:
		//"	col = texture(reflect_tex, reflect(-v, n)).rgb;\n"

		//reflect + refract:
		"	float refl = mix(0.06, 1.0, pow(1.0 - max(0.0, dot(n, v)), 5.0));\n" //schlick's approximation
		"	col = (1.0 - refl) * texture(reflect_tex, refract(-v, n, 1.0 / 1.5)).rgb;\n"
		"	col += refl * texture(reflect_tex, reflect(-v, n)).rgb;\n"

		//Varying roughness:
		//"	col = texture(reflect_tex, reflect(-v, n), round(0.5+0.5*sin(5.0*position.z)) * 4.0).rgb;\n"

		//partial mirror: (~"clear coat", but probably could use angular dependence)
		//"	col = 0.5 * color.rgb * light;\n"
		//"	col += 0.5 * texture(reflect_tex, reflect(-v, n)).rgb;\n"

		"	fragColor = vec4(pow(col.r,0.45), pow(col.g,0.45), pow(col.b,0.45), color.a);\n"
		"}\n"
	);

	object_to_clip_mat4 = glGetUniformLocation(program, "object_to_clip");
	object_to_light_mat4x3 = glGetUniformLocation(program, "object_to_light");
	normal_to_light_mat3 = glGetUniformLocation(program, "normal_to_light");
	eye_vec3 = glGetUniformLocation(program, "eye");

	glUseProgram(program);

	GLuint diffuse_tex_samplerCube = glGetUniformLocation(program, "diffuse_tex");
	glUniform1i(diffuse_tex_samplerCube, 0);

	GLuint reflect_tex_samplerCube = glGetUniformLocation(program, "reflect_tex");
	glUniform1i(reflect_tex_samplerCube, 1);

	glUseProgram(0);

	GL_ERRORS();
}

Load< CubeReflectProgram > cube_reflect_program(LoadTagInit, [](){
	return new CubeReflectProgram();
});
