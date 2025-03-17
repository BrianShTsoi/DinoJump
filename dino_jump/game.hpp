#ifndef GAME_HPP
#define GAME_HPP

#include "bit_array_2d.hpp"
#include "constants.hpp"
#include "dino.hpp"
#include "obstacle.hpp"

#include <array>

using Frame = BitArray2D<MAX_X>;

inline constexpr int DINO_DRAW_Y = MAX_Y - DINO_HEIGHT;
inline constexpr int CACTUS_DRAW_Y = MAX_Y - CACTUS_HEIGHT;
inline constexpr int BIRD_DRAW_Y = MAX_Y - BIRD_HEIGHT;

enum Input_State { JUMP, DUCK, NEUTRAL };

class Game {
  public:
	Game();
	int get_score();
	bool get_collision();
	void input(Input_State input);
	void update_obstacles();
	void update_frame();
	const Frame &get_frame() const;

  private:
	const BitArray2D<DINO_WIDTH> &fetch_dino_sprite();
	void draw_dino();
	void draw_dino_death();
	void draw_small_cactus_with_collision(std::array<int, 2> location);
	void draw_large_cactus_with_collision(std::array<int, 2> location);
	void draw_bird_with_collision(std::array<int, 2> location);
	int score;
	int cooldown_count;
	bool collision;
	Frame frame;
	Dino dino;
	Obstacle_Manager obs_manager;
};

#endif // GAME_HPP