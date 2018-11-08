#include "GL.hpp"
#include "Load.hpp"

//CubeProgram draws a surface by looking up a cube-map using 3D texture coordinates; no lighting is done.
struct CubeDiffuseProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
	GLuint object_to_clip_mat4 = -1U;
	GLuint normal_to_light_mat3 = -1U;

	//textures:
	//texture0 - cube map

	//attributes:
	//Position (vec4)
	//Normal (vec3)
	//Color (vec4)

	CubeDiffuseProgram();
};

extern Load< CubeDiffuseProgram > cube_diffuse_program;
