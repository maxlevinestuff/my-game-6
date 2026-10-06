#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <string>
#include <array>
#include <vector>

int score = 0;
int difficulty = 25;

constexpr float horizontal_cage_increase = 10.0f;
float cage_x = 25.0f * horizontal_cage_increase;
float cage_y = 25.0f * horizontal_cage_increase;
float cage_z = 12.5f;

std::random_device rd;
std::mt19937 gen(rd());

const int PLANET_COUNT = 1;
enum PlanetType {
	asteroid
};
struct PlanetData {
	PlanetType type;
	Scene::Transform *transform;
	glm::vec3 tentative_pos;
	glm::vec3 velocity;
	float radius;
};
int spawned_index = 0;
std::vector<PlanetData> planet_data;

std::array<GLuint, PLANET_COUNT> planet_meshes_for_lit_color_texture_program = {};

Load< std::array<MeshBuffer, PLANET_COUNT> > planet_meshes(LoadTagDefault, []() -> std::array<MeshBuffer, PLANET_COUNT> const * {
	auto *rets = new std::array<MeshBuffer, PLANET_COUNT> {
		MeshBuffer(data_path("asteroid.pnct"))
	};
	for (int i = 0; i < PLANET_COUNT; i++) {
		planet_meshes_for_lit_color_texture_program[i] = (*rets)[i].make_vao_for_program(lit_color_texture_program->program);
	}
	// MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	// hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return rets;
});

GLuint cage_meshes_for_lit_color_texture_program = 0;

Load< MeshBuffer > cage_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	auto *ret = new MeshBuffer(data_path("cage.pnct"));
	cage_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	// MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	// hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > cage_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("cage.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = cage_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = cage_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

std::vector<PlanetData> generate_level() {
	start_over:
	std::vector<PlanetData> level;
	for (int i = 0; i < difficulty; i++) {
		int attempts = 0;
		replace:
		attempts++;
		PlanetData planet {};
		planet.type = asteroid;
		planet.radius = std::uniform_real_distribution<float>(1.0f, 3.0f)(gen);
		planet.tentative_pos = glm::vec3(std::uniform_real_distribution<float>(-cage_x + planet.radius, cage_x - planet.radius)(gen), std::uniform_real_distribution<float>(-cage_y + planet.radius, cage_y - planet.radius)(gen), std::uniform_real_distribution<float>(-cage_z + planet.radius, cage_z - planet.radius)(gen));
		for (PlanetData &other : level) {
			float min_dist = planet.radius + other.radius;
			if (glm::distance(planet.tentative_pos, other.tentative_pos) < min_dist) {
				if (attempts > 20)
					goto start_over;
				else
					goto replace;
			}
		}
		level.push_back(planet);
	}
	return level;
}

void spawn_planet(PlayMode &play_mode, PlanetData &planetData) {
	Mesh const &mesh = (*planet_meshes)[planetData.type].lookup("Asteroid");

	play_mode.scene.transforms.emplace_back();
	Scene::Transform &transform = play_mode.scene.transforms.back();

	play_mode.scene.drawables.emplace_back(&transform);
	Scene::Drawable &drawable = play_mode.scene.drawables.back();

	drawable.pipeline = lit_color_texture_program_pipeline;

	drawable.pipeline.vao = planet_meshes_for_lit_color_texture_program[planetData.type];
	drawable.pipeline.type = mesh.type;
	drawable.pipeline.start = mesh.start;
	drawable.pipeline.count = mesh.count;

	planetData.transform = &transform;
	planetData.transform->position = planetData.tentative_pos;
	planetData.transform->scale = glm::vec3(planetData.radius);
	//planetData.radius = std::uniform_real_distribution<float>(0.6f, 2.0f)(gen);
	planetData.velocity = glm::vec3(std::uniform_real_distribution<float>(-1.0f, 1.0f)(gen), std::uniform_real_distribution<float>(-1.0f, 1.0f)(gen), std::uniform_real_distribution<float>(-1.0f, 1.0f)(gen));
	spawned_index++;
}

void make_level(PlayMode &play_mode) {
	std::vector<PlanetData> level = generate_level();
	for (int i = 0; i < level.size(); i++) {
		if (i < planet_data.size()) {
			PlanetData &planet = planet_data[i];
			planet.radius = level[i].radius;
			planet.transform->position = planet.tentative_pos;
			planet.transform->scale = glm::vec3(planet.radius);
		} else {
			spawn_planet(play_mode, level[i]);
			planet_data.push_back(level[i]);
		}
	}
}

Scene::Transform *rocket = nullptr;
Scene::Transform *cage = nullptr;

PlayMode::PlayMode() : scene(*cage_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Rocket") rocket = &transform;
		else if (transform.name == "Cage") cage = &transform;
	}
	if (rocket == nullptr) throw std::runtime_error("rocket not found.");
	if (cage == nullptr) throw std::runtime_error("cage leg not found.");

	cage->scale = glm::vec3(horizontal_cage_increase, horizontal_cage_increase, 1.0f);

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//spawn_planet(*this, life);
	//spawn_planet(*this, asteroid);
	make_level(*this);
	make_level(*this);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_LEFT) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_UP) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_DOWN) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	for (PlanetData &planet : planet_data) {
		glm::vec3 &pos = planet.transform->position;
		pos += planet.velocity * elapsed;
		if (pos.x > (cage_x - planet.radius)) {
			pos.x = cage_x - planet.radius;
			planet.velocity.x = -planet.velocity.x;
		}
		if (pos.x < -(cage_x - planet.radius)) {
			pos.x = -(cage_x - planet.radius);
			planet.velocity.x = -planet.velocity.x;
		}

		if (pos.y > (cage_y - planet.radius)) {
			pos.y = cage_y - planet.radius;
			planet.velocity.y = -planet.velocity.y;
		}
		if (pos.y < -(cage_y - planet.radius)) {
			pos.y = -(cage_y - planet.radius);
			planet.velocity.y = -planet.velocity.y;
		}

		if (pos.z > (cage_z - planet.radius)) {
			pos.z = cage_z - planet.radius;
			planet.velocity.z = -planet.velocity.z;
		}
		if (pos.z < -(cage_z - planet.radius)) {
			pos.z = -(cage_z - planet.radius);
			planet.velocity.z = -planet.velocity.z;
		}
	}

	for (int i = 0; i < planet_data.size(); i++) {
		for (int j = i + 1; j < planet_data.size(); j++) {
			PlanetData &planet1 = planet_data[i];
			PlanetData &planet2 = planet_data[j];
			glm::vec3 &pos1 = planet_data[i].transform->position;
			glm::vec3 &pos2 = planet_data[j].transform->position;
			float dist = glm::distance(pos1, pos2);
			if (dist > (planet1.radius + planet2.radius))
				continue;
			glm::vec3 normal = (pos1 - pos2) / dist;
			float approach = glm::dot(planet1.velocity - planet2.velocity, normal);
			planet1.velocity -= approach * normal;
			planet2.velocity += approach * normal;
		}
	}

	glm::vec3 forward = rocket->rotation * glm::vec3(-1.0f, 0.0f, 0.0f);
	rocket->position += forward * elapsed * 3.0f;

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		float ofs = 2.0f / drawable_size.y;

		lines.draw_text("$" + std::to_string(score),
			glm::vec3(-aspect + 0.1f * H, 1.0 - 1.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("$" + std::to_string(score),
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0 - 1.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
