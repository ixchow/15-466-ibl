#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

// The 'GameMode' mode is the main gameplay mode:

struct GameMode : public Mode {
	GameMode();
	virtual ~GameMode();

	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

	//update is called at the start of a new frame, after events are handled:
	virtual void update(float elapsed) override;

	//draw is called after update:
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//starts up a 'quit/resume' pause menu:
	void show_pause_menu();

	//------- game state -------

	glm::uvec2 board_size = glm::uvec2(5,4);
	std::vector< MeshBuffer::Mesh const * > board_meshes;
	std::vector< glm::quat > board_rotations;

	glm::uvec2 cursor = glm::vec2(0,0);

	struct {
		bool roll_left = false;
		bool roll_right = false;
		bool roll_up = false;
		bool roll_down = false;
	} controls;

};
