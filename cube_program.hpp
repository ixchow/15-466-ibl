#include "GL.hpp"
#include "Load.hpp"

//CubeProgram draws a surface by looking up a cube-map using 3D texture coordinates; no lighting is done.
struct CubeProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
	GLuint object_to_clip_mat4 = -1U;

	//textures:
	//texture0 - cube map

	//attributes:
	//Position (vec4 so you can set .w to zero to make sky far away)
	//TexCoord (vec3)

	CubeProgram();
};

extern Load< CubeProgram > cube_program;
