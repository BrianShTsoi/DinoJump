#include "dino.hpp"
#include "difficulty.hpp"

Dino::Dino()
	: state(Dino_State::RUNNING), step(Step_State::LEFT), y_position(GROUND_HEIGHT),
	  jump_ticks(LEVELS[0].jump_ticks), jump_time(0) {}

void Dino::set_jump_ticks(int ticks) {
	// Changing the airtime mid-air would warp the arc, so only take it on the ground
	if (this->state != Dino_State::JUMPING) {
		this->jump_ticks = ticks;
	}
}

Dino_State Dino::get_state() { return this->state; }

Step_State Dino::get_step() { return this->step; }

int Dino::get_y_position() { return this->y_position; }

void Dino::try_set_input_state(Dino_State input_state) {
	if (this->state == Dino_State::JUMPING) {
		this->jump();
		return;
	}

	switch (input_state) {
		case Dino_State::DUCKING:
			this->duck();
			break;
		case Dino_State::JUMPING:
			this->jump();
			break;
		default:
			this->neutral();
	}
}

void Dino::toggle_step() {
	this->step = (this->step == Step_State::LEFT) ? Step_State::RIGHT : Step_State::LEFT;
}

void Dino::neutral() {
	this->state = Dino_State::RUNNING;
	toggle_step();
}

void Dino::duck() {
	this->state = Dino_State::DUCKING;
	toggle_step();
}

void Dino::jump() {
	this->state = Dino_State::JUMPING;
	this->jump_time++;

	if (this->jump_time >= this->jump_ticks) {
		this->y_position = GROUND_HEIGHT;
		this->jump_time = 0;
		this->state = Dino_State::RUNNING;
		return;
	}

	// Symmetric parabola: 0 at t = 0 and t = jump_ticks, JUMP_PEAK in the middle
	this->y_position = GROUND_HEIGHT + (4 * JUMP_PEAK * jump_time * (jump_ticks - jump_time)) /
										   (jump_ticks * jump_ticks);
}

void Dino::die() {
	if (this->state == Dino_State::DUCKING) {
		this->state = Dino_State::DEAD_DUCKING;
	} else {
		this->state = Dino_State::DEAD;
	}
}
