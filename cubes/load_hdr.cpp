#include "load_hdr.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

#include <glm/gtc/type_ptr.hpp>

//with reference to:
//  http://paulbourke.net/dataformats/pic/
//and the 'pic' section of:
//  http://radsite.lbl.gov/radiance/refer/filefmts.pdf

void load_hdr(std::string const &filename, glm::uvec2 *size_, std::vector< glm::u8vec4 > *data_) {
	assert(size_);
	assert(data_);
	auto &size = *size_;
	auto &data = *data_;

	std::ifstream in(filename, std::ios::binary);
	{ //check header:
		char header[11]; //expecting "#?RADIANCE\n"
		in.read(header, 11);
		if (std::string(header, header+11) != "#?RADIANCE\n") {
			throw std::runtime_error("hdr file '" + filename + "' does not start with expected header.");
		}
	}
	{ //read information lines:
		std::string FORMAT = "";

		std::string line;
		while (std::getline(in, line)) {
			if (line == "") break; //blank line is end of headers.
			if (line[0] == '#') continue; //ignore comment/info lines
			{ //look for assignments:
				auto i = line.find('=');
				if (i != std::string::npos) {
					std::string variable = line.substr(0,i);
					std::string value = line.substr(i+1);
					if (variable == "FORMAT") {
						FORMAT = value;
					} else {
						//silently ignore.
					}
				}
			}
			std::cerr << "WARNING: unknown header line: '" << line << "'" << std::endl;
		}

		if (FORMAT == "") {
			std::cerr << "WARNING: no 'FORMAT=' in file; assuming 32-bit_rle_rgbe" << std::endl;
			FORMAT="32-bit_rle_rgbe";
		}
		if (FORMAT != "32-bit_rle_rgbe") {
			throw std::runtime_error("hdr file is in unsupported format (" + FORMAT + ")");
		}
	}

	//indicate how picture needs to be flipped after reading in order to arrive at a lower-left-origin coordinate system:
	bool flip_y;
	bool flip_x;
	bool transpose;
	{ //read resolution line:
		std::string line;
		if (!getline(in, line)) {
			throw std::runtime_error("hdr file '" + filename + "' missing resolution string.");
		}
		std::istringstream str(line);
		std::string l1, l2;
		if (!(str >> l1 >> size.y >> l2 >> size.x)) {
			throw std::runtime_error("hdr file '" + filename + "' has bad resolution line '" + line + "'.");
		}
		std::string temp;
		if (str >> temp) {
			std::cerr << "WARNING: ignoring trailing data in hdr file resolution line." << std::endl;
		}
		if ((l1 == "-Y" && l2 == "+X")
		 || (l1 == "-Y" && l2 == "-X")
		 || (l1 == "+Y" && l2 == "+X")
		 || (l1 == "+Y" && l2 == "-X") ) {
			flip_y = (l1[0] == '-');
			flip_x = (l2[0] == '-');
			transpose = false;
		} else if ((l1 == "-X" && l2 == "+Y")
		        || (l1 == "-X" && l2 == "-Y")
		        || (l1 == "+X" && l2 == "+Y")
		        || (l1 == "+X" && l2 == "-Y") ) {
			flip_x = (l1[0] == '-');
			flip_y = (l2[0] == '-');
			transpose = true;
		} else {
			throw std::runtime_error("hdr file '" + filename + "' has unexpected layout '" + line + "'.");
		}
	}

	data.resize(size.x*size.y);

	//read the scanlines:

	auto get_pix = [&in,&filename]() -> glm::u8vec4 {
		glm::u8vec4 ret;
		if (!in.read(reinterpret_cast< char * >(glm::value_ptr(ret)), 4)) {
			throw std::runtime_error("hdr file '" + filename + "' did not have a pixel when we were expecting one.");
		}
		return ret;
	};
	auto get_byte = [&in,&filename]() -> uint8_t {
		uint8_t ret;
		if (!in.read(reinterpret_cast< char * >(&ret), 1)) {
			throw std::runtime_error("hdr file '" + filename + "' did not have a byte when we were expecting one.");
		}
		return ret;
	};

	for (uint32_t y = 0; y < size.y; ++y) {
		//with reference to read code in ray/src/common/color.c (radiance source code)

		//Look at first pixel of line to determine format:
		glm::u8vec4 pix = get_pix();
		//"new" RLE format [separated components]:
		if (pix.r == 2 && pix.g == 2 && (pix.b & 128) == 0) {
			uint32_t len = (uint32_t(pix.b) << 8) | uint32_t(pix.a);
			if (len != size.x) {
				throw std::runtime_error("hdr file '" + filename + "' has scanline that should be new RLE but reports an incorrect length (" + std::to_string(len) + " instead of " + std::to_string(size.x) + ").");
			}
			for (uint32_t c = 0; c < 4; ++c) {
				for (uint32_t x = 0; x < size.x; /* later */) {
					uint8_t code = get_byte();
					if (code > 128) {
						//run, mask upper bit to get length:
						code &= 0x7F;
						if (x + code > size.x) {
							throw std::runtime_error("hdr file '" + filename + "' has scanline over-run.");
						}
						//get run value:
						uint8_t val = get_byte();
						while (code) {
							data[y*size.x+x][c] = val;
							++x;
							--code;
						}
					} else {
						//non-run:
						if (x + code > size.x) {
							throw std::runtime_error("hdr file '" + filename + "' has scanline over-run.");
						}
						while (code) {
							data[y*size.x+x][c] = get_byte();
							++x;
							--code;
						}
					}
				}
			}
		} else {
			//the reference code does this clever rshift thing; I'm going to more-or-less do the same thing, while at the same time being concerned that the code will pretty much never be touched.
			uint32_t rshift = 0;
			for (uint32_t x = 0; x < size.x; /* later */) {
				if (x != 0) pix = get_pix();
				if (pix.r == 1 && pix.r == 1 && pix.g == 1) {
					if (x == 0) {
						throw std::runtime_error("hdr file '" + filename + "' specifies repeat at the start of a scanline");
					}
					for (uint32_t c = uint32_t(pix.a) << rshift; c > 0; --c) {
						data[y*size.x+x] = data[y*size.x+x-1];
						++x;
						if (x >= size.x) {
							throw std::runtime_error("hdr file '" + filename + "' specifies repeat that overflows a scanline");
						}
					}
					rshift += 8;
				} else {
					data[y*size.x+x] = pix;
					++x;
					rshift = 0;
				}
			}
		}
	}

	if (flip_y) {
		std::vector< glm::u8vec4 > new_data;
		new_data.resize(size.x*size.y);
		for (uint32_t y = 0; y < size.y; ++y) {
			uint32_t ny = size.y-1-y;
			for (uint32_t x = 0; x < size.x; ++x) {
				new_data[ny*size.x+x] = data[y*size.x+x];
			}
		}
		data = std::move(new_data);
	}
	if (flip_x) {
		std::vector< glm::u8vec4 > new_data;
		new_data.resize(size.x*size.y);
		for (uint32_t y = 0; y < size.y; ++y) {
			for (uint32_t x = 0; x < size.x; ++x) {
				uint32_t nx = size.x-1-x;
				new_data[y*size.x+nx] = data[y*size.x+x];
			}
		}
		data = std::move(new_data);
	}
	if (transpose) {
		glm::uvec2 new_size(size.y, size.x);
		std::vector< glm::u8vec4 > new_data;
		new_data.resize(new_size.y*new_size.x);
		for (uint32_t y = 0; y < size.y; ++y) {
			for (uint32_t x = 0; x < size.x; ++x) {
				uint32_t nx = y;
				uint32_t ny = x;
				new_data[ny*new_size.x+nx] = data[y*size.x+x];
			}
		}
		size = new_size;
		data = std::move(new_data);
	}
}
