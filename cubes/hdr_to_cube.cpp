#include "load_hdr.hpp"
#include "load_save_png.hpp"
#include "rgbe.hpp"

#include <iostream>

void save_tone_mapped_png(std::string const &filename, glm::uvec2 size, std::vector< glm::vec3 > const &data) {
	std::vector< glm::u8vec4 > mapped;
	mapped.reserve(data.size());
	for (auto pix : data) {
		//luminance (in lumens per steradian per square meter) of pixel:
		// [according to picture file format docs]
		//float lum = 179 * 0.265f * pix.r + 0.670f * pix.g + 0.065f * pix.b;

		//gamma compression:
		constexpr const float Gamma = 0.45f;
		pix.r = std::pow(pix.r, Gamma);
		pix.g = std::pow(pix.g, Gamma);
		pix.b = std::pow(pix.b, Gamma);

		glm::ivec3 amt = glm::ivec3(255.0f * pix);
		mapped.emplace_back(
			std::min(255, std::max(0, amt.r)),
			std::min(255, std::max(0, amt.g)),
			std::min(255, std::max(0, amt.b)),
			0xff
		);
	}
	save_png(filename, size, mapped.data(), LowerLeftOrigin);
}

enum Face {
	PositiveX = 0, NegativeX = 1,
	PositiveY = 2, NegativeY = 3,
	PositiveZ = 4, NegativeZ = 5,
};

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
	std::cout << "Converting to linear floating point..."; std::cout.flush();
	std::vector< glm::vec3 > data;
	data.reserve(data_rgbe.size());
	for (auto const &pix : data_rgbe) {
		data.emplace_back( rgbe_to_float(pix) );
	}
	std::cout << " done." << std::endl;

	//check the conversion by dumping tone-mapped values:
	std::cout << "Writing tone-mapped png [DEBUG-latlon.png]..."; std::cout.flush();
	save_tone_mapped_png("DEBUG-latlon.png", size, data);
	std::cout << " done." << std::endl;

	//function for sampling a given direction from latlon map:
	auto lookup = [&data,&size](glm::vec3 const &dir) -> glm::vec3 {
		float lon = std::atan2(dir.y, dir.x);
		float lat = std::atan2(dir.z, glm::length(glm::vec2(dir)));
		float s = ((lon / M_PI) + 1.0f) / 2.0f;
		float t = ((lat / (0.5f * M_PI)) + 1.0f) / 2.0f;
		//clamp (shouldn't be needed?):
		s = std::max(0.0f, std::min(1.0f, s));
		t = std::max(0.0f, std::min(1.0f, t));

		//return value from nearest pixel center:
		glm::ivec2 px = glm::ivec2(std::floor(size.x*s), std::floor(size.y*t));
		//clamp (may be needed if sampling exactly the right or left edge):
		px.x = std::max(0, std::min(int32_t(size.x)-1, px.x));
		px.y = std::max(0, std::min(int32_t(size.y)-1, px.y));

		return data[px.y*size.x+px.x];
	};

	//accumulate color into cubemap pixels:
	uint32_t cube_size = 512U;
	std::vector< glm::vec3 > faces[6]; //+x, -x, +y, -y, +z, -z

	//will sample multiple times per direction:
	std::vector< glm::vec3 > samples;
	{ //even sampling over texel area:
		constexpr uint32_t Count = 6; //6x6 = 36 samples
		for (uint32_t t = 0; t < Count; ++t) {
			for (uint32_t s = 0; s < Count; ++s) {
				samples.emplace_back(
					(s + 0.5f) / float(Count),
					(t + 0.5f) / float(Count),
					1.0f / float(Count * Count)
				);
			}
		}
	}
	std::cout << "Using " << samples.size() << " samples per texel." << std::endl;

	for (uint32_t f = 0; f < 6; ++f) {
		std::cout << "Sampling face " << f << "/6 ..."; std::cout.flush();
		glm::vec3 sc; //maps to rightward axis on face
		glm::vec3 tc; //maps to upward axis on face
		glm::vec3 ma; //direction to face
		//See OpenGL 4.4 Core Profile specification, Table 8.18:
		if      (f == PositiveX) { sc = glm::vec3( 0.0f, 0.0f,-1.0f); tc = glm::vec3( 0.0f,-1.0f, 0.0f); ma = glm::vec3( 1.0f, 0.0f, 0.0f); }
		else if (f == NegativeX) { sc = glm::vec3( 0.0f, 0.0f, 1.0f); tc = glm::vec3( 0.0f,-1.0f, 0.0f); ma = glm::vec3(-1.0f, 0.0f, 0.0f); }
		else if (f == PositiveY) { sc = glm::vec3( 1.0f, 0.0f, 0.0f); tc = glm::vec3( 0.0f, 0.0f, 1.0f); ma = glm::vec3( 0.0f, 1.0f, 0.0f); }
		else if (f == NegativeY) { sc = glm::vec3( 1.0f, 0.0f, 0.0f); tc = glm::vec3( 0.0f, 0.0f,-1.0f); ma = glm::vec3( 0.0f,-1.0f, 0.0f); }
		else if (f == PositiveZ) { sc = glm::vec3( 1.0f, 0.0f, 0.0f); tc = glm::vec3( 0.0f,-1.0f, 0.0f); ma = glm::vec3( 0.0f, 0.0f, 1.0f); }
		else if (f == NegativeZ) { sc = glm::vec3(-1.0f, 0.0f, 0.0f); tc = glm::vec3( 0.0f,-1.0f, 0.0f); ma = glm::vec3( 0.0f, 0.0f,-1.0f); }
		else assert(0 && "Invalid face.");
	
		std::vector< glm::vec3 > &face = faces[f];
		face.reserve(cube_size * cube_size);
		for (uint32_t t = 0; t < cube_size; ++t) {
			for (uint32_t s = 0; s < cube_size; ++s) {
				glm::vec3 acc = glm::vec3(0.0f);
				for (auto const &sample : samples) {
					glm::vec3 dir = ma
					              + (2.0f * (s + 0.5f + sample.x) / cube_size - 1.0f) * sc
					              + (2.0f * (t + 0.5f + sample.y) / cube_size - 1.0f) * tc;
					acc += sample.z * lookup(dir);
				}
				face.emplace_back(acc);
			}
		}
		std::cout << " done." << std::endl;
	}

	//write faces out to separate files:
	std::cout << "Writing tone-mapped pngs [DEBUG-(pos|neg)[XYZ].png]..."; std::cout.flush();
	save_tone_mapped_png("DEBUG-posX.png", glm::uvec2(cube_size, cube_size), faces[PositiveX]);
	save_tone_mapped_png("DEBUG-negX.png", glm::uvec2(cube_size, cube_size), faces[NegativeX]);
	save_tone_mapped_png("DEBUG-posY.png", glm::uvec2(cube_size, cube_size), faces[PositiveY]);
	save_tone_mapped_png("DEBUG-negY.png", glm::uvec2(cube_size, cube_size), faces[NegativeY]);
	save_tone_mapped_png("DEBUG-posZ.png", glm::uvec2(cube_size, cube_size), faces[PositiveZ]);
	save_tone_mapped_png("DEBUG-negZ.png", glm::uvec2(cube_size, cube_size), faces[NegativeZ]);
	std::cout << " done." << std::endl;

	//write a single +x/-x/+y/-y/+z/-z (all stacked in a column with +x at the bottom) file for storage:
	std::cout << "Writing final rgbe png..."; std::cout.flush();
	std::vector< glm::u8vec4 > cube_data;
	cube_data.reserve(6 * cube_size * cube_size);
	uint32_t overflow = 0;
	for (auto const &face : faces) {
		for (auto const &pix : face) {
			cube_data.emplace_back( float_to_rgbe( pix ) );
			if (cube_data.back() == glm::u8vec4(0xff, 0xff, 0xff, 0xff)) ++overflow;
		}
	}
	std::cout << "Output contains " << overflow << " bright-white (likely overflow) pixels." << std::endl;
	save_png(png_file, glm::uvec2(cube_size, 6 * cube_size), cube_data.data(), LowerLeftOrigin);
}
