#pragma once

/*
 * TransformAnimation wraps Translation-Rotation-Scale animations exported from blender.
 * You attach a TransformAnimation to your hierarchy using a TransformAnimationPlayer (which you call each frame
 * to update the Transforms).
 *
 */

#include "Scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

//contains transform animation data:
struct TransformAnimation {
	TransformAnimation(std::string const &filename); //load from a file; throws on error.
	std::vector< std::string > names;
	struct TRS {
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;
	};
	uint32_t frames;
	std::vector< TRS > frames_data; //raw frame data (names * frames) elements
};

//copies frame data from TransformAnimation to scene transforms:
struct TransformAnimationPlayer {
	TransformAnimationPlayer(TransformAnimation const &animation, std::vector< Scene::Transform * > const &transforms, float speed = 1.0f);

	TransformAnimation const &animation;
	std::vector< Scene::Transform * > transforms;

	float frame = 0.0f;
	float frames_per_second = 24.0f;

	//advance playback by 'elapsed' seconds, set transforms:
	void update(float elapsed);

	bool done() const { return frame >= animation.frames; }

};
