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
int difficulty = 5;

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
		planet.tentative_pos = glm::vec3(std::uniform_real_distribution<float>(-25.0f + planet.radius, 25.0f - planet.radius)(gen), std::uniform_real_distribution<float>(-25.0f + planet.radius, 25.0f - planet.radius)(gen), std::uniform_real_distribution<float>(-12.5f + planet.radius, 12.5f - planet.radius)(gen));
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
}

void make_level(PlayMode &play_mode) {
	planet_data = generate_level();
	for (PlanetData &planet: planet_data) {
		spawn_planet(play_mode, planet);
	}
}

PlayMode::PlayMode() : scene(*cage_scene) {
	//get pointers to leg for convenience:
	// for (auto &transform : scene.transforms) {
	// 	if (transform.name == "Hip.FL") hip = &transform;
	// 	else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
	// 	else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	// }
	// if (hip == nullptr) throw std::runtime_error("Hip not found.");
	// if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	// if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	// hip_base_rotation = hip->rotation;
	// upper_leg_base_rotation = upper_leg->rotation;
	// lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//spawn_planet(*this, life);
	//spawn_planet(*this, asteroid);
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

	//slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);

	// hip->rotation = hip_base_rotation * glm::angleAxis(
	// 	glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 1.0f, 0.0f)
	// );
	// upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );
	// lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
	// 	glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
	// 	glm::vec3(0.0f, 0.0f, 1.0f)
	// );

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
