#pragma once

#include <glm/glm.hpp>
#include <algorithm>

//see radiance source code at ray/src/common/color.c -- setcolr() function

inline glm::vec3 rgbe_to_float(glm::u8vec4 col) {
	//avoid decoding zero to a denormalized value:
	if (col == glm::u8vec4(0,0,0,0)) return glm::vec3(0.0f);

	int exp = int(col.a) - 128;
	return glm::vec3(
		std::ldexp((col.r + 0.5f) / 256.0f, exp),
		std::ldexp((col.g + 0.5f) / 256.0f, exp),
		std::ldexp((col.b + 0.5f) / 256.0f, exp)
	);
}

inline glm::u8vec4 float_to_rgbe(glm::vec3 col) {

	float d = std::max(col.r, std::max(col.g, col.b));

	//1e-32 is from the radiance code, and is probably larger than strictly necessary:
	if (d <= 1e-32f) {
		return glm::u8vec4(0,0,0,0);
	}

	int e;
	float fac = 255.999f * (std::frexp(d, &e) / d);

	//value is too large to represent, clamp to bright white:
	if (e > 127) {
		return glm::u8vec4(0xff, 0xff, 0xff, 0xff);
	}

	//scale and store:
	return glm::u8vec4(
		std::max(0, int32_t(col.r * fac)),
		std::max(0, int32_t(col.g * fac)),
		std::max(0, int32_t(col.b * fac)),
		e + 128
	);
}
