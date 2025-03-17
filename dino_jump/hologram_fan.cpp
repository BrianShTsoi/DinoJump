#include "hologram_fan.hpp"

HologramFan::HologramFan()
	: board1(TLC59116(0b1100000)), board2(TLC59116(0b1100001)), board3(TLC59116(0b1100010)),
	  board4(TLC59116(0b1100011)), board5(TLC59116(0b1100100)), board6(TLC59116(0b1100101)) {
	managers[0].add(&board1);
	managers[0].add(&board2);
	managers[0].add(&board3);
	managers[1].add(&board4);
	managers[1].add(&board5);
	managers[1].add(&board6);
}

void HologramFan::begin() {
	managers[0].begin();
	managers[1].begin();
}

void HologramFan::flash_frame(const BitArray2D<MAX_X>& frame, int manager_num) {
	for (int col = 0; col < MAX_X; col++) {
		managers[manager_num].setPatternAutoIncrement(frame.get_col(col), 255);
	}
	managers[0].setPatternAutoIncrement(0, 255);
	managers[1].setPatternAutoIncrement(0, 255);
}