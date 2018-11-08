#include "GL.hpp"
#include "Load.hpp"

//CubeReflectProgram draws a surface by using cube-maps for lighting
struct CubeReflectProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
	GLuint object_to_clip_mat4 = -1U;
	GLuint object_to_light_mat4x3 = -1U;
	GLuint normal_to_light_mat3 = -1U;
	GLuint eye_vec3 = -1U; //camera position in lighting space

	//textures:
	//texture0 - cube map (diffuse)
	//texture1 - cube map (shine)

	//attributes:
	//Position (vec4)
	//Normal (vec3)
	//Color (vec4)

	CubeReflectProgram();
};

extern Load< CubeReflectProgram > cube_reflect_program;
