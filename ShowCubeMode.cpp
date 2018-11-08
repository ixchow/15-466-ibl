#include "ShowCubeMode.hpp"
#include "MenuMode.hpp"

#include "Load.hpp"
#include "cube_program.hpp"
#include "cube_diffuse_program.hpp"
#include "make_vao_for_program.hpp"
#include "load_save_png.hpp"
#include "rgbe.hpp"
#include "data_path.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <random>

extern std::shared_ptr< MenuMode > menu;

//load an rgbe cubemap texture:
GLuint load_cube(std::string const &filename) {
	//assume cube is stacked faces +x,-x,+y,-y,+z,-z:
	glm::uvec2 size;
	std::vector< glm::u8vec4 > data;
	load_png(filename, &size, &data, LowerLeftOrigin);
	if (size.y != size.x * 6) {
		throw std::runtime_error("Expecting stacked faces in cubemap.");
	}

	//convert from rgb+exponent to floating point:
	std::vector< glm::vec3 > float_data;
	float_data.reserve(data.size());
	for (auto const &px : data) {
		float_data.emplace_back(rgbe_to_float(px));
	}

	//upload to cubemap:
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	//the RGB9_E5 format is close to the source format and a lot more efficient to store than full floating point.
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB9_E5, size.x, size.x, 0, GL_RGB, GL_FLOAT, float_data.data() + 0*size.x*size.x);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB9_E5, size.x, size.x, 0, GL_RGB, GL_FLOAT, float_data.data() + 1*size.x*size.x);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB9_E5, size.x, size.x, 0, GL_RGB, GL_FLOAT, float_data.data() + 2*size.x*size.x);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB9_E5, size.x, size.x, 0, GL_RGB, GL_FLOAT, float_data.data() + 3*size.x*size.x);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB9_E5, size.x, size.x, 0, GL_RGB, GL_FLOAT, float_data.data() + 4*size.x*size.x);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB9_E5, size.x, size.x, 0, GL_RGB, GL_FLOAT, float_data.data() + 5*size.x*size.x);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//this will probably be ignored because of GL_TEXTURE_CUBE_MAP_SEAMLESS:
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	//NOTE: turning this on to enable nice filtering at cube map boundaries:
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	GL_ERRORS();

	return tex;
}


Load< GLuint > sky_cube(LoadTagDefault, [](){
	return new GLuint(load_cube(data_path("cape_hill_512.png")));
});

Load< GLuint > diffuse_cube(LoadTagDefault, [](){
	return new GLuint(load_cube(data_path("cape_hill_diffuse.png")));
});

MeshBuffer::Mesh const *ship_rocket = nullptr;
Load< MeshBuffer > ship_meshes(LoadTagDefault, [](){
	auto ret = new MeshBuffer(data_path("ship.pnc"));
	ship_rocket = &(ret->lookup("Rocket"));
	return ret;
});

Load< GLuint > ship_meshes_for_cube_diffuse_program(LoadTagDefault, [](){
	return new GLuint(ship_meshes->make_vao_for_program(cube_diffuse_program->program));
});

uint32_t cube_mesh_count = 0;
Load< GLuint > cube_mesh_for_cube_program(LoadTagDefault, [](){
	//mesh for showing cube map texture:
	glm::vec3 v0(-1.0f,-1.0f,-1.0f);
	glm::vec3 v1(+1.0f,-1.0f,-1.0f);
	glm::vec3 v2(-1.0f,+1.0f,-1.0f);
	glm::vec3 v3(+1.0f,+1.0f,-1.0f);
	glm::vec3 v4(-1.0f,-1.0f,+1.0f);
	glm::vec3 v5(+1.0f,-1.0f,+1.0f);
	glm::vec3 v6(-1.0f,+1.0f,+1.0f);
	glm::vec3 v7(+1.0f,+1.0f,+1.0f);

	std::vector< glm::vec3 > verts{
		v0,v1,v4, v4,v1,v5,
		v0,v2,v1, v1,v2,v3,
		v0,v4,v2, v2,v4,v6,
		v7,v6,v5, v5,v6,v4,
		v7,v5,v3, v3,v5,v1,
		v7,v3,v6, v6,v3,v2
	};

	cube_mesh_count = verts.size();

	//upload verts to GPU:
	GLuint vbo = 0;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//create vertex array object describing layout (position is used as texture coordinate):
	auto Position = MeshBuffer::Attrib(3, GL_FLOAT, MeshBuffer::Attrib::AsFloat, sizeof(glm::vec3), 0);
	auto TexCoord = MeshBuffer::Attrib(3, GL_FLOAT, MeshBuffer::Attrib::AsFloat, sizeof(glm::vec3), 0);

	std::vector< std::pair< char const *, MeshBuffer::Attrib const & > > attribs;
	attribs.emplace_back("Position", Position);
	attribs.emplace_back("TexCoord", TexCoord);

	return new GLuint(make_vao_for_program(vbo, attribs.begin(), attribs.end(), cube_program->program, "cube_program"));
});


