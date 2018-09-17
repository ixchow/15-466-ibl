#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>


Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("paddle-ball.pnc"));
});

Load< GLuint > meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

Scene::Transform *paddle_transform = nullptr;
Scene::Transform *ball_transform = nullptr;

Scene::Camera *camera = nullptr;

Load< Scene > scene(LoadTagDefault, [](){
	Scene *ret = new Scene;
	//load transform hierarchy:
	ret->load(data_path("paddle-ball.scene"), [](Scene &s, Scene::Transform *t, std::string const &m){
		Scene::Object *obj = s.new_object(t);

		obj->program = vertex_color_program->program;
		obj->program_mvp_mat4  = vertex_color_program->object_to_clip_mat4;
		obj->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		obj->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;

		MeshBuffer::Mesh const &mesh = meshes->lookup(m);
		obj->vao = *meshes_for_vertex_color_program;
		obj->start = mesh.start;
		obj->count = mesh.count;
	});

	//look up paddle and ball transforms:
	for (Scene::Transform *t = ret->first_transform; t != nullptr; t = t->alloc_next) {
		if (t->name == "Paddle") {
			if (paddle_transform) throw std::runtime_error("Multiple 'Paddle' transforms in scene.");
			paddle_transform = t;
		}
		if (t->name == "Ball") {
			if (ball_transform) throw std::runtime_error("Multiple 'Ball' transforms in scene.");
			ball_transform = t;
		}
	}
	if (!paddle_transform) throw std::runtime_error("No 'Paddle' transform in scene.");
	if (!ball_transform) throw std::runtime_error("No 'Ball' transform in scene.");

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

GameMode::GameMode(Client &client_) : client(client_) {
	client.connection.send_raw("h", 1); //send a 'hello' to the server
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (evt.type == SDL_MOUSEMOTION) {
		state.paddle.x = (evt.motion.x - 0.5f * window_size.x) / (0.5f * window_size.x) * Game::FrameWidth;
		state.paddle.x = std::max(state.paddle.x, -0.5f * Game::FrameWidth + 0.5f * Game::PaddleWidth);
		state.paddle.x = std::min(state.paddle.x,  0.5f * Game::FrameWidth - 0.5f * Game::PaddleWidth);
	}

	return false;
}

void GameMode::update(float elapsed) {
	state.update(elapsed);

	if (client.connection) {
		//send game state to server:
		client.connection.send_raw("s", 1);
		client.connection.send_raw(&state.paddle.x, sizeof(float));
	}

	client.poll([&](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			//probably won't get this.
		} else if (event == Connection::OnClose) {
			std::cerr << "Lost connection to server." << std::endl;
		} else { assert(event == Connection::OnRecv);
			std::cerr << "Ignoring " << c->recv_buffer.size() << " bytes from server." << std::endl;
			c->recv_buffer.clear();
		}
	});

	//copy game state to scene positions:
	ball_transform->position.x = state.ball.x;
	ball_transform->position.y = state.ball.y;

	paddle_transform->position.x = state.paddle.x;
	paddle_transform->position.y = state.paddle.y;
}

void GameMode::draw(glm::uvec2 const &drawable_size) {
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.25f, 0.0f, 0.5f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light positions:
	glUseProgram(vertex_color_program->program);

	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));

	scene->draw(camera);

	GL_ERRORS();
}
