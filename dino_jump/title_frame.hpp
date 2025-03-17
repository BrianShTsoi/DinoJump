#ifndef TITLE_FRAME_HPP
#define TITLE_FRAME_HPP

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "bit_array_2d.hpp"

constexpr int TITLE_HEIGHT = 48;
constexpr int TITLE_WIDTH = 100;

using TitleFrame = BitArray2D<TITLE_WIDTH>;

extern const std::array<std::array<uint8_t, TITLE_WIDTH>, TITLE_HEIGHT> TITLE_SPRITE;
extern const TitleFrame TITLE_FRAME; 

#endif // TITLE_FRAME_HPP
