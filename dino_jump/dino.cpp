#include "dino.hpp"

Dino::Dino()
	: state(Dino_State::RUNNING), step(Step_State::LEFT), y_position(0),
	  jump_velocity(INIT_JUMP_VELOCITY), jump_time(0) {}

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

	int curve = (PARABOLIC_MOTION * jump_time * (jump_time - JUMP_DURATION)) /
				(JUMP_DURATION * JUMP_DURATION);
	this->y_position = INIT_JUMP_VELOCITY + curve;
	this->jump_time++;

	if (this->y_position <= 0) {
		this->y_position = 0;
		this->jump_velocity = INIT_JUMP_VELOCITY;
		this->jump_time = 0;
		this->state = Dino_State::RUNNING;
	}
}

void Dino::die() {
	if (this->state == Dino_State::DUCKING) {
		this->state = Dino_State::DEAD_DUCKING;
	} else {
		this->state = Dino_State::DEAD;
	}
}
