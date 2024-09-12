#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include <chrono>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>

auto timeNow(){
	return std::chrono::system_clock::now().time_since_epoch().count();
}
static auto lastTime = timeNow();
static std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
// New idea - obstacleVelocity increases gradually; failure resets you
static constexpr float baseOV = 10.0f;
static float obstacleVelocity = baseOV;
static constexpr float vStep = 1.0f;
static constexpr float adjustVelocity = 1.0f;
static uint32_t score = 0;
static uint32_t best = 0;
//static bool cameraEnabled = false;
static constexpr float minScale = 0.5f;
static constexpr float maxScale = 3.5f;
static constexpr float maxV = 50.0f;
static glm::vec2 correctScale = glm::vec2(1.0f,1.0f);
//position where the obstacle should reset
static constexpr float oob = -10.0f;
static constexpr float startPos = 40.0f;
static bool failed = false;
// Obstacle intersects player between positions of -0.12 and 0.12
static constexpr float playerBounds = 0.12f;
// Check for if bound was missed
static float lastPos = startPos;
// Obstacle intersects player where player is between 73% and 110% scaling of obstacle
static constexpr float errorMax = 1.1f;
static constexpr float errorMin = 0.73f;


void resetObstacle(bool died, Scene::Transform *obsout){
	static float range = maxScale - minScale;
	correctScale.x = (float(rng()) * range) / float(rng.max()) + minScale;
	correctScale.y = (float(rng()) * range) / float(rng.max()) + minScale;
	//obsin->scale = glm::vec3(correctScale.x, 1.0f, correctScale.y);
	obsout->scale = glm::vec3(correctScale.x, 1.0f, correctScale.y);
	// std::cout << "Current: " + glm::to_string(obsout->scale);
	if (!died) {
		obstacleVelocity = std::min(obstacleVelocity + vStep, maxV);
		score += 1;
		best = std::max(best, score);
	} else {
		obstacleVelocity = baseOV;
		score = 0;
	}
	obsout->position.y = startPos;
}

void dieUndie(Scene::Transform *player, Scene::Transform *dead){
	glm::vec3 tempPos = player->position;
	player->position = dead->position;
	dead->position = tempPos;
}


GLuint bridge_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > bridge_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("bridge.pnct"));
	bridge_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});


Load< Scene > bridge_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("bridge.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = bridge_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = bridge_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*bridge_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		//if (transform.name == "Hip.FL") hip = &transform;
		//else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		//else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
		if (transform.name == "Player") player = &transform;
		else if (transform.name == "DeadPlayer") dead = &transform;
		else if (transform.name == "ObsOut") obsout = &transform;
		else if (transform.name == "ObsIn") obsin = &transform;
	}
	if (player == nullptr) throw std::runtime_error("Player not found.");
	if (dead == nullptr) throw std::runtime_error("DeadPlayer not found.");
	if (obsout == nullptr) throw std::runtime_error("Obstacle (Outer) not found.");
	if (obsin == nullptr) throw std::runtime_error("Obstacle (Inner) not found.");
	obsout->position.y = startPos;
	dead->position.y = -startPos;

	/*hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;*/

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}


PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a || evt.key.keysym.sym == SDLK_LEFT ) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d || evt.key.keysym.sym == SDLK_RIGHT ) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w || evt.key.keysym.sym == SDLK_UP ) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s || evt.key.keysym.sym == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a || evt.key.keysym.sym == SDLK_LEFT ) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d || evt.key.keysym.sym == SDLK_RIGHT ) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w || evt.key.keysym.sym == SDLK_UP ) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s || evt.key.keysym.sym == SDLK_DOWN ) {
			down.pressed = false;
			return true;
		} /*else if (evt.key.keysym.sym == SDLK_BACKSPACE	) {
			cameraEnabled = !cameraEnabled;
		}*/
	} /*else if (cameraEnabled && evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (cameraEnabled && evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}*/

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);

	/*hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);*/

	//combine inputs into a move:
	// constexpr float PlayerSpeed = 30.0f;
	
	obsout->position.y -= obstacleVelocity * elapsed;
	if (obsout->position.y < oob) {
		resetObstacle(failed, obsout);
		if (failed){
			dieUndie(player, dead);
		}
		failed = false;
	}
	
	glm::vec2 move = glm::vec2(0.0f);
	if (left.pressed && !right.pressed) move.x =-1.0f;
	if (!left.pressed && right.pressed) move.x = 1.0f;
	if (down.pressed && !up.pressed) move.y =-1.0f;
	if (!down.pressed && up.pressed) move.y = 1.0f;

	// make it so that moving diagonally doesn't go faster (unneeded):
	// if (move != glm::vec2(0.0f)) move = glm::normalize(move) * adjustVelocity * elapsed;
	//if (!cameraEnabled) {
	//Rate of change
	static constexpr float roc = 0.005f;
	static constexpr float minChange = 0.002f;
	float xmod = (player->scale.x - minScale) / maxScale;
	float ymod = (player->scale.z - minScale) / maxScale;
	xmod = std::max(roc * obstacleVelocity * xmod * (1-xmod), minChange);
	ymod = std::max(roc * obstacleVelocity * ymod * (1-ymod), minChange);
	player->scale += glm::vec3(move.x * adjustVelocity * xmod, 0.0f, move.y * adjustVelocity * ymod);
	player->scale = glm::vec3(std::clamp(player->scale.x, minScale, maxScale), 1.0f, std::clamp(player->scale.z, minScale, maxScale));
	dead->scale += glm::vec3(move.x * adjustVelocity * xmod, 0.0f, move.y * adjustVelocity * ymod);
	dead->scale = glm::vec3(std::clamp(player->scale.x, minScale, maxScale), 1.0f, std::clamp(player->scale.z, minScale, maxScale));
	//player->position = glm::vec3(0.0f,0.0f,0.0f);
	//}

	/*static auto max = rng.max();
	if (rng() < max / 1000){
		std::cout << (glm::to_string(player->scale)) << std::endl;

	}*/


	/*if (cameraEnabled){
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}*/
	
	if ((obsout->position.y < playerBounds && lastPos > playerBounds)){
		if (player->scale.x > errorMax * correctScale.x || player->scale.x < errorMin * correctScale.x) {
			failed = true;
			dieUndie(player,dead);
		}
		else if (player->scale.z > errorMax * correctScale.y || player->scale.z < errorMin * correctScale.y){
			failed = true;
			dieUndie(player,dead);
		}
	}
	lastPos = obsout->position.y;
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
	static float lumi = 4.0f;
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f*lumi, 1.0f*lumi, 0.95f*lumi)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
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
		lines.draw_text("Current: " + std::to_string(score) + "; " + "Best: " + std::to_string(best),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Current: " + std::to_string(score) + "; " + "Best: " + std::to_string(best),
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
