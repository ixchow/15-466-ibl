#include "BridgeMode.hpp"

#include "MenuMode.hpp"
#include "TransformAnimation.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "check_fb.hpp" //helper for checking currently bound OpenGL framebuffer
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "load_save_png.hpp"
#include "vertex_color_program.hpp"
#include "depth_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <unordered_map>


Load< MeshBuffer > bridge_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("bridge.pnc"));
});

Load< GLuint > bridge_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(bridge_meshes->make_vao_for_program(vertex_color_program->program));
});

Load< TransformAnimation > bridge_deploy_tanim(LoadTagDefault, [](){
	return new TransformAnimation(data_path("bridge-deploy.manim"));
});

std::vector< Scene::Transform * > bridge_deploy_transforms;

static Scene::Camera *camera = nullptr;

Load< Scene > bridge_scene(LoadTagDefault, [](){
	Scene *ret = new Scene;

	//pre-build some program info (material) blocks to assign to each object:
	Scene::Object::ProgramInfo vertex_color_program_info;
	vertex_color_program_info.program = vertex_color_program->program;
	vertex_color_program_info.vao = *bridge_meshes_for_vertex_color_program;
	vertex_color_program_info.mvp_mat4  = vertex_color_program->object_to_clip_mat4;
	vertex_color_program_info.mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
	vertex_color_program_info.itmv_mat3 = vertex_color_program->normal_to_light_mat3;

	//load transform hierarchy:
	ret->load(data_path("bridge.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m){
		Scene::Object *obj = s.new_object(t);

		obj->programs[Scene::Object::ProgramTypeDefault] = vertex_color_program_info;

		MeshBuffer::Mesh const &mesh = bridge_meshes->lookup(m);
		obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
		obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;
	});

	//look up various transforms:
	std::unordered_map< std::string, Scene::Transform * > name_to_transform;
	for (Scene::Transform *t = ret->first_transform; t != nullptr; t = t->alloc_next) {
		auto ret = name_to_transform.insert(std::make_pair(t->name, t));
		if (!ret.second) {
			std::cerr << "WARNING: multiple transforms with the name '" << t->name << "' in scene." << std::endl;
		}
	}

	for (auto const &name : bridge_deploy_tanim->names) {
		auto f = name_to_transform.find(name);
		if (f == name_to_transform.end()) {
			std::cerr << "WARNING: transform '" << name << "' appears in animation but not in scene." << std::endl;
			bridge_deploy_transforms.emplace_back(nullptr);
		} else {
			bridge_deploy_transforms.emplace_back(f->second);
		}
	}
	assert(bridge_deploy_transforms.size() == bridge_deploy_tanim->names.size());

	//look up the camera:
	for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
		if (c->transform->name == "Camera") {
			if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
			camera = c;
		}
	}
	if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

	return ret;
});

BridgeMode::BridgeMode() {
}

BridgeMode::~BridgeMode() {
}

bool BridgeMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
		if (current_animations.empty()) {
			current_animations.emplace_back(*bridge_deploy_tanim, bridge_deploy_transforms);
		}
		return true;
	}

	if (evt.type == SDL_MOUSEMOTION) {
	}

	return false;
}

void BridgeMode::update(float elapsed) {
	for (auto ca = current_animations.begin(); ca != current_animations.end(); /* later */ ) {
		ca->update(elapsed);
		if (ca->done()) {
			auto old = ca;
			++ca;
			current_animations.erase(old);
		} else {
			++ca;
		}
	}
}

void BridgeMode::draw(glm::uvec2 const &drawable_size) {
	//Draw scene:
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light positions:
	glUseProgram(vertex_color_program->program);

	//don't use distant directional light at all (color == 0):
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
	//use hemisphere light for sky light:
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.9f, 0.9f, 0.95f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

	bridge_scene->draw(camera);

	GL_ERRORS();
}
