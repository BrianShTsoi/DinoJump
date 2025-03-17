#ifndef DINO_HPP
#define DINO_HPP

#include "constants.hpp"

inline constexpr int PARABOLIC_MOTION = -20;
inline constexpr int INIT_JUMP_VELOCITY = 12;
inline constexpr int JUMP_DURATION = 25;

inline constexpr int DEAD_EYE_X = 15;
inline constexpr int DEAD_EYE_Y = 1;
inline constexpr int DEAD_DUCK_EYE_Y = 10;

enum Dino_State { RUNNING, JUMPING, DUCKING, DEAD, DEAD_DUCKING };
enum Step_State { LEFT, RIGHT };

class Dino {
  public:
	Dino();
	Dino_State get_state();
	Step_State get_step();
	int get_y_position();
	void try_set_input_state(Dino_State input_state);
	void die();

  private:
	void neutral();
	void duck();
	void jump();
	void toggle_step();
	Dino_State state;
	Step_State step;
	int y_position;
	int jump_velocity;
	int jump_time;
};

#endif // DINO_HPP
