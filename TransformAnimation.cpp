#include "TransformAnimation.hpp"

#include "read_chunk.hpp"

#include <iostream>
#include <fstream>

TransformAnimation::TransformAnimation(std::string const &filename) {
	std::ifstream file(filename, std::ios::binary);

	std::vector< char > strings_data;
	read_chunk(file, "str0", &strings_data);

	struct IndexEntry {
		uint32_t begin;
		uint32_t end;
	};

	std::vector< IndexEntry > index_data;
	read_chunk(file, "idx0", &index_data);

	//build names from index:
	names.reserve(index_data.size());
	for (auto const &e : index_data) {
		if (e.begin > e.end || e.end > strings_data.size()) {
			throw std::runtime_error("idx0 chunk in '" + filename + "' contains an invalid entry.");
		}
		names.emplace_back(strings_data.data() + e.begin, strings_data.data() + e.end);
	}

	//frames_data gets read directly into this->frames_data:
	read_chunk(file, "xff0", &frames_data);

	if (frames_data.size() % names.size() != 0) {
		throw std::runtime_error("xff0 chunk in '" + filename + "' contains a partial frame.");
	}

	frames = frames_data.size() / names.size();

	if (frames == 0) {
		throw std::runtime_error("Animation in '" + filename + "' contains zero frames.");
	}
}

TransformAnimationPlayer::TransformAnimationPlayer(TransformAnimation const &animation_, std::vector< Scene::Transform * > const &transforms_, float speed) : animation(animation_), transforms(transforms_) {
	if (transforms.size() != animation.names.size()) {
		std::cerr << "WARNING: TransformAnimationPlayer was passed a list of " << transforms.size() << " transforms for an animation on " << animation.names.size() << " objects. Will trim / pad with null." << std::endl;
		while (transforms.size() > animation.names.size()) {
			transforms.pop_back();
		}
		while (transforms.size() < animation.names.size()) {
			transforms.emplace_back(nullptr);
		}
	}

	frame = 0.0f;
	frames_per_second = speed * 24.0f;
}

void TransformAnimationPlayer::update(float elapsed) {
	//update playback based on frame rate:
	frame += frames_per_second * elapsed;

	//floor to nearest frame:
	int32_t iframe = int32_t(std::floor(frame)); // animation.frames - 1
	int32_t iframe2 = iframe + 1; //animation.frames
	float amt = frame - iframe;

	//hold first/last frame if out of range:
	if (iframe < 0) {
		iframe = 0;
		iframe2 = iframe;
		amt = 0.0f;
	}
	if (iframe2 >= animation.frames) { 
		iframe = int32_t(animation.frames) - 1;
		iframe2 = iframe;
		amt = 0.0f;
	}
	assert(iframe >= 0 && iframe < animation.frames);
	assert(iframe2 >= 0 && iframe2 < animation.frames);

	//copy data from animation to transforms:
	TransformAnimation::TRS const *frame = animation.frames_data.data() + (animation.names.size() * iframe);
	TransformAnimation::TRS const *frame2 = animation.frames_data.data() + (animation.names.size() * iframe2);
	assert(transforms.size() == animation.names.size());
	for (uint32_t i = 0; i < transforms.size(); ++i) {
		if (transforms[i] != nullptr) {
			transforms[i]->position = glm::mix(frame[i].translation, frame2[i].translation, amt);
			transforms[i]->rotation = glm::slerp(frame[i].rotation, frame2[i].rotation, amt);
			transforms[i]->scale = glm::mix(frame[i].scale, frame2[i].scale, amt);
		}
	}
}
