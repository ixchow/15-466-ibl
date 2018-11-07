#pragma once

#include "GL.hpp"
#include "gl_errors.hpp"

#include <set>
#include <iostream>

/*
 * Bind a bunch of named attributes in a vertex buffer and check that program has all attribs bound.
 * Example:
 * std::map< std::string, MeshBuffer::Attrib > const map;
 * //... add attribs to map
 * vao = make_vao_for_program(vbo, map.begin(), map.end(), program);
 *
 */

template< typename ITER >
inline GLuint make_vao_for_program(GLuint vbo, ITER begin, ITER end, GLuint program, char const *program_name_ = nullptr) {
	std::string program_name = "";
	if (program_name_) program_name = " '" + std::string(program_name_) + "'";
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
			std::cerr << "WARNING: attribute '" << name << "' in buffer isn't active in program" << program_name << "." << std::endl;
		} else {
			attrib.VertexAttribPointer(location);
			glEnableVertexAttribArray(location);
			bound.insert(location);
		}
	};
	for (auto i = begin; i != end; ++i) {
		bind_attribute(i->first, i->second);
	}
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
			throw std::runtime_error("ERROR: active attribute '" + std::string(name) + "' in program" + program_name + " is not bound.");
		}
	}

	GL_ERRORS();

	return vao;
}
