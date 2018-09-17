#include "Scene.hpp"
#include "read_chunk.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>

glm::mat4 Scene::Transform::make_local_to_parent() const {
	return glm::mat4( //translate
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(position, 1.0f)
	)
	* glm::mat4_cast(rotation) //rotate
	* glm::mat4( //scale
		glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, scale.z, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
	);
}

glm::mat4 Scene::Transform::make_parent_to_local() const {
	glm::vec3 inv_scale;
	inv_scale.x = (scale.x == 0.0f ? 0.0f : 1.0f / scale.x);
	inv_scale.y = (scale.y == 0.0f ? 0.0f : 1.0f / scale.y);
	inv_scale.z = (scale.z == 0.0f ? 0.0f : 1.0f / scale.z);
	return glm::mat4( //un-scale
		glm::vec4(inv_scale.x, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, inv_scale.y, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, inv_scale.z, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
	)
	* glm::mat4_cast(glm::inverse(rotation)) //un-rotate
	* glm::mat4( //un-translate
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-position, 1.0f)
	);
}

glm::mat4 Scene::Transform::make_local_to_world() const {
	if (parent) {
		return parent->make_local_to_world() * make_local_to_parent();
	} else {
		return make_local_to_parent();
	}
}

glm::mat4 Scene::Transform::make_world_to_local() const {
	if (parent) {
		return make_parent_to_local() * parent->make_world_to_local();
	} else {
		return make_parent_to_local();
	}
}

void Scene::Transform::DEBUG_assert_valid_pointers() const {
	if (parent == nullptr) {
		//if no parent, can't have siblings:
		assert(prev_sibling == nullptr);
		assert(next_sibling == nullptr);
	} else {
		//if have parent, last child if and only if no next sibling:
		assert((next_sibling == nullptr) == (this == parent->last_child));
	}
	//check proper return pointers from neighbors:
	assert(prev_sibling == nullptr || prev_sibling->next_sibling == this);
	assert(next_sibling == nullptr || next_sibling->prev_sibling == this);
	assert(last_child == nullptr || last_child->parent == this);
}


void Scene::Transform::set_parent(Transform *new_parent, Transform *before) {
	DEBUG_assert_valid_pointers();
	assert(before == nullptr || (new_parent != nullptr && before->parent == new_parent));
	if (parent) {
		//remove from existing parent:
		if (prev_sibling) prev_sibling->next_sibling = next_sibling;
		if (next_sibling) next_sibling->prev_sibling = prev_sibling;
		else parent->last_child = prev_sibling;
		next_sibling = prev_sibling = nullptr;
	}
	parent = new_parent;
	if (parent) {
		//add to new parent:
		if (before) {
			prev_sibling = before->prev_sibling;
			next_sibling = before;
			next_sibling->prev_sibling = this;
		} else {
			prev_sibling = parent->last_child;
			parent->last_child = this;
		}
		if (prev_sibling) prev_sibling->next_sibling = this;
	}
	DEBUG_assert_valid_pointers();
}

//---------------------------

glm::mat4 Scene::Camera::make_projection() const {
	return glm::infinitePerspective( fovy, aspect, near );
}

//---------------------------

//templated helper functions to avoid having to write the same new/delete code three times:
template< typename T, typename... Args >
T *list_new(T * &first, Args&&... args) {
	T *t = new T(std::forward< Args >(args)...); //"perfect forwarding"
	if (first) {
		t->alloc_next = first;
		first->alloc_prev_next = &t->alloc_next;
	}
	t->alloc_prev_next = &first;
	first = t;
	return t;
}

template< typename T >
void list_delete(T * t) {
	assert(t && "It is invalid to delete a null scene object [yes this is different than 'delete']");
	assert(t->alloc_prev_next);
	if (t->alloc_next) {
		t->alloc_next->alloc_prev_next = t->alloc_prev_next;
	}
	*t->alloc_prev_next = t->alloc_next;
	//PARANOIA:
	t->alloc_next = nullptr;
	t->alloc_prev_next = nullptr;
}

Scene::Transform *Scene::new_transform() {
	return list_new< Scene::Transform >(first_transform);
}

void Scene::delete_transform(Scene::Transform *transform) {
	list_delete< Scene::Transform >(transform);
}

Scene::Object *Scene::new_object(Scene::Transform *transform) {
	assert(transform && "Scene::Object must be attached to a transform.");
	return list_new< Scene::Object >(first_object, transform);
}

void Scene::delete_object(Scene::Object *object) {
	list_delete< Scene::Object >(object);
}

Scene::Camera *Scene::new_camera(Scene::Transform *transform) {
	assert(transform && "Scene::Camera must be attached to a transform.");
	return list_new< Scene::Camera >(first_camera, transform);
}

void Scene::delete_camera(Scene::Camera *object) {
	list_delete< Scene::Camera >(object);
}

void Scene::draw(Scene::Camera const *camera) const {
	assert(camera && "Must have a camera to draw scene from.");

	glm::mat4 world_to_camera = camera->transform->make_world_to_local();
	glm::mat4 world_to_clip = camera->make_projection() * world_to_camera;

	for (Scene::Object *object = first_object; object != nullptr; object = object->alloc_next) {
		glm::mat4 local_to_world = object->transform->make_local_to_world();

		//compute modelview+projection (object space to clip space) matrix for this object:
		glm::mat4 mvp = world_to_clip * local_to_world;

		//compute modelview (object space to camera local space) matrix for this object:
		glm::mat4 mv = local_to_world;

		//NOTE: inverse cancels out transpose unless there is scale involved
		glm::mat3 itmv = glm::inverse(glm::transpose(glm::mat3(mv)));

		//set up program uniforms:
		glUseProgram(object->program);
		if (object->program_mvp_mat4 != -1U) {
			glUniformMatrix4fv(object->program_mvp_mat4, 1, GL_FALSE, glm::value_ptr(mvp));
		}
		if (object->program_mv_mat4x3 != -1U) {
			glUniformMatrix4x3fv(object->program_mv_mat4x3, 1, GL_FALSE, glm::value_ptr(mv));
		}
		if (object->program_itmv_mat3 != -1U) {
			glUniformMatrix3fv(object->program_itmv_mat3, 1, GL_FALSE, glm::value_ptr(itmv));
		}

		if (object->set_uniforms) object->set_uniforms();

		glBindVertexArray(object->vao);

		//draw the object:
		glDrawArrays(GL_TRIANGLES, object->start, object->count);
	}
}


Scene::~Scene() {
	while (first_camera) {
		delete_camera(first_camera);
	}
	while (first_object) {
		delete_object(first_object);
	}
	while (first_transform) {
		delete_transform(first_transform);
	}
}

void Scene::load(std::string const &filename,
	std::function< void(Scene &, Transform *, std::string const &) > const &on_object) {

	std::ifstream file(filename, std::ios::binary);

	std::vector< char > names;
	read_chunk(file, "str0", &names);

	struct HierarchyEntry {
		uint32_t parent;
		uint32_t name_begin;
		uint32_t name_end;
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;
	};
	static_assert(sizeof(HierarchyEntry) == 4 + 4 + 4 + 4*3 + 4*4 + 4*3, "HierarchyEntry is packed.");
	std::vector< HierarchyEntry > hierarchy;
	read_chunk(file, "xfh0", &hierarchy);

	struct MeshEntry {
		uint32_t transform;
		uint32_t name_begin;
		uint32_t name_end;
	};
	static_assert(sizeof(MeshEntry) == 4 + 4 + 4, "MeshEntry is packed.");
	std::vector< MeshEntry > meshes;
	read_chunk(file, "msh0", &meshes);

	struct CameraEntry {
		uint32_t transform;
		char type[4]; //"pers" or "orth"
		float data; //fov in degrees for 'pers', scale for 'orth'
		float clip_near, clip_far;
	};
	static_assert(sizeof(CameraEntry) == 4 + 4 + 4 + 4 + 4, "CameraEntry is packed.");
	std::vector< CameraEntry > cameras;
	read_chunk(file, "cam0", &cameras);

	struct LightEntry {
		uint32_t transform;
		char type;
		glm::u8vec3 color;
		float energy;
		float distance;
		float fov;
	};
	static_assert(sizeof(LightEntry) == 4 + 1 + 3 + 4 + 4 + 4, "LightEntry is packed.");
	std::vector< LightEntry > lights;
	read_chunk(file, "lmp0", &lights);

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in scene file '" << filename << "'" << std::endl;
	}

	//--------------------------------
	//Now that file is loaded, create transforms for hierarchy entries:

	std::vector< Transform * > hierarchy_transforms;
	hierarchy_transforms.reserve(hierarchy.size());

	for (auto const &h : hierarchy) {
		Transform *t = new_transform();
		if (h.parent != -1U) {
			if (h.parent >= hierarchy_transforms.size()) {
				throw std::runtime_error("scene file '" + filename + "' did not contain transforms in topological-sort order.");
			}
			t->set_parent(hierarchy_transforms[h.parent]);
		}

		if (h.name_begin <= h.name_end && h.name_end <= names.size()) {
			t->name = std::string(names.begin() + h.name_begin, names.begin() + h.name_end);
		} else {
				throw std::runtime_error("scene file '" + filename + "' contains hierarchy entry with invalid name indices");
		}

		t->position = h.position;
		t->rotation = h.rotation;
		t->scale = h.scale;

		hierarchy_transforms.emplace_back(t);
	}
	assert(hierarchy_transforms.size() == hierarchy.size());

	for (auto const &m : meshes) {
		if (m.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains mesh entry with invalid transform index (" + std::to_string(m.transform) + ")");
		}
		if (!(m.name_begin <= m.name_end && m.name_end <= names.size())) {
			throw std::runtime_error("scene file '" + filename + "' contains mesh entry with invalid name indices");
		}
		std::string name = std::string(names.begin() + m.name_begin, names.begin() + m.name_end);

		if (on_object) {
			on_object(*this, hierarchy_transforms[m.transform], name);
		}

	}

	for (auto const &c : cameras) {
		if (c.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains camera entry with invalid transform index (" + std::to_string(c.transform) + ")");
		}
		if (std::string(c.type, 4) != "pers") {
			std::cout << "Ignoring non-perspective camera (" + std::string(c.type, 4) + ") stored in file." << std::endl;
			continue;
		}
		Camera *camera = new_camera(hierarchy_transforms[c.transform]);
		camera->fovy = c.data / 180.0f * 3.1415926f; //FOV is stored in degrees; convert to radians.
		camera->near = c.clip_near;
		//N.b. far plane is ignored because cameras use infinite perspective matrices.
	}

}
