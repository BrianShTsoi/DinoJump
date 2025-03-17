#include "game.hpp"
#include "bit_array_2d.hpp"
#include "constants.hpp"
#include "dino.hpp"
#include "sprites.hpp"

Game::Game()
	: score(0), cooldown_count(0), collision(false), frame(), dino(Dino()),
	  obs_manager(Obstacle_Manager()) {}
const BitArray2D<DINO_WIDTH> &fetch_dino_sprite();
int Game::get_score() { return this->score; }

bool Game::get_collision() { return this->collision; }

void Game::input(Input_State input) {
	Dino_State input_state;
	switch (input) {
		case DUCK:
			input_state = Dino_State::DUCKING;
			break;
		case JUMP:
			input_state = Dino_State::JUMPING;
			break;
		default:
			input_state = Dino_State::RUNNING;
	}
	this->dino.try_set_input_state(input_state);
}

void Game::update_obstacles() {
	// Shift obstacles
	if (this->obs_manager.shift_obstacles()) {
		this->score++;
	}

	// Handle obstacle generation
	if (cooldown_count == 0) {
		this->obs_manager.generate_obstacle();
		cooldown_count = OBSTACLE_COOLDOWN;
	} else {
		cooldown_count--;
	}
}

const BitArray2D<DINO_WIDTH> &Game::fetch_dino_sprite() {
	switch (this->dino.get_state()) {
		case Dino_State::RUNNING:
			return this->dino.get_step() == Step_State::LEFT ? DINO_LEFT_SPRITE_BIT_ARRAY
															 : DINO_RIGHT_SPRITE_BIT_ARRAY;
		case Dino_State::DUCKING:
			return this->dino.get_step() == Step_State::LEFT ? DINO_LEFT_DUCK_SPRITE_BIT_ARRAY
															 : DINO_RIGHT_DUCK_SPRITE_BIT_ARRAY;
		case Dino_State::JUMPING:
		default:
			return DINO_JUMP_SPRITE_BIT_ARRAY;
	}
}

void Game::draw_dino() {
	const BitArray2D<DINO_WIDTH> &sprite = fetch_dino_sprite();

	int y_position = dino.get_y_position();
	for (int i = 0; i < DINO_WIDTH; ++i) {
		for (int j = 0; j < DINO_HEIGHT; ++j) {
			frame.set(DINO_DRAW_Y + j - y_position, i, sprite.get(j, i));
		}
	}
}

void Game::draw_dino_death() {
	const BitArray2D<DEAD_EYE_DIAMETER> &sprite = DEAD_EYE_SPRITE_BIT_ARRAY;

	// Only modification for dino sprite when dead is the eye
	int dead_eye_y = dino.get_state() == DEAD ? DEAD_EYE_Y : DEAD_DUCK_EYE_Y;
	int y_position = dino.get_y_position();
	for (int i = 0; i < DEAD_EYE_DIAMETER; ++i) {
		for (int j = 0; j < DEAD_EYE_DIAMETER; ++j) {
			frame.set(DINO_DRAW_Y + +dead_eye_y + j - y_position, DEAD_EYE_X + i, sprite.get(j, i));
		}
	}
}

void Game::draw_small_cactus_with_collision(std::array<int, 2> location) {
	const auto &bit_array = SMALL_CACTUS_SPRITE_BIT_ARRAY;

	for (int i = 0; i < CACTUS_WIDTH; ++i) {
		if (i + location[0] >= MAX_X || i + location[0] <= 0) {
			continue;
		}

		for (int j = 0; j < CACTUS_HEIGHT; ++j) {
			if (bit_array.get(j, i)) {
        // TODO
        // uncomment
//				if (frame.get(CACTUS_DRAW_Y + j - location[1], i + location[0]) == 1) {
//					this->collision = true;
//				}
				frame.set(CACTUS_DRAW_Y + j - location[1], i + location[0], bit_array.get(j, i));
			}
		}
	}
}

void Game::draw_large_cactus_with_collision(std::array<int, 2> location) {
	const auto &bit_array = LARGE_CACTUS_SPRITE_BIT_ARRAY;

	for (int i = 0; i < CACTUS_WIDTH; ++i) {
		if (i + location[0] >= MAX_X || i + location[0] <= 0) {
			continue;
		}

		for (int j = 0; j < CACTUS_HEIGHT; ++j) {
			if (bit_array.get(j, i)) {
        // TODO
        // uncomment
//				if (frame.get(CACTUS_DRAW_Y + j - location[1], i + location[0]) == 1) {
//					this->collision = true;
//				}
				frame.set(CACTUS_DRAW_Y + j - location[1], i + location[0], bit_array.get(j, i));
			}
		}
	}
}

void Game::draw_bird_with_collision(std::array<int, 2> location) {
	const auto &bit_array = BIRD_SPRITE_BIT_ARRAY;

	for (int i = 0; i < BIRD_WIDTH; ++i) {
		if (i + location[0] >= MAX_X || i + location[0] <= 0) {
			continue;
		}

		for (int j = 0; j < BIRD_HEIGHT; ++j) {
			if (bit_array.get(j, i)) {
        // TODO
        // uncomment
//				if (frame.get(BIRD_DRAW_Y + j - location[1], i + location[0]) == 1) {
//					this->collision = true;
//				}
				frame.set(BIRD_DRAW_Y + j - location[1], i + location[0], bit_array.get(j, i));
			}
		}
	}
}

void Game::update_frame() {
	frame.clear();

	draw_dino();

	std::vector<Obstacle> obstacles = obs_manager.fetch_obstacles();
	for (auto obs_itr = obstacles.begin(); obs_itr != obstacles.end(); ++obs_itr) {
		switch (obs_itr->get_type()) {
			case Obstacle_Type::SMALL_CACTUS:
				draw_small_cactus_with_collision(obs_itr->get_location());
				break;
			case Obstacle_Type::LARGE_CACTUS:
				draw_large_cactus_with_collision(obs_itr->get_location());
				break;
			case Obstacle_Type::LOW_BIRD:
			case Obstacle_Type::HIGH_BIRD:
				draw_bird_with_collision(obs_itr->get_location());
		}
	}

	if (collision) {
		dino.die();
		draw_dino_death();
	}
}

const Frame &Game::get_frame() const { return this->frame; }
