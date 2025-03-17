#ifndef HOLOGRAM_FAN_HPP
#define HOLOGRAM_FAN_HPP

#include "bit_array_2d.hpp"
#include "constants.hpp"

#include <TLC59116.h>
#include <TLC59116Manager.h>

using Frame = BitArray2D<MAX_X>;

inline constexpr int MAX_BRIGHTNESS = 255;

class HologramFan {
  public:
	HologramFan();
	void begin();
	void flash_frame(const BitArray2D<MAX_X>& frame, int manager_num);

  private:
	TLC59116 board1;
	TLC59116 board2;
	TLC59116 board3;
	TLC59116 board4;
	TLC59116 board5;
	TLC59116 board6;
	TLC59116Manager managers[2];
};

#endif // HOLOGRAM_FAN_HPP