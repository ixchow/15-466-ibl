#include "Scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

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

void Scene::draw(Scene::Camera const *camera) {
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
