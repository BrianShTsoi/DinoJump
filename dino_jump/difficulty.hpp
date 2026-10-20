#ifndef DIFFICULTY_HPP
#define DIFFICULTY_HPP

inline constexpr int POINTS_PER_LEVEL = 5;

struct Difficulty {
	int obstacle_speed;    // pixels obstacles move per tick
	int obstacle_cooldown; // ticks between obstacle spawns, minus one
	int jump_ticks;        // jump airtime in ticks
};

inline constexpr Difficulty LEVELS[] = {
	{5, 15, 8},  {6, 14, 8},  {7, 13, 8}, {8, 12, 8}, {9, 11, 7}, {10, 10, 7},
	{11, 9, 7},  {12, 8, 6},  {13, 7, 6}, {14, 6, 6}, {14, 5, 5}, {14, 4, 5},
};

inline constexpr int NUM_LEVELS = sizeof(LEVELS) / sizeof(LEVELS[0]);

inline const Difficulty &difficulty_for_score(int score) {
	int level = score / POINTS_PER_LEVEL;
	return LEVELS[level < NUM_LEVELS ? level : NUM_LEVELS - 1];
}

#endif // DIFFICULTY_HPP
