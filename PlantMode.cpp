#include "PlantMode.hpp"
#include "MenuMode.hpp"

#include "MenuMode.hpp"
#include "BoneAnimation.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "check_fb.hpp" //helper for checking currently bound OpenGL framebuffer
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "load_save_png.hpp"
#include "vertex_color_program.hpp"
#include "bone_vertex_color_program.hpp"
#include "depth_program.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <unordered_map>

extern std::shared_ptr< MenuMode > menu;

MeshBuffer::Mesh const *plant_tile = nullptr;

Load< MeshBuffer > plant_meshes(LoadTagDefault, [](){
	auto ret = new MeshBuffer(data_path("plant.pnc"));
	plant_tile = &ret->lookup("Tile");
	return ret;
});

Load< GLuint > plant_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(plant_meshes->make_vao_for_program(vertex_color_program->program));
});

BoneAnimation::Animation const *plant_banim_wind = nullptr;
BoneAnimation::Animation const *plant_banim_walk = nullptr;

Load< BoneAnimation > plant_banims(LoadTagDefault, [](){
	auto ret = new BoneAnimation(data_path("plant.banims"));
	plant_banim_wind = &(ret->lookup("Wind"));
	plant_banim_walk = &(ret->lookup("Walk"));
	return ret;
});

Load< GLuint > plant_banims_for_bone_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(plant_banims->make_vao_for_program(bone_vertex_color_program->program));
});

PlantMode::PlantMode() {
	//Make a scene from scratch using the plant prop and the tile mesh:
	{ //make a tile floor:
		Scene::Object::ProgramInfo tile_info;
		tile_info.program = vertex_color_program->program;
		tile_info.vao = *plant_meshes_for_vertex_color_program;
		tile_info.start = plant_tile->start;
		tile_info.count = plant_tile->count;
		tile_info.mvp_mat4 = vertex_color_program->object_to_clip_mat4;
		tile_info.mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		tile_info.itmv_mat3 = vertex_color_program->normal_to_light_mat3;

		for (int32_t x = -5; x <= 5; ++x) {
			for (int32_t y = -5; y <= 5; ++y) {
				Scene::Transform *transform = scene.new_transform();
				transform->name = "Tile-" + std::to_string(x) + "," + std::to_string(y); //<-- no reason to do this, we don't have scene debugger or anything
				transform->position = glm::vec3(2.0f*x, 2.0f*y, 0.0f);
				Scene::Object *tile = scene.new_object(transform);
				tile->programs[Scene::Object::ProgramTypeDefault] = tile_info;
			}
		}
	}

	{ //put some plants around the edge:
		Scene::Object::ProgramInfo plant_info;
		plant_info.program = bone_vertex_color_program->program;
		plant_info.vao = *plant_banims_for_bone_vertex_color_program;
		plant_info.start = plant_banims->mesh.start;
		plant_info.count = plant_banims->mesh.count;
		plant_info.mvp_mat4 = bone_vertex_color_program->object_to_clip_mat4;
		plant_info.mv_mat4x3 = bone_vertex_color_program->object_to_light_mat4x3;
		plant_info.itmv_mat3 = bone_vertex_color_program->normal_to_light_mat3;

		plant_animations.reserve(5);
		for (int32_t x = -2; x <= 2; ++x) {
			if (x != 0) {
				plant_animations.emplace_back(*plant_banims, *plant_banim_wind, BoneAnimationPlayer::Once);
				plant_animations.back().position = 1.0f;
			} else {
				plant_animations.emplace_back(*plant_banims, *plant_banim_walk, BoneAnimationPlayer::Loop, 0.0f);
			}

			BoneAnimationPlayer *player = &plant_animations.back();
		
			plant_info.set_uniforms = [player](){
				player->set_uniform(bone_vertex_color_program->bones_mat4x3_array);
			};

			Scene::Transform *transform = scene.new_transform();
			transform->position.x = x * 2.5f;
			Scene::Object *plant = scene.new_object(transform);
			plant->programs[Scene::Object::ProgramTypeDefault] = plant_info;

			if (x == 0) this->plant = plant;
		};
		assert(plant_animations.size() == 5);
	}

	{ //make a camera:
		Scene::Transform *transform = scene.new_transform();
		transform->position = glm::vec3(0.0f, -10.0f, 3.0f);
		transform->rotation = glm::quat_cast(glm::mat3(glm::lookAt(
			transform->position,
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		)));
		camera = scene.new_camera(transform);
		camera->near = 0.01f;
		camera->fovy = glm::radians(45.0f);
	}

}

PlantMode::~PlantMode() {
}

bool PlantMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
		menu->background = shared_from_this();
		Mode::set_current(menu);
		return true;
	}

	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_UP) {
		forward = true;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_UP) {
		forward = false;
		return true;
	}
	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
		backward = true;
		return true;
	}
		if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
		backward = false;
		return true;
	}

	if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (!mouse_captured) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_captured = true;
		}
	}
	if (evt.type == SDL_MOUSEBUTTONUP) {
		if (mouse_captured) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			mouse_captured = false;
		}
	}
	if (evt.type == SDL_MOUSEMOTION) {
		if (mouse_captured) {
			float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
			float pitch = -evt.motion.yrel / float(window_size.y) * camera->fovy;

			//update camera angles:
			camera_elevation = glm::clamp(camera_elevation + pitch, glm::radians(10.0f), glm::radians(80.0f));
			camera_azimuth = camera_azimuth + yaw;
		}
	}

	return false;
}

void PlantMode::update(float elapsed) {

	{
		float step = 0.0f;
		if (forward) step += elapsed * 4.0f;
		if (backward) step -= elapsed * 4.0f;
		plant->transform->position.y += step;
		plant_animations[2].position -= step / 1.88803f;
		plant_animations[2].position -= std::floor(plant_animations[2].position);
	}

	float ce = std::cos(camera_elevation);
	float se = std::sin(camera_elevation);
	float ca = std::cos(camera_azimuth);
	float sa = std::sin(camera_azimuth);
	camera->transform->position = camera_radius * glm::vec3(ce * ca, ce * sa, se) + plant->transform->position;
	camera->transform->rotation =
		glm::quat_cast(glm::transpose(glm::mat3(glm::lookAt(
			camera->transform->position,
			plant->transform->position,
			glm::vec3(0.0f, 0.0f, 1.0f)
		))));
	
	static std::mt19937 mt(0xfeedf00d);
	wind_acc -= elapsed;
	if (wind_acc < 0.0f) {
		wind_acc = mt() / float(mt.max()) * 2.0f;
		uint32_t idx = mt() % plant_animations.size();
		if (idx != 2) {
			if (plant_animations[idx].done()) {
				plant_animations[idx].position = 0.0f;
			}
		}
	}

	for (auto &anim : plant_animations) {
		anim.update(elapsed);
	}
}

void PlantMode::draw(glm::uvec2 const &drawable_size) {
	//Draw scene:
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//set up light positions:
	glUseProgram(vertex_color_program->program);

	//don't use distant directional light at all (color == 0):
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
	//use hemisphere light for sky light:
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.9f, 0.9f, 0.95f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

	//set up light positions (bone program):
	glUseProgram(bone_vertex_color_program->program);

	//don't use distant directional light at all (color == 0):
	glUniform3fv(bone_vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(bone_vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
	//use hemisphere light for sky light:
	glUniform3fv(bone_vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.9f, 0.9f, 0.95f)));
	glUniform3fv(bone_vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

	scene.draw(camera);

	GL_ERRORS();
}
