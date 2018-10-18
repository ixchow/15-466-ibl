#include "BoneAnimation.hpp"

#include "read_chunk.hpp"
#include "gl_errors.hpp"

#include <set>
#include <fstream>

BoneAnimation::BoneAnimation(std::string const &filename) {
	std::cout << "Reading bone-based animation from '" << filename << "'." << std::endl;

	std::ifstream file(filename, std::ios::binary);

	std::vector< char > strings;
	read_chunk(file, "str0", &strings);

	{ //read bones:
		struct BoneInfo {
			uint32_t name_begin, name_end;
			uint32_t parent;
			glm::mat4x3 inverse_bind_matrix;
		};
		static_assert(sizeof(BoneInfo) == 4*2 + 4 + 4*12, "BoneInfo is packed.");

		std::vector< BoneInfo > file_bones;
		read_chunk(file, "bon0", &file_bones);
		bones.reserve(file_bones.size());
		for (auto const &file_bone : file_bones) {
			if (!(file_bone.name_begin <= file_bone.name_end && file_bone.name_end <= strings.size())) {
				throw std::runtime_error("bone has out-of-range name begin/end");
			}
			if (!(file_bone.parent == -1U || file_bone.parent < bones.size())) {
				throw std::runtime_error("bone has invalid parent");
			}
			bones.emplace_back();
			Bone &bone = bones.back();
			bone.name = std::string(&strings[0] + file_bone.name_begin, &strings[0] + file_bone.name_end);
			bone.parent = file_bone.parent;
			bone.inverse_bind_matrix = file_bone.inverse_bind_matrix;
		}
	}

	static_assert(sizeof(PoseBone) == 3*4 + 4*4 + 3*4, "PoseBone is packed.");
	read_chunk(file, "frm0", &frame_bones);
	if (frame_bones.size() % bones.size() != 0) {
		throw std::runtime_error("frame bones is not divisible by bones");
	}

	uint32_t frames = frame_bones.size() / bones.size();

	{ //read actions (animations):
		struct AnimationInfo {
			uint32_t name_begin, name_end;
			uint32_t begin, end;
		};
		static_assert(sizeof(AnimationInfo) == 4*2 + 4*2, "AnimationInfo is packed.");

		std::vector< AnimationInfo > file_animations;
		read_chunk(file, "act0", &file_animations);
		animations.reserve(file_animations.size());
		for (auto const &file_animation : file_animations) {
			if (!(file_animation.name_begin <= file_animation.name_end && file_animation.name_end <= strings.size())) {
				throw std::runtime_error("animation has out-of-range name begin/end");
			}
			if (!(file_animation.begin <= file_animation.end && file_animation.end <= frames)) {
				throw std::runtime_error("animation has out-of-range frames begin/end");
			}
			animations.emplace_back();
			Animation &animation = animations.back();
			animation.name = std::string(&strings[0] + file_animation.name_begin, &strings[0] + file_animation.name_end);
			animation.begin = file_animation.begin;
			animation.end = file_animation.end;
		}
	}

	{ //read actual mesh:
		struct Vertex {
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::u8vec4 Color;
			glm::vec2 TexCoord;
			glm::vec4 BoneWeights;
			glm::uvec4 BoneIndices;
		};
		static_assert(sizeof(Vertex) == 3*4+3*4+4*1+2*4+4*4+4*4, "Vertex is packed.");
		//GLAttribBuffer< glm::vec3, glm::vec3, glm::u8vec4, glm::vec2, glm::vec4, glm::uvec4 > buffer;
		std::vector< Vertex > data;
		read_chunk(file, "msh0", &data);

		//check bone indices:
		for (auto const &vertex : data) {
			if ( vertex.BoneIndices.x >= bones.size()
				|| vertex.BoneIndices.y >= bones.size()
				|| vertex.BoneIndices.z >= bones.size()
				|| vertex.BoneIndices.w >= bones.size()
			) {
				throw std::runtime_error("animation mesh has out of range vertex index");
			}
		}

		{ //DEBUG: dump bounding box info
			glm::vec3 min = glm::vec3(std::numeric_limits< float >::infinity());
			glm::vec3 max = glm::vec3(-std::numeric_limits< float >::infinity());
			for (auto const &v : data) {
				min = glm::min(min, v.Position);
				max = glm::max(max, v.Position);
			}
			std::cout << "INFO: bounding box of animation mesh in '" << filename << "' is [" << min.x << "," << max.x << "]x[" << min.y << "," << max.y << "]x[" << min.z << "," << max.z << "]" << std::endl;
		}

		//upload data:
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(Vertex), data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//specify the (only) mesh:
		mesh.start = 0;
		mesh.count = data.size();

		//store attributes for later vao creation:
		Position = MeshBuffer::Attrib(3, GL_FLOAT, MeshBuffer::Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, Position));
		Normal = MeshBuffer::Attrib(3, GL_FLOAT, MeshBuffer::Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, Normal));
		Color = MeshBuffer::Attrib(4, GL_UNSIGNED_BYTE, MeshBuffer::Attrib::AsFloatFromFixedPoint, sizeof(Vertex), offsetof(Vertex, Color));
		TexCoord = MeshBuffer::Attrib(2, GL_FLOAT, MeshBuffer::Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, TexCoord));
		BoneWeights = MeshBuffer::Attrib(4, GL_FLOAT, MeshBuffer::Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, BoneWeights));
		BoneIndices = MeshBuffer::Attrib(4, GL_UNSIGNED_INT, MeshBuffer::Attrib::AsInteger, sizeof(Vertex), offsetof(Vertex, BoneIndices));

	}

	GL_ERRORS();
}

const BoneAnimation::Animation &BoneAnimation::lookup(std::string const &name) const {
	for (auto const &animation : animations) {
		if (animation.name == name) return animation;
	}
	throw std::runtime_error("Animation with name '" + name + "' does not exist.");
}

GLuint BoneAnimation::make_vao_for_program(GLuint program) const {


	GL_ERRORS();
	//create a new vertex array object:
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//Try to bind all attributes in this buffer:
	std::set< GLuint > bound;
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	auto bind_attribute = [&](char const *name, MeshBuffer::Attrib const &attrib) {
		if (attrib.size == 0) return; //don't bind empty attribs
		GLint location = glGetAttribLocation(program, name);
		if (location == -1) {
			std::cerr << "WARNING: attribute '" << name << "' in mesh buffer isn't active in program." << std::endl;
		} else {
			attrib.VertexAttribPointer(location);
			glEnableVertexAttribArray(location);
			bound.insert(location);
		}
	};
	bind_attribute("Position", Position);
	bind_attribute("Normal", Normal);
	bind_attribute("Color", Color);
	bind_attribute("TexCoord", TexCoord);
	bind_attribute("BoneWeights", BoneWeights);
	bind_attribute("BoneIndices", BoneIndices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	//Check that all active attributes were bound:
	GLint active = 0;
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &active);
	assert(active >= 0 && "Doesn't makes sense to have negative active attributes.");
	for (GLuint i = 0; i < GLuint(active); ++i) {
		GLchar name[100];
		GLint size = 0;
		GLenum type = 0;
		glGetActiveAttrib(program, i, 100, NULL, &size, &type, name);
		name[99] = '\0';
		GLint location = glGetAttribLocation(program, name);
		if (!bound.count(GLuint(location))) {
			throw std::runtime_error("ERROR: active attribute '" + std::string(name) + "' in program is not bound.");
		}
	}

	GL_ERRORS();

	return vao;
}
