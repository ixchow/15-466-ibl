#include "WalkMesh.hpp"

WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), triangles(triangles_) {
	//TODO: construct next_vertex map
}

WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	WalkPoint closest;
	//TODO: iterate through triangles
	//TODO: for each triangle, find closest point on triangle to world_point
	//TODO: if point is closest, closest.triangle gets the current triangle, closest.weights gets the barycentric coordinates
	return closest;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
	//TODO: project step to barycentric coordinates to get weights_step
	glm::vec3 weights_step;

	//TODO: when does wp.weights + t * weights_step cross a triangle edge?
	float t = 1.0f;

	if (t >= 1.0f) { //if a triangle edge is not crossed
		//TODO: wp.weights gets moved by weights_step, nothing else needs to be done.

	} else { //if a triangle edge is crossed
		//TODO: wp.weights gets moved to triangle edge, and step gets reduced
		//if there is another triangle over the edge:
			//TODO: wp.triangle gets updated to adjacent triangle
			//TODO: step gets rotated over the edge
		//else if there is no other triangle over the edge:
			//TODO: wp.triangle stays the same.
			//TODO: step gets updated to slide along the edge
	}
}
