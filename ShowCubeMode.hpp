#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "Scene.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <list>

// The 'ShowCubeMode' loads and shows a HDR cube-map:

struct ShowCubeMode : public Mode {
	ShowCubeMode();
	virtual ~ShowCubeMode();

	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//controls:
	bool mouse_captured = false;
	bool paused = true;

	//scene:
	Scene scene;
	Scene::Camera *camera = nullptr;
	Scene::Object *cube = nullptr;
	Scene::Object *rocket = nullptr;

	glm::vec3 camera_center = glm::vec3(0.0f);
	float camera_radius = 10.0f;
	float camera_azimuth = glm::radians(60.0f);
	float camera_elevation = glm::radians(45.0f);
};
