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
	int32_t iframe = int32_t(std::floor(frame));

	//hold first/last frame if out of range:
	if (iframe < 0) iframe = 0;
	if (iframe >= animation.frames) iframe = int32_t(animation.frames) - 1;

	//copy data from animation to transforms:
	TransformAnimation::TRS const *frame = &animation.frames_data[animation.names.size() * iframe];
	assert(transforms.size() == animation.names.size());
	for (uint32_t i = 0; i < transforms.size(); ++i) {
		if (transforms[i] != nullptr) {
			transforms[i]->position = frame[i].translation;
			transforms[i]->rotation = frame[i].rotation;
			transforms[i]->scale = frame[i].scale;
		}
	}
}
