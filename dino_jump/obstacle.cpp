#include "obstacle.hpp"

/* Obstacle Functions */

Obstacle::Obstacle(Obstacle_Type type) : type(type) {
	switch (type) {
		case SMALL_CACTUS:
			this->y_position = 0;
			this->x_position = MAX_X - 1;
			break;
		case LARGE_CACTUS:
			this->y_position = 0;
			this->x_position = MAX_X - 1;
			break;
		case LOW_BIRD:
			this->y_position = 18;
			this->x_position = MAX_X - 1;
			break;
		case HIGH_BIRD:
			this->y_position = 28;
			this->x_position = MAX_X - 1;
			break;
		default:
			break;
	}
}

Obstacle_Type Obstacle::get_type() { return this->type; }

std::array<int, 2> Obstacle::get_location() { return {this->x_position, this->y_position}; }

bool Obstacle::shift_location() {
	this->x_position -= 3;
	int width = 0;
	switch (this->type) {
		case SMALL_CACTUS:
		case LARGE_CACTUS:
			width = CACTUS_WIDTH;
			break;
		case LOW_BIRD:
		case HIGH_BIRD:
			width = BIRD_WIDTH;
			break;
		default:
			break;
	}
	return this->x_position + width >= 0;
}

/* Obstacle Manager Functions */

Obstacle_Manager::Obstacle_Manager() : obstacles() {
	// Set seed for random number generator
	std::srand(static_cast<unsigned int>(std::time(0)));
}

std::vector<Obstacle> Obstacle_Manager::fetch_obstacles() { return this->obstacles; }

void Obstacle_Manager::generate_obstacle() {
	// Generate random value in enum range
	int random_value = std::rand() % static_cast<int>(Obstacle_Type::HIGH_BIRD + 1);
	// Add new obstacle of type to obstacles vector
	this->obstacles.push_back(Obstacle(static_cast<Obstacle_Type>(random_value)));
}

bool Obstacle_Manager::shift_obstacles() {
	bool obstacle_passed = false;
	for (std::vector<Obstacle>::iterator obs_itr = this->obstacles.begin();
		 obs_itr != this->obstacles.end();) {
		// Check if obstacle shifted out of playing field
		bool valid = obs_itr->shift_location();
		if (!valid) {
			obs_itr = this->obstacles.erase(obs_itr);
			obstacle_passed = true;
		} else {
			++obs_itr;
		}
	}

	return obstacle_passed;
}
