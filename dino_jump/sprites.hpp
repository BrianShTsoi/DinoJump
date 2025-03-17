#ifndef SPRITES_HPP
#define SPRITES_HPP

#include "bit_array_2d.hpp"
#include "constants.hpp"

#include <array>

// Marked as const instead of constexpr because extern constexpr is not possible
extern const BitArray2D<CACTUS_WIDTH> SMALL_CACTUS_SPRITE_BIT_ARRAY;
extern const BitArray2D<CACTUS_WIDTH> LARGE_CACTUS_SPRITE_BIT_ARRAY;
extern const BitArray2D<BIRD_WIDTH> BIRD_SPRITE_BIT_ARRAY;
extern const BitArray2D<DINO_WIDTH> DINO_JUMP_SPRITE_BIT_ARRAY;
extern const BitArray2D<DINO_WIDTH> DINO_LEFT_SPRITE_BIT_ARRAY;
extern const BitArray2D<DINO_WIDTH> DINO_RIGHT_SPRITE_BIT_ARRAY;
extern const BitArray2D<DINO_WIDTH> DINO_LEFT_DUCK_SPRITE_BIT_ARRAY;
extern const BitArray2D<DINO_WIDTH> DINO_RIGHT_DUCK_SPRITE_BIT_ARRAY;
extern const BitArray2D<DEAD_EYE_DIAMETER> DEAD_EYE_SPRITE_BIT_ARRAY;

#endif // SPRITES_HPP