ShowCubeMode::ShowCubeMode() {
	//build a basic scene:
	{ //make a cube at the center to show the cubemap on:
		Scene::Object::ProgramInfo cube_info;
		cube_info.program = cube_program->program;
		cube_info.vao = *cube_mesh_for_cube_program;
		cube_info.start = 0;
		cube_info.count = cube_mesh_count;
		cube_info.mvp_mat4 = cube_program->object_to_clip_mat4;
		cube_info.textures[0] = *sky_cube;
		cube_info.texture_targets[0] = GL_TEXTURE_CUBE_MAP;
		Scene::Transform *transform = scene.new_transform();
		Scene::Object *cube = scene.new_object(transform);
		cube->programs[Scene::Object::ProgramTypeDefault] = cube_info;
	}

	{ //add a rocket ship:
		Scene::Object::ProgramInfo info;
		info.program = cube_diffuse_program->program;
		info.vao = *ship_meshes_for_cube_diffuse_program;
		info.start = ship_rocket->start;
		info.count = ship_rocket->count;
		info.mvp_mat4 = cube_diffuse_program->object_to_clip_mat4;
		info.itmv_mat3 = cube_diffuse_program->normal_to_light_mat3;
		info.textures[0] = *diffuse_cube;
		info.texture_targets[0] = GL_TEXTURE_CUBE_MAP;
		Scene::Transform *transform = scene.new_transform();
		Scene::Object *object = scene.new_object(transform);
		object->programs[Scene::Object::ProgramTypeDefault] = info;
		transform->position = glm::vec3(2.0f, 0.0f, 0.0f);
	}


	{ //make a camera:
		Scene::Transform *transform = scene.new_transform();
		camera = scene.new_camera(transform);
		camera->near = 0.1f;
		camera->fovy = glm::radians(60.0f);
	}
}

ShowCubeMode::~ShowCubeMode() {
}

bool ShowCubeMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
		menu->background = shared_from_this();
		Mode::set_current(menu);
		return true;
	}

	if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (!mouse_captured) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_captured = true;
		}
	}
	if (evt.type == SDL_MOUSEBUTTONUP) {
		if (mouse_captured) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			mouse_captured = false;
		}
	}
	if (evt.type == SDL_MOUSEWHEEL) {
		camera_radius *= std::pow(0.5f, evt.wheel.y / 10.0f);
		if (camera_radius < 2.0f) camera_radius = 2.0f;
	}
	if (evt.type == SDL_MOUSEMOTION) {
		if (mouse_captured) {
			float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
			float pitch = -evt.motion.yrel / float(window_size.y) * camera->fovy;

			//update camera angles:
			camera_elevation = glm::clamp(camera_elevation + pitch, glm::radians(-80.0f), glm::radians(80.0f));
			camera_azimuth = camera_azimuth + yaw;
		}
	}

	return false;
}

void ShowCubeMode::update(float elapsed) {
	float ce = std::cos(camera_elevation);
	float se = std::sin(camera_elevation);
	float ca = std::cos(camera_azimuth);
	float sa = std::sin(camera_azimuth);
	camera->transform->position = camera_radius * glm::vec3(ce * ca, ce * sa, se);
	camera->transform->rotation =
		glm::quat_cast(glm::transpose(glm::mat3(glm::lookAt(
			camera->transform->position,
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		))));
}

void ShowCubeMode::draw(glm::uvec2 const &drawable_size) {
	//Draw scene:
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	{ //draw the sky by drawing the cube centered at the camera:
		glDisable(GL_DEPTH_TEST); //don't write to depth buffer
		//only render the back of the cube:
		glDisable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		glUseProgram(cube_program->program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, *sky_cube);

		//make a matrix that acts as if the camera is at the origin:
		glm::mat4 world_to_camera = camera->transform->make_world_to_local();
		world_to_camera[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		glm::mat4 world_to_clip = camera->make_projection() * world_to_camera;
		glm::mat4 object_to_clip = world_to_clip;

		glUniformMatrix4fv(cube_program->object_to_clip_mat4, 1, GL_FALSE, glm::value_ptr(object_to_clip));

		glBindVertexArray(*cube_mesh_for_cube_program);

		glDrawArrays(GL_TRIANGLES, 0, cube_mesh_count);

		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

		//reset state:
		glEnable(GL_DEPTH_TEST);
		//glDisable(GL_CULL_FACE);
		//glCullFace(GL_BACK);
	}


	//Note: no light positions to set up, yay!
	scene.draw(camera);

	GL_ERRORS();
}

