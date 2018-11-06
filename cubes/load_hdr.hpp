#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

//load an .hdr / .pic image into a [rgbe] buffer.
//origin will be placed at the lower left (opengl-style)
//NOTE: this code is likely to work with a subset (only) of hdr files

//(will throw on error)

void load_hdr(std::string const &file, glm::uvec2 *size, std::vector< glm::u8vec4 > *data);
