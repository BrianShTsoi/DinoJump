#ifndef OBSTACLE_HPP
#define OBSTACLE_HPP

#include "constants.hpp"

#include <array>
#include <cstdlib>
#include <ctime>
#include <vector>

enum Obstacle_Type { SMALL_CACTUS, LARGE_CACTUS, LOW_BIRD, HIGH_BIRD };

class Obstacle {
  public:
	Obstacle(Obstacle_Type type);
	Obstacle_Type get_type();
	std::array<int, 2> get_location();
	bool shift_location();

  private:
	int x_position;
	int y_position;
	Obstacle_Type type;
};

class Obstacle_Manager {
  public:
	Obstacle_Manager();
	std::vector<Obstacle> fetch_obstacles();
	void generate_obstacle();
	bool shift_obstacles();

  private:
	std::vector<Obstacle> obstacles;
};

#endif // OBSTACLE_HPP
