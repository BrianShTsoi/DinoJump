#ifndef DINO_HPP
#define DINO_HPP

#include "constants.hpp"

// Jump height in pixels above the ground; 21 puts the top of the dino on the
// top LED row, any higher and it is drawn off the display
inline constexpr int JUMP_PEAK = 21;

inline constexpr int DINO_X = 13;
inline constexpr int DEAD_EYE_X = 28;
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
	void set_jump_ticks(int ticks);
	void die();

  private:
	void neutral();
	void duck();
	void jump();
	void toggle_step();
	Dino_State state;
	Step_State step;
	int y_position;
	int jump_ticks;
	int jump_time;
};

#endif // DINO_HPP
