#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "TransformAnimation.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <list>

// The 'BridgeMode' mode shows off some transform animations:

struct BridgeMode : public Mode {
	BridgeMode();
	virtual ~BridgeMode();

	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	std::list< TransformAnimationPlayer > current_animations;
};
