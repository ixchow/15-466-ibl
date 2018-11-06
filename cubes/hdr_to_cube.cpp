#include "load_hdr.hpp"
#include "load_save_png.hpp"

#include <iostream>

int main(int argc, char **argv) {
	if (argc != 3) {
		std::cerr << "Usage:\n\t./hdr_to_cube <latlon.hdr> <cube.png>" << std::endl;
		return 1;
	}
	std::string hdr_file = argv[1];
	std::string png_file = argv[2];

	glm::uvec2 size;
	std::vector< glm::u8vec4 > data_rgbe;
	load_hdr(hdr_file, &size, &data_rgbe);

	std::cout << "Loaded a " << size.x << " x " << size.y << " hdr image from '" << hdr_file << "'" << std::endl;

	//convert rgbe data to linear floating point data:
	std::vector< glm::vec3 > data;
	data.reserve(data_rgbe.size());
	for (auto const &pix : data_rgbe) {
		int exp = int(pix.a) - 128;
		data.emplace_back(
			std::ldexp(pix.r / 255.999f, exp),
			std::ldexp(pix.g / 255.999f, exp),
			std::ldexp(pix.b / 255.999f, exp)
		);
	}

	{ //dump tone-mapped image:
		std::vector< glm::u8vec4 > mapped;
		mapped.reserve(data.size());
		for (auto pix : data) {
			//luminance (in lumens per steradian per square meter) of pixel:
			// [according to picture file format docs]
			//float lum = 179 * 0.265f * pix.r + 0.670f * pix.g + 0.065f * pix.b;

			//pix = pix * (10.0f * std::log(lum + 1.0f) / lum);
			//pix = pix * (10.0f * std::pow(lum, 0.5f) / lum);
			pix.r = std::pow(pix.r, 0.45f);
			pix.g = std::pow(pix.g, 0.45f);
			pix.b = std::pow(pix.b, 0.45f);

			glm::ivec3 amt = glm::ivec3(255.0f * pix);
			mapped.emplace_back(
				std::min(255, std::max(0, amt.r)),
				std::min(255, std::max(0, amt.g)),
				std::min(255, std::max(0, amt.b)),
				0xff
			);
		}

		save_png("temp.png", size, mapped.data(), LowerLeftOrigin);
	}

}
