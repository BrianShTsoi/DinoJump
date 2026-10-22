#include "game.hpp"
#include "bit_array_2d.hpp"
#include "constants.hpp"
#include "difficulty.hpp"
#include "dino.hpp"
#include "sprites.hpp"

#include <cstring>

namespace {
// 5x7 bitmap font, row 0 = top. Low 5 bits of each byte; bit 4 = leftmost col.
// Only glyphs used by the death screen ("GAME OVER") are defined.
constexpr int FONT_W = 5;
constexpr int FONT_H = 7;
constexpr int FONT_SPACING = 1;

struct Glyph {
  char c;
  uint8_t rows[FONT_H];
};

const Glyph FONT[] = {
  {' ', {0, 0, 0, 0, 0, 0, 0}},
  {'G', {0b01111, 0b10000, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
  {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
  {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
  {'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
  {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
  {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
  {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
};

const uint8_t *glyph_for(char c) {
  for (const Glyph &g : FONT) {
    if (g.c == c) {
      return g.rows;
    }
  }
  return FONT[0].rows; // fall back to blank glyph
}
} // namespace

Game::Game()
  : score(0), cooldown_count(0), collision(false), ground_pattern(GROUND_PATTERN_ARRAY), frame(), dino(Dino()),
    obs_manager(Obstacle_Manager()) {
  // Set seed for random number generator
  std::srand(static_cast<unsigned int>(std::time(0)));
}

int Game::get_score() { return this->score; }

bool Game::get_collision() { return this->collision; }

void Game::input(Input_State input) {
  Dino_State input_state;
  switch (input) {
    case DUCK:
      input_state = Dino_State::DUCKING;
      break;
    case JUMP:
      input_state = Dino_State::JUMPING;
      break;
    default:
      input_state = Dino_State::RUNNING;
  }
  this->dino.set_jump_ticks(difficulty_for_score(this->score).jump_ticks);
  this->dino.try_set_input_state(input_state);
}

void Game::update_obstacles() {
  const Difficulty &difficulty = difficulty_for_score(this->score);

  // Shift obstacles
  if (this->obs_manager.shift_obstacles(difficulty.obstacle_speed)) {
    this->score++;
  }

  // Handle obstacle generation
  if (cooldown_count == 0) {
    this->obs_manager.generate_obstacle();
    cooldown_count = difficulty.obstacle_cooldown;
  } else {
    cooldown_count--;
  }
}

const BitArray2D<DINO_WIDTH> &Game::fetch_dino_sprite() {
  switch (this->dino.get_state()) {
    case Dino_State::RUNNING:
      return this->dino.get_step() == Step_State::LEFT ? DINO_LEFT_SPRITE_BIT_ARRAY
                               : DINO_RIGHT_SPRITE_BIT_ARRAY;
    case Dino_State::DUCKING:
      return this->dino.get_step() == Step_State::LEFT ? DINO_LEFT_DUCK_SPRITE_BIT_ARRAY
                               : DINO_RIGHT_DUCK_SPRITE_BIT_ARRAY;
    case Dino_State::JUMPING:
    default:
      return DINO_JUMP_SPRITE_BIT_ARRAY;
  }
}

void Game::draw_dino() {
  const BitArray2D<DINO_WIDTH> &sprite = fetch_dino_sprite();

  int y_position = dino.get_y_position();
  for (int i = 0; i < DINO_WIDTH; ++i) {
    for (int j = 0; j < DINO_HEIGHT; ++j) {
      frame.set(DINO_DRAW_Y + j - y_position, i + DINO_X, sprite.get(j, i));
    }
  }
}

void Game::draw_dino_death() {
  const BitArray2D<DEAD_EYE_DIAMETER> &sprite = DEAD_EYE_SPRITE_BIT_ARRAY;

  // Only modification for dino sprite when dead is the eye
  int dead_eye_y = dino.get_state() == DEAD ? DEAD_EYE_Y : DEAD_DUCK_EYE_Y;
  int y_position = dino.get_y_position();
  for (int i = 0; i < DEAD_EYE_DIAMETER; ++i) {
    for (int j = 0; j < DEAD_EYE_DIAMETER; ++j) {
      int row = DINO_DRAW_Y + dead_eye_y + j - y_position;
      // Guard: dino may die mid-jump with the eye above the frame top
      if (row < 0 || row >= MAX_Y) {
        continue;
      }
      frame.set(row, DEAD_EYE_X + i, sprite.get(j, i));
    }
  }
}

void Game::draw_small_cactus_with_collision(std::array<int, 2> location) {
  const auto &bit_array = SMALL_CACTUS_SPRITE_BIT_ARRAY;

  for (int i = 0; i < CACTUS_WIDTH; ++i) {
    if (i + location[0] >= MAX_X || i + location[0] <= 0) {
      continue;
    }

    for (int j = 0; j < CACTUS_HEIGHT; ++j) {
      if (bit_array.get(j, i)) {
        if (frame.get(CACTUS_DRAW_Y + j - location[1], i + location[0]) == 1) {
          this->collision = true;
        }
        frame.set(CACTUS_DRAW_Y + j - location[1], i + location[0], bit_array.get(j, i));
      }
    }
  }
}

void Game::draw_large_cactus_with_collision(std::array<int, 2> location) {
  const auto &bit_array = LARGE_CACTUS_SPRITE_BIT_ARRAY;

  for (int i = 0; i < CACTUS_WIDTH; ++i) {
    if (i + location[0] >= MAX_X || i + location[0] <= 0) {
      continue;
    }

    for (int j = 0; j < CACTUS_HEIGHT; ++j) {
      if (bit_array.get(j, i)) {
        if (frame.get(CACTUS_DRAW_Y + j - location[1], i + location[0]) == 1) {
          this->collision = true;
        }
        frame.set(CACTUS_DRAW_Y + j - location[1], i + location[0], bit_array.get(j, i));
      }
    }
  }
}

void Game::draw_bird_with_collision(std::array<int, 2> location) {
  const auto &bit_array = BIRD_SPRITE_BIT_ARRAY;

  for (int i = 0; i < BIRD_WIDTH; ++i) {
    if (i + location[0] >= MAX_X || i + location[0] <= 0) {
      continue;
    }

    for (int j = 0; j < BIRD_HEIGHT; ++j) {
      if (bit_array.get(j, i)) {
        if (frame.get(BIRD_DRAW_Y + j - location[1], i + location[0]) == 1) {
          this->collision = true;
        }
        frame.set(BIRD_DRAW_Y + j - location[1], i + location[0], bit_array.get(j, i));
      }
    }
  }
}

void Game::draw_ground() {
  for (int col = 0; col < MAX_X; ++col) {
    frame.set(MAX_Y - GROUND_HEIGHT, col, 1);
    for (int row = 0; row < GROUND_PATTERN_HEIGHT; ++row) {
      frame.set(MAX_Y - row - 1, col, ground_pattern.get(GROUND_PATTERN_HEIGHT - row - 1, col));
    }
  }

  ground_pattern.rotate();
}

void Game::draw_text(const char *text, int top_row) {
  int len = static_cast<int>(std::strlen(text));
  // Centered for "GAME OVER": (MAX_X - 53) / 2
  int x = 33;

  for (int i = 0; i < len; ++i) {
    const uint8_t *rows = glyph_for(text[i]);
    for (int r = 0; r < FONT_H; ++r) {
      for (int col = 0; col < FONT_W; ++col) {
        // Overlay only: set lit pixels, leave background untouched
        if ((rows[r] >> (FONT_W - 1 - col)) & 1) {
          int px = x + col;
          int py = top_row + r;
          if (px >= 0 && px < MAX_X && py >= 0 && py < MAX_Y) {
            frame.set(py, px, 1);
          }
        }
      }
    }
    x += FONT_W + FONT_SPACING;
  }
}

void Game::draw_game_over() {
  // Centered in the open "sky" band (rows 0-20); dino occupies rows 21-40
  draw_text("GAME OVER", (21 - FONT_H) / 2);
}

void Game::update_frame() {
  frame.clear();

  draw_ground();
  draw_dino();

  std::vector<Obstacle> obstacles = obs_manager.fetch_obstacles();
  for (auto obs_itr = obstacles.begin(); obs_itr != obstacles.end(); ++obs_itr) {
    switch (obs_itr->get_type()) {
      case Obstacle_Type::SMALL_CACTUS:
        draw_small_cactus_with_collision(obs_itr->get_location());
        break;
      case Obstacle_Type::LARGE_CACTUS:
        draw_large_cactus_with_collision(obs_itr->get_location());
        break;
      case Obstacle_Type::LOW_BIRD:
      case Obstacle_Type::HIGH_BIRD:
        draw_bird_with_collision(obs_itr->get_location());
    }
  }

  if (collision) {
    dino.die();
    draw_dino_death();
    draw_game_over();
  }
}

const Frame &Game::get_frame() const { return this->frame; }
