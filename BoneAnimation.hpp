#pragma once

#include "MeshBuffer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <map>
#include <string>
#include <vector>

//"BoneAnimation" holds a mesh loaded from a file along with skin weights,
// a heirarchy of bones and their bind info,
// and a collection of animations defined on those bones

struct BoneAnimation {
	//Skinned mesh:
	GLuint vbo = 0;
	MeshBuffer::Attrib Position;
	MeshBuffer::Attrib Normal;
	MeshBuffer::Attrib Color;
	MeshBuffer::Attrib TexCoord;
	MeshBuffer::Attrib BoneWeights;
	MeshBuffer::Attrib BoneIndices;
	MeshBuffer::Mesh mesh;

	//Skeleton description:
	struct Bone {
		std::string name;
		uint32_t parent = -1U;
		glm::mat4x3 inverse_bind_matrix;
	};
	std::vector< Bone > bones;

	//Animation poses:
	struct PoseBone {
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;
	};
	std::vector< PoseBone > frame_bones;

	PoseBone const *get_frame(uint32_t frame) const {
		return &frame_bones[frame * bones.size()];
	}

	//Animation index:
	struct Animation {
		std::string name;
		uint32_t begin = 0;
		uint32_t end = 0;
	};

	std::vector< Animation > animations;


	//construct from a file:
	// note: will throw if file fails to read.
	BoneAnimation(std::string const &filename);

	//look up a particular animation, will throw if not found:
	const Animation &lookup(std::string const &name) const;

	//build a vertex array object that links this vbo to attributes to a program:
	//  will throw if program defines attributes not contained in this buffer
	//  and warn if this buffer contains attributes not active in the program
	GLuint make_vao_for_program(GLuint program) const;
};

struct BoneAnimationPlayer {
	//TODO
};
