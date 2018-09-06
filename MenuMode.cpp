#include "MenuMode.hpp"

#include "Load.hpp"
#include "compile_program.hpp"
#include "MeshBuffer.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>

//---------- resources ------------
Load< MeshBuffer > menu_meshes(LoadTagInit, [](){
	return new MeshBuffer(data_path("menu.p"));
});


//Uniform locations in menu_program:
GLint menu_program_mvp = -1;
GLint menu_program_color = -1;

Load< GLuint > menu_program(LoadTagInit, [](){
	GLuint *ret = new GLuint(compile_program(
		"#version 330\n"
		"uniform mat4 mvp;\n"
		"in vec4 Position;\n"
		"void main() {\n"
		"	gl_Position = mvp * Position;\n"
		"}\n"
	,
		"#version 330\n"
		"uniform vec3 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = vec4(color, 1.0);\n"
		"}\n"
	));

	menu_program_mvp = glGetUniformLocation(*ret, "mvp");
	menu_program_color = glGetUniformLocation(*ret, "color");

	return ret;
});

//Binding for using menu_program on menu_meshes:
Load< GLuint > menu_binding(LoadTagDefault, [](){
	return new GLuint(menu_meshes->make_vao_for_program(*menu_program));
});

GLint fade_program_color = -1;

Load< GLuint > fade_program(LoadTagInit, [](){
	GLuint *ret = new GLuint(compile_program(
		"#version 330\n"
		"void main() {\n"
		"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
		"}\n"
	,
		"#version 330\n"
		"uniform vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = color;\n"
		"}\n"
	));

	fade_program_color = glGetUniformLocation(*ret, "color");

	return ret;
});


//----------------------

bool MenuMode::handle_event(SDL_Event const &e, glm::uvec2 const &window_size) {
	if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_ESCAPE) {
			Mode::set_current(nullptr);
			return true;
		} else if (e.key.keysym.sym == SDLK_UP) {
			//find previous selectable thing that isn't selected:
			uint32_t old = selected;
			selected -= 1;
			while (selected < choices.size() && !choices[selected].on_select) --selected;
			if (selected >= choices.size()) selected = old;

			return true;
		} else if (e.key.keysym.sym == SDLK_DOWN) {
			//find next selectable thing that isn't selected:
			uint32_t old = selected;
			selected += 1;
			while (selected < choices.size() && !choices[selected].on_select) ++selected;
			if (selected >= choices.size()) selected = old;

			return true;
		} else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
			if (selected < choices.size() && choices[selected].on_select) {
				choices[selected].on_select();
			}
			return true;
		}
	}
	return false;
}

void MenuMode::update(float elapsed) {
	bounce += elapsed / 0.7f;
	bounce -= std::floor(bounce);

	if (background) {
		background->update(elapsed * background_time_scale);
	}
}

void MenuMode::draw(glm::uvec2 const &drawable_size) {
	if (background && background_fade < 1.0f) {
		background->draw(drawable_size);

		glDisable(GL_DEPTH_TEST);
		if (background_fade > 0.0f) {
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glUseProgram(*fade_program);
			glUniform4fv(fade_program_color, 1, glm::value_ptr(glm::vec4(0.0f, 0.0f, 0.0f, background_fade)));
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glUseProgram(0);
			glDisable(GL_BLEND);
		}
	}
	glDisable(GL_DEPTH_TEST);

	float aspect = drawable_size.x / float(drawable_size.y);
	//scale factors such that a rectangle of aspect 'aspect' and height '1.0' fills the window:
	glm::vec2 scale = glm::vec2(1.0f / aspect, 1.0f);
	glm::mat4 projection = glm::mat4(
		glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
	);

	float total_height = 0.0f;
	for (auto const &choice : choices) {
		total_height += choice.height + 2.0f * choice.padding;
	}

	glUseProgram(*menu_program);
	glBindVertexArray(*menu_binding);

	//character width and spacing helpers:
	// (...in terms of the menu font's default 3-unit height)
	auto width = [](char a) {
		if (a == 'I') return 1.0f;
		else if (a == 'L') return 2.0f;
		else if (a == 'M' || a == 'W') return 4.0f;
		else return 3.0f;
	};
	auto spacing = [](char a, char b) {
		return 1.0f;
	};

	float select_bounce = std::abs(std::sin(bounce * 3.1515926f * 2.0f));

	float y = 0.5f * total_height;
	for (auto const &choice : choices) {
		y -= choice.padding;
		y -= choice.height;

		bool is_selected = (&choice - &choices[0] == selected);
		std::string label = choice.label;

		if (is_selected) {
			label = "*" + label + "*";
		}

		float total_width = 0.0f;
		for (uint32_t i = 0; i < label.size(); ++i) {
			if (i > 0) total_width += spacing(label[i-1], label[i]);
			total_width += width(label[i]);
		}
		if (is_selected) {
			total_width += 2.0f * select_bounce;
		}

		float x = -0.5f * total_width;
		for (uint32_t i = 0; i < label.size(); ++i) {
			if (i > 0) x += spacing(label[i-1], label[i]);
			if (is_selected && (i == 1 || i + 1 == label.size())) {
				x += select_bounce;
			}

			if (label[i] != ' ') {
				float s = choice.height * (1.0f / 3.0f);
				glm::mat4 mvp = projection * glm::mat4(
					glm::vec4(s, 0.0f, 0.0f, 0.0f),
					glm::vec4(0.0f, s, 0.0f, 0.0f),
					glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
					glm::vec4(s * x, y, 0.0f, 1.0f)
				);
				glUniformMatrix4fv(menu_program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
				glUniform3f(menu_program_color, 1.0f, 1.0f, 1.0f);

				MeshBuffer::Mesh const &mesh = menu_meshes->lookup(label.substr(i,1));
				glDrawArrays(GL_TRIANGLES, mesh.start, mesh.count);
			}

			x += width(label[i]);
		}

		y -= choice.padding;
	}

	glEnable(GL_DEPTH_TEST);
}
