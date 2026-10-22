#ifndef TITLE_FRAME_HPP
#define TITLE_FRAME_HPP

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "bit_array_2d.hpp"
#include "constants.hpp"

constexpr int TITLE_HEIGHT = 48;

using TitleFrame = BitArray2D<MAX_X>;

extern const std::array<std::array<uint8_t, MAX_X>, TITLE_HEIGHT> TITLE_SPRITE;
extern const TitleFrame TITLE_FRAME; 

#endif // TITLE_FRAME_HPP
