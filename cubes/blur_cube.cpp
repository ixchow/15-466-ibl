#include "load_save_png.hpp"
#include "rgbe.hpp"

#include <iostream>
#include <functional>
#include <random>
#include <algorithm>

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
	if (argc != 6 && argc != 7) {
		std::cerr << "Usage:\n\t./blur_cube <in.png> <diffuse|bokeh|...> <samples> <out size> <out.png> [brightest]\nBlur a cubemap (stored as an rgbe png, with faces stacked +x/-x/+y/-y/+z/-z top-to-bottom) into another cubemap (stored into the same format)" << std::endl;
		return 1;
	}
	std::string in_file = argv[1];
	std::string mode = argv[2];
	int32_t samples = std::atoi(argv[3]);
	glm::ivec2 out_size;
	out_size.x = std::atoi(argv[4]);
	out_size.y = out_size.x * 6;
	std::string out_file = argv[5];
	int32_t brightest = 10000;
	if (argc == 7) brightest = std::atoi(argv[6]);

	struct BrightDirection {
		glm::vec3 direction = glm::vec3(0.0f);
		glm::vec3 light = glm::vec3(0.0f); //already multiplied by solid angle, I guess?
	};
	std::vector< BrightDirection > bright_directions;

	std::function< glm::vec3() > make_sample; //returns an upper hemisphere direction
	std::function< glm::vec3(glm::vec3) > sum_bright_directions; //run lighting for bright directions, given normal

	if (mode == "diffuse") {
		make_sample = []() -> glm::vec3 {
			//attempt to importance sample upper hemisphere (cos-weighted):
			//based on: http://www.rorydriscoll.com/2009/01/07/better-sampling/
			static std::mt19937 mt(0x12341234);
			glm::vec2 rv(mt() / float(mt.max()), mt() / float(mt.max()));
			float phi = rv.x * 2.0f * M_PI;
			float r = std::sqrt(rv.y);
			return glm::vec3(
				std::cos(phi) * r,
				std::sin(phi) * r,
				std::sqrt(1.0f - rv.y)
			);
		};
		sum_bright_directions = [&bright_directions](glm::vec3 n) -> glm::vec3 {
			glm::vec3 ret = glm::vec3(0.0f);
			for (auto const &bd : bright_directions) {
				ret += std::max(0.0f, glm::dot(bd.direction, n)) * bd.light;
			}
			return ret;
		};
	} else if (mode == "bokeh") {
		constexpr float angle = 0.7f / 180.0f * float(M_PI);
		float max_r = std::sin(angle);
		float min_y = std::sqrt(1.0f - max_r * max_r);
		make_sample = [min_y]() -> glm::vec3 {
			//try to uniformly sample a disc...
			static std::mt19937 mt(0x12341234);
			glm::vec2 rv(mt() / float(mt.max()), mt() / float(mt.max()));
			float phi = rv.x * 2.0f * M_PI;
			//float r = max_r * std::sqrt(rv.y);
			rv.y = min_y + (1.0f - min_y) * rv.y;
			float r = std::sqrt(1.0f - rv.y * rv.y);
			return glm::vec3(
				std::cos(phi) * r,
				std::sin(phi) * r,
				rv.y
			);
		};
		float thresh = std::cos(angle); //hmmmmmmm
		sum_bright_directions = [&bright_directions,thresh](glm::vec3 n) -> glm::vec3 {
			glm::vec3 ret = glm::vec3(0.0f);
			for (auto const &bd : bright_directions) {
			 	if (glm::dot(bd.direction, n) > thresh) {
					ret += bd.light;
				}
			}
			return ret;
		};
	} else if (mode == "sharp") {
		make_sample = []() -> glm::vec3 {
			return glm::vec3(0.0f, 0.0f, 1.0f);
		};
	} else {
		std::cerr << "Blur must be 'diffuse', 'bokeh', or 'sharp'." << std::endl;
		return 1;
	}
	if (out_size.x < 1) {
		std::cerr << "Output cube map size must be at least 1." << std::endl;
		return 1;
	}
	if (samples < 1) {
		std::cerr << "Samples per pixel must be at least 1." << std::endl;
		return 1;
	}

	glm::uvec2 in_size;
	std::vector< glm::u8vec4 > in_data_rgbe;
	load_png(in_file, &in_size, &in_data_rgbe, LowerLeftOrigin);

	std::cout << "Loaded a " << in_size.x << " x " << in_size.y << " rgbe image from '" << in_file << "'" << std::endl;

	if (in_size.x * 6 != in_size.y) {
		std::cerr << "Expecting a 1x6 image." << std::endl;
		return 1;
	}

	//convert rgbe data to linear floating point data:
	std::cout << "Converting to linear floating point..."; std::cout.flush();
	std::vector< glm::vec3 > in_data;
	in_data.reserve(in_data_rgbe.size());
	for (auto const &pix : in_data_rgbe) {
		in_data.emplace_back( rgbe_to_float(pix) );
	}
	std::cout << " done." << std::endl;

	if (sum_bright_directions) {
		uint32_t bright = std::min< uint32_t >(in_data.size(), brightest);
		std::cout << "Separating the brightest " << bright << " pixels..."; std::cout.flush();
		std::vector< std::pair< float, uint32_t > > pixels;
		pixels.reserve(in_data.size());
		for (auto const &px : in_data) {
			pixels.emplace_back(std::max(px.r, std::max(px.g, px.b)), pixels.size());
		}
		std::stable_sort(pixels.begin(), pixels.end());
		for (uint32_t b = 0; b < bright; ++b) {
			uint32_t i = pixels[pixels.size()-1-b].second;
			uint32_t s = i % in_size.x;
			uint32_t t = (i / in_size.x) % in_size.x;
			uint32_t f = i / (in_size.x * in_size.x);

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

			glm::vec3 dir = glm::normalize(ma
			              + (2.0f * (s + 0.5f) / in_size.x - 1.0f) * sc
			              + (2.0f * (t + 0.5f) / in_size.x - 1.0f) * tc);

			bright_directions.emplace_back();
			bright_directions.back().direction = dir;
			float solid_angle = 4.0f * M_PI / float(6.0f * in_size.x * in_size.x); // approximate, since pixels on cube actually take up different amounts depending on position
			bright_directions.back().light = in_data[i] * solid_angle;

			in_data[i] = glm::vec3(0.0f, 0.0f, 0.0f); //remove from input data
		}
		std::cout << " done." << std::endl;
	}

	{ //DEBUG: tone map and save again:
		std::cout << "Writing tone-mapped png [DEBUG-blur-in.png]..."; std::cout.flush();
		save_tone_mapped_png("DEBUG-blur-in.png", in_size, in_data);
		std::cout << " done." << std::endl;
	}


/*
	//check the conversion by dumping tone-mapped values:
	std::cout << "Writing tone-mapped png [DEBUG-latlon.png]..."; std::cout.flush();
	save_tone_mapped_png("DEBUG-latlon.png", size, data);
	std::cout << " done." << std::endl;
*/

	//function for sampling a given direction from cubemap:
	auto lookup = [&in_data,&in_size](glm::vec3 const &dir) -> glm::vec3 {
		float sc, tc, ma;
		uint32_t f;
		if (std::abs(dir.x) >= std::abs(dir.y) && std::abs(dir.x) >= std::abs(dir.z)) {
			if (dir.x >= 0) { sc = -dir.z; tc = -dir.y; ma = dir.x; f = PositiveX; }
			else            { sc =  dir.z; tc = -dir.y; ma =-dir.x; f = NegativeX; }
		} else if (std::abs(dir.y) >= std::abs(dir.z)) {
			if (dir.y >= 0) { sc =  dir.x; tc =  dir.z; ma = dir.y; f = PositiveY; }
			else            { sc =  dir.x; tc = -dir.z; ma =-dir.y; f = NegativeY; }
		} else {
			if (dir.z >= 0) { sc =  dir.x; tc = -dir.y; ma = dir.z; f = PositiveZ; }
			else            { sc = -dir.x; tc = -dir.y; ma =-dir.z; f = NegativeZ; }
		}

		int32_t s = std::floor(0.5f * (sc / ma + 1.0f) * in_size.x);
		s = std::max(0, std::min(int32_t(in_size.x)-1, s));
		int32_t t = std::floor(0.5f * (tc / ma + 1.0f) * in_size.x);
		t = std::max(0, std::min(int32_t(in_size.x)-1, t));

		return in_data[(f*in_size.x+t)*in_size.x+s];
	};

	std::vector< glm::vec3 > out_data;
	out_data.reserve(out_size.x * out_size.y);

	std::cout << "Using " << samples << " samples per texel." << std::endl;

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
	
		for (uint32_t t = 0; t < uint32_t(out_size.x); ++t) {
			for (uint32_t s = 0; s < uint32_t(out_size.x); ++s) {
				glm::vec3 N = glm::normalize(ma
				            + (2.0f * (s + 0.5f) / out_size.x - 1.0f) * sc
				            + (2.0f * (t + 0.5f) / out_size.x - 1.0f) * tc);
				glm::vec3 temp = (abs(N.z) < 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f));
				glm::vec3 TX = glm::normalize(glm::cross(N, temp));
				glm::vec3 TY = glm::cross(N, TX);

				glm::vec3 acc = glm::vec3(0.0f);
				for (uint32_t i = 0; i < uint32_t(samples); ++i) {
					//very inspired by the SampleGGX code in "Real Shading in Unreal" (https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf):
			

					glm::vec3 dir = make_sample();

					acc += lookup( dir.x * TX + dir.y * TY + dir.z * N );
					//acc += (dir.x * TX + dir.y * TY + dir.z * N) * 0.5f + 0.5f; //DEBUG
				}
				acc *= 1.0f / float(samples);
				if (sum_bright_directions) {
					acc += sum_bright_directions(N);
				}
				out_data.emplace_back(acc);

			}
		}
		std::cout << " done." << std::endl;
	}

	//write a single +x/-x/+y/-y/+z/-z (all stacked in a column with +x at the bottom) file for storage:
	std::cout << "Writing final rgbe png..."; std::cout.flush();
	std::vector< glm::u8vec4 > out_data_rgbe;
	out_data_rgbe.reserve(out_size.x * 6 * out_size.y);
	for (auto const &pix : out_data) {
		out_data_rgbe.emplace_back( float_to_rgbe( pix ) );
	}
	std::cout << " done." << std::endl;
	save_png(out_file, out_size, out_data_rgbe.data(), LowerLeftOrigin);

	{ //DEBUG: tone map and save again:
		std::cout << "Writing tone-mapped png [DEBUG-blur-out.png]..."; std::cout.flush();
		save_tone_mapped_png("DEBUG-blur-out.png", out_size, out_data);
		std::cout << " done." << std::endl;
	}
}
