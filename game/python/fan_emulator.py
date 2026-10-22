"""Desktop emulator for the DinoJump hologram fan arcade.

Mirrors the firmware in dino_jump/ (game ESP32) and input/ (input ESP32) closely
enough that difficulty tweaks made here carry over to the real machine:

  * The game advances one tick every time a strip flashes, i.e. twice per fan
    revolution (beam break -> flash strip 1 -> tick -> half period -> flash
    strip 0 -> tick). The fan period is therefore a difficulty knob.
  * The frame shown on a flash is the one computed on the previous tick.
  * Button presses are latched until the next tick consumes them (onReceive
    only ever sets the flags), and JUMP has priority over DUCK.
  * The input ESP32 ignores the buttons for 1 s after START is pressed.

Jump models (setting `jump_model`, try them live from the tuning panel)
  0 original         dino.cpp before levels: instant lift, symmetric arc, slow fall
  1 lift + drop      original arc, but lands instantly at JUMP_DURATION
  2 smooth parabola  gradual rise and fall, tuned by peak height and airtime
                     (the firmware jump: JUMP_PEAK, airtime from the level)

Difficulty levels (setting `levels`, on by default, mirrors difficulty.hpp)
  level = min(start_level + score // points_per_level, last level) picks obstacle
  speed, obstacle cooldown and jump airtime from the LEVELS table in
  difficulty.hpp (override with --level-table). A new airtime only applies while
  the dino is on the ground. With levels off, the manual values are used.

Emulator-only setting `dino_x` moves the dino (defaults to DINO_X in dino.hpp).

Sprites and default constants are parsed directly from the firmware sources so
the emulator stays in sync with them.

Controls
  Enter            START button (title screen -> game)
  Space / W        JUMP button
  S                DUCK button
  Up / Down        select tuning parameter (Tab / Shift+Tab also work)
  Left / Right     decrease / increase selected parameter
  I                invincible mode: collisions are counted instead of fatal
                   (score still counts, handy for playing through the levels)
  R                restart the run
  P                pause,  N  single tick while paused
  C                print the current settings as C++ constants
  Esc / Q          quit (also prints the C++ constants)
"""

import argparse
import dataclasses
import random
import re
import sys
from pathlib import Path

import pygame

FIRMWARE_DIR = Path(__file__).resolve().parents[2] / "dino_jump"


#############################################################################
# FIRMWARE SOURCE PARSING
#############################################################################


def read_constants(*files):
    constants = {}
    for f in files:
        for name, value in re.findall(
            r"constexpr\s+int\s+(\w+)\s*=\s*(-?\d+)\s*;", f.read_text()
        ):
            constants[name] = int(value)
    return constants


def read_game_over(source, constants):
    """Font, text and position of the death screen text drawn by game.cpp."""
    font = {
        ch: [int(v, 0) for v in rows.split(",")]
        for ch, rows in re.findall(r"\{'(.)',\s*\{([^}]*)\}\}", source)
    }
    call = re.search(r'draw_text\("([^"]*)",\s*([^;]*)\);', source)
    if not font or not call:
        return None
    # Evaluate the row expression, e.g. "(21 - FONT_H) / 2"
    expr = call.group(2)
    for name in ("FONT_W", "FONT_H", "FONT_SPACING", "MAX_X", "MAX_Y"):
        expr = re.sub(rf"\b{name}\b", str(constants[name]), expr)
    if not re.fullmatch(r"[\d\s+\-*/()]+", expr):
        raise ValueError(f"can't evaluate draw_text row {call.group(2)!r}")
    top_row = eval(expr.replace("/", "//"))
    text = call.group(1)
    fixed_x = re.search(r"^\s*int x = (\d+);", source, re.MULTILINE)
    if fixed_x:
        x = int(fixed_x.group(1))
    else:
        total_w = len(text) * (constants["FONT_W"] + constants["FONT_SPACING"]) - constants["FONT_SPACING"]
        x = max(0, (constants["MAX_X"] - total_w) // 2)
    return {"font": font, "text": text, "x": x, "top_row": top_row}


def read_level_table(path):
    """Return the {speed, cooldown, jump_ticks} rows of LEVELS[] in difficulty.hpp."""
    match = re.search(r"\bLEVELS\[\]\s*=\s*\{(.*?)\};", path.read_text(), re.DOTALL)
    if not match:
        raise ValueError(f"LEVELS table not found in {path}")
    return tuple(
        tuple(int(v) for v in row)
        for row in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}", match.group(1))
    )


def read_sprite_rows(source, name):
    """Return the rows (lists of 0/1) of a `std::array<std::array<...>> NAME = {{...}};`."""
    match = re.search(rf"\b{name}\s*=?\s*\{{(.*?)\}}\s*;", source, re.DOTALL)
    if not match:
        raise ValueError(f"sprite {name} not found in firmware source")
    rows = re.findall(r"\{([\d,\s]+)\}", match.group(1))
    return [[int(v) for v in row.replace(",", " ").split()] for row in rows]


#############################################################################
# BIT ARRAY (mirrors bit_array_2d.hpp: one int per column, row 0 = LSB)
#############################################################################


class BitArray2D:
    MAX_ROW_SIZE = 64

    def __init__(self, width, rows=None):
        self.width = width
        self.cols = [0] * width
        for r, row in enumerate(rows or []):
            for c, b in enumerate(row):
                self.cols[c] |= b << r

    def get(self, r, c):
        if not (0 <= r < self.MAX_ROW_SIZE and 0 <= c < self.width):
            return 0
        return (self.cols[c] >> r) & 1

    def set(self, r, c, b):
        # Out of range writes are undefined behaviour in the C++ version, ignore them
        if not (0 <= r < self.MAX_ROW_SIZE and 0 <= c < self.width):
            return
        self.cols[c] = (self.cols[c] & ~(1 << r)) | (b << r)

    def rotate(self):
        self.cols = self.cols[1:] + self.cols[:1]

    def clear(self):
        self.cols = [0] * self.width

    def copy(self):
        other = BitArray2D(self.width)
        other.cols = list(self.cols)
        return other

    def union(self, other):
        result = self.copy()
        result.cols = [a | b for a, b in zip(self.cols, other.cols)]
        return result


#############################################################################
# TUNABLE CONFIG
#############################################################################


@dataclasses.dataclass
class Config:
    # Fan revolution period. The game ticks twice per revolution.
    fan_period_ms: int = 250
    # constants.hpp
    low_bird_height: int = 25
    high_bird_height: int = 35
    # Manual difficulty, used when levels are off (one difficulty.hpp LEVELS entry)
    obstacle_cooldown: int = 15
    obstacle_speed: int = 5
    # Original jump (dino.hpp/dino.cpp before the smooth jump replaced it)
    parabolic_motion: int = -20
    init_jump_velocity: int = 12
    jump_duration: int = 8
    # Jump model, see JUMP_MODELS. The firmware uses the smooth jump with
    # JUMP_PEAK from dino.hpp and its airtime from the current level.
    jump_model: int = 2
    smooth_peak: int = 21
    smooth_ticks: int = 8
    # obstacle.cpp, Obstacle_Manager::generate_obstacle (firmware: uniform)
    weight_small_cactus: int = 1
    weight_large_cactus: int = 1
    weight_low_bird: int = 1
    weight_high_bird: int = 1
    # dino.hpp DINO_X, the dino's left column
    dino_x: int = 13
    # Difficulty levels, see LEVEL_TABLE
    levels: int = 1
    points_per_level: int = 5
    start_level: int = 0
    level_table: tuple = ()
    # dino_jump.ino / input.ino (not live-tunable)
    death_screen_ms: int = 5000
    input_start_delay_ms: int = 1000

    def __post_init__(self):
        if not self.level_table:
            self.level_table = LEVEL_TABLE

    def level(self, score):
        return min(self.start_level + score // self.points_per_level, len(self.level_table) - 1)

    def at_score(self, score):
        """Settings in effect at this score, using its difficulty level."""
        if not self.levels:
            return self
        speed, cooldown, airtime = self.level_table[self.level(score)]
        return dataclasses.replace(
            self, obstacle_speed=speed, obstacle_cooldown=cooldown, smooth_ticks=airtime
        )

    @classmethod
    def from_firmware(cls, constants, level_table=None):
        level_table = level_table or LEVEL_TABLE
        speed, cooldown, airtime = level_table[0]
        return cls(
            low_bird_height=constants.get("LOW_BIRD_HEIGHT", 25),
            high_bird_height=constants.get("HIGH_BIRD_HEIGHT", 35),
            obstacle_speed=speed,
            obstacle_cooldown=cooldown,
            smooth_ticks=airtime,
            smooth_peak=constants.get("JUMP_PEAK", 21),
            points_per_level=constants.get("POINTS_PER_LEVEL", 5),
            level_table=level_table,
            death_screen_ms=constants.get("DEATH_SCREEN_PERIOD", 5000),
            dino_x=constants.get("DINO_X", 13),
        )

    @property
    def tick_ms(self):
        return self.fan_period_ms / 2

    def weights(self):
        return [
            self.weight_small_cactus,
            self.weight_large_cactus,
            self.weight_low_bird,
            self.weight_high_bird,
        ]

    def cpp_snippet(self):
        lines = ["// dino.hpp"]
        if self.jump_model == SMOOTH_PARABOLA:
            lines.append(f"inline constexpr int JUMP_PEAK = {self.smooth_peak};")
        else:
            lines += [
                f"inline constexpr int PARABOLIC_MOTION = {self.parabolic_motion};",
                f"inline constexpr int INIT_JUMP_VELOCITY = {self.init_jump_velocity};",
                f"inline constexpr int JUMP_DURATION = {self.jump_duration};",
                "// dino.cpp: replace Dino::jump() (the levels' jump_ticks no longer apply)",
                JUMP_CPP[self.jump_model],
            ]
        table = (
            self.level_table
            if self.levels
            else ((self.obstacle_speed, self.obstacle_cooldown, self.smooth_ticks),)
        )
        rows = ", ".join("{%d, %d, %d}" % row for row in table)
        lines += [
            "// constants.hpp",
            f"inline constexpr int LOW_BIRD_HEIGHT = {self.low_bird_height};",
            f"inline constexpr int HIGH_BIRD_HEIGHT = {self.high_bird_height};",
            "// difficulty.hpp" + ("" if self.levels else " (levels off: a single fixed level)"),
            f"inline constexpr int POINTS_PER_LEVEL = {self.points_per_level};",
            f"inline constexpr Difficulty LEVELS[] = {{{rows}}};",
        ]
        if len(set(self.weights())) > 1:
            lines += [
                "// obstacle.cpp, generate_obstacle: firmware picks uniformly; desired weights",
                "// (SMALL_CACTUS, LARGE_CACTUS, LOW_BIRD, HIGH_BIRD) = "
                + str(tuple(self.weights())),
            ]
        lines.append(
            f"// fan period {self.fan_period_ms} ms -> {1000 / self.tick_ms:.1f} game ticks/s"
        )
        return "\n".join(lines)


ORIGINAL_JUMP, LIFT_AND_DROP, SMOOTH_PARABOLA = range(3)

# (obstacle px/tick, obstacle cooldown, jump airtime) per difficulty level.
# Fallback only: the real table is read from LEVELS in difficulty.hpp.
LEVEL_TABLE = (
    (5, 15, 8),
    (6, 14, 8),
    (7, 13, 8),
    (8, 12, 8),
    (9, 11, 7),
    (10, 10, 7),
    (11, 9, 7),
    (12, 8, 6),
    (13, 7, 6),
    (14, 6, 6),
    (14, 5, 5),
    (14, 4, 5),
)
LEVEL_PARAMS = {"obstacle_speed", "obstacle_cooldown", "smooth_ticks"}
JUMP_MODELS = ["original", "lift + drop", "smooth"]
ORIGINAL_JUMP_PARAMS = {"init_jump_velocity", "parabolic_motion", "jump_duration"}
SMOOTH_JUMP_PARAMS = {"smooth_peak", "smooth_ticks"}

JUMP_CPP = {
    ORIGINAL_JUMP: """void Dino::jump() {
	this->state = Dino_State::JUMPING;

	int curve = (PARABOLIC_MOTION * jump_time * (jump_time - JUMP_DURATION)) /
				(JUMP_DURATION * JUMP_DURATION);
	this->y_position = INIT_JUMP_VELOCITY + curve + GROUND_HEIGHT;
	this->jump_time++;

	if (this->y_position <= GROUND_HEIGHT) {
		this->y_position = GROUND_HEIGHT;
		this->jump_time = 0;
		this->state = Dino_State::RUNNING;
	}
}""",
    LIFT_AND_DROP: """void Dino::jump() {
	this->state = Dino_State::JUMPING;

	int curve = (PARABOLIC_MOTION * jump_time * (jump_time - JUMP_DURATION)) /
				(JUMP_DURATION * JUMP_DURATION);
	this->y_position = INIT_JUMP_VELOCITY + curve + GROUND_HEIGHT;

	// Drop straight back to the ground once the arc returns to lift height,
	// mirroring the instant lift on takeoff
	if (this->jump_time > JUMP_DURATION || this->y_position <= GROUND_HEIGHT) {
		this->y_position = GROUND_HEIGHT;
		this->jump_time = 0;
		this->state = Dino_State::RUNNING;
		return;
	}
	this->jump_time++;
}""",
}

# (attribute, label, step, min, max)
TUNABLES = [
    ("fan_period_ms", "fan period ms", 10, 40, 1000),
    ("obstacle_cooldown", "obstacle cooldown", 1, 0, 100),
    ("obstacle_speed", "obstacle px/tick", 1, 1, 30),
    ("init_jump_velocity", "INIT_JUMP_VELOCITY", 1, 0, 40),
    ("parabolic_motion", "PARABOLIC_MOTION", 2, -100, 0),
    ("jump_duration", "JUMP_DURATION", 1, 1, 30),
    ("jump_model", "jump model", 1, 0, len(JUMP_MODELS) - 1),
    ("smooth_peak", "JUMP_PEAK", 1, 1, 40),
    ("smooth_ticks", "jump airtime", 1, 2, 40),
    ("low_bird_height", "LOW_BIRD_HEIGHT", 1, 0, 39),
    ("high_bird_height", "HIGH_BIRD_HEIGHT", 1, 0, 39),
    ("weight_small_cactus", "weight small cactus", 1, 0, 10),
    ("weight_large_cactus", "weight large cactus", 1, 0, 10),
    ("weight_low_bird", "weight low bird", 1, 0, 10),
    ("weight_high_bird", "weight high bird", 1, 0, 10),
    ("levels", "difficulty levels", 1, 0, 1),
    ("dino_x", "dino x", 1, 0, 60),
    ("points_per_level", "points per level", 1, 1, 100),
    ("start_level", "start level", 1, 0, 99),
]


#############################################################################
# GAME LOGIC (mirrors dino.cpp, obstacle.cpp, game.cpp)
#############################################################################

JUMP, DUCK, NEUTRAL = "JUMP", "DUCK", "NEUTRAL"
RUNNING, JUMPING, DUCKING, DEAD, DEAD_DUCKING = range(5)
SMALL_CACTUS, LARGE_CACTUS, LOW_BIRD, HIGH_BIRD = range(4)


def c_div(a, b):
    """Integer division truncating toward zero, like C++."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


class Firmware:
    """Constants and sprites loaded from the firmware sources."""

    def __init__(self, firmware_dir):
        self.c = read_constants(
            firmware_dir / "constants.hpp",
            firmware_dir / "dino.hpp",
            firmware_dir / "difficulty.hpp",
        )
        self.level_table = read_level_table(firmware_dir / "difficulty.hpp")
        game_src = (firmware_dir / "game.cpp").read_text()
        # FONT_W/H/SPACING are constexprs in game.cpp
        self.c.update(read_constants(firmware_dir / "game.cpp"))
        self.game_over = read_game_over(game_src, self.c)
        # e.g. DEATH_SCREEN_PERIOD, MAX_EXPECTED_PERIOD
        ino = (firmware_dir / "dino_jump.ino").read_text()
        for name, value in re.findall(r"const unsigned long (\w+) = (\d+);", ino):
            self.c[name] = int(value)
        sprites = (firmware_dir / "sprites.cpp").read_text()
        title = (firmware_dir / "title_frame.cpp").read_text()
        max_x = self.c["MAX_X"]

        def sprite(name, width, src=sprites):
            return BitArray2D(width, read_sprite_rows(src, name))

        self.small_cactus = sprite("SMALL_CACTUS_SPRITE", self.c["CACTUS_WIDTH"])
        self.large_cactus = sprite("LARGE_CACTUS_SPRITE", self.c["CACTUS_WIDTH"])
        self.bird = sprite("BIRD_SPRITE", self.c["BIRD_WIDTH"])
        self.dino_jump = sprite("DINO_JUMP_SPRITE", self.c["DINO_WIDTH"])
        self.dino_left = sprite("DINO_LEFT_SPRITE", self.c["DINO_WIDTH"])
        self.dino_right = sprite("DINO_RIGHT_SPRITE", self.c["DINO_WIDTH"])
        self.dino_left_duck = sprite("DINO_LEFT_DUCK_SPRITE", self.c["DINO_WIDTH"])
        self.dino_right_duck = sprite("DINO_RIGHT_DUCK_SPRITE", self.c["DINO_WIDTH"])
        self.dead_eye = sprite("DEAD_EYE_SPRITE", self.c["DEAD_EYE_DIAMETER"])
        self.ground_pattern = sprite("GROUND_PATTERN_SPRITE", max_x)
        # Both strips flash this on the title screen
        self.title = sprite("TITLE_SPRITE", max_x, title)


class Dino:
    def __init__(self, fw, cfg):
        self.fw, self.cfg = fw, cfg
        self.state = RUNNING
        self.step_left = True
        self.y_position = fw.c["GROUND_HEIGHT"]
        self.jump_time = 0

    def try_set_input_state(self, input_state):
        if self.state == JUMPING:
            self.jump()
        elif input_state == DUCKING:
            self.state = DUCKING
            self.step_left = not self.step_left
        elif input_state == JUMPING:
            self.jump()
        else:
            self.state = RUNNING
            self.step_left = not self.step_left

    def jump(self):
        self.state = JUMPING
        if self.cfg.jump_model == SMOOTH_PARABOLA:
            self.smooth_jump()
            return
        cfg, ground = self.cfg, self.fw.c["GROUND_HEIGHT"]
        t, d = self.jump_time, cfg.jump_duration
        curve = c_div(cfg.parabolic_motion * t * (t - d), d * d)
        self.y_position = cfg.init_jump_velocity + curve + ground
        if cfg.jump_model == LIFT_AND_DROP and t > d:
            self.land()
            return
        self.jump_time += 1
        if self.y_position <= ground:
            self.land()

    def smooth_jump(self):
        cfg = self.cfg
        self.jump_time += 1
        t, n = self.jump_time, cfg.smooth_ticks
        if t >= n:
            self.land()
            return
        self.y_position = self.fw.c["GROUND_HEIGHT"] + c_div(4 * cfg.smooth_peak * t * (n - t), n * n)

    def land(self):
        self.y_position = self.fw.c["GROUND_HEIGHT"]
        self.jump_time = 0
        self.state = RUNNING

    def die(self):
        self.state = DEAD_DUCKING if self.state == DUCKING else DEAD


class Obstacle:
    def __init__(self, fw, cfg, kind):
        self.kind = kind
        self.x = fw.c["MAX_X"] - 1
        self.y = {
            SMALL_CACTUS: fw.c["GROUND_HEIGHT"],
            LARGE_CACTUS: fw.c["GROUND_HEIGHT"],
            LOW_BIRD: cfg.low_bird_height,
            HIGH_BIRD: cfg.high_bird_height,
        }[kind]
        self.width = (
            fw.c["CACTUS_WIDTH"] if kind in (SMALL_CACTUS, LARGE_CACTUS) else fw.c["BIRD_WIDTH"]
        )


class Game:
    def __init__(self, fw, cfg, rng):
        self.fw, self.cfg, self.rng = fw, cfg, rng
        # Settings in effect this tick (cfg with the current difficulty level applied)
        self.eff = cfg.at_score(0)
        self.c = fw.c
        self.score = 0
        self.cooldown_count = 0
        self.collision = False
        # Emulator only: collisions are counted instead of ending the game
        self.invincible = False
        self.hit = False
        self.hits = 0
        self.ground_pattern = fw.ground_pattern.copy()
        self.frame = BitArray2D(self.c["MAX_X"])
        self.dino = Dino(fw, self.eff)
        self.obstacles = []

    def tick(self, inp):
        """One firmware game step: input(), update_obstacles(), update_frame()."""
        self.eff = self.cfg.at_score(self.score)
        if self.dino.state == JUMPING:
            # Keep the airtime of a jump in progress, changing it mid-air warps the arc
            self.eff = dataclasses.replace(self.eff, smooth_ticks=self.dino.cfg.smooth_ticks)
        self.dino.cfg = self.eff
        self.dino.try_set_input_state(
            {JUMP: JUMPING, DUCK: DUCKING}.get(inp, RUNNING)
        )
        self.update_obstacles()
        self.update_frame()

    def update_obstacles(self):
        remaining = []
        passed = False
        for obs in self.obstacles:
            obs.x -= self.eff.obstacle_speed
            if obs.x + obs.width >= 0:
                remaining.append(obs)
            else:
                passed = True
        self.obstacles = remaining
        if passed:
            self.score += 1

        if self.cooldown_count <= 0:
            weights = self.eff.weights()
            kind = self.rng.choices(range(4), weights if any(weights) else None)[0]
            self.obstacles.append(Obstacle(self.fw, self.eff, kind))
            self.cooldown_count = self.eff.obstacle_cooldown
        else:
            self.cooldown_count -= 1

    def dino_sprite(self):
        fw, d = self.fw, self.dino
        if d.state == RUNNING:
            return fw.dino_left if d.step_left else fw.dino_right
        if d.state == DUCKING:
            return fw.dino_left_duck if d.step_left else fw.dino_right_duck
        return fw.dino_jump

    def draw_with_collision(self, sprite, height, x, y):
        c = self.c
        draw_y = c["MAX_Y"] - height
        for i in range(sprite.width):
            if i + x >= c["MAX_X"] or i + x <= 0:
                continue
            for j in range(height):
                if sprite.get(j, i):
                    r = draw_y + j - y
                    if self.frame.get(r, i + x):
                        self.collision = True
                    self.frame.set(r, i + x, 1)

    def update_frame(self):
        c, fw, frame = self.c, self.fw, self.frame
        frame.clear()

        # draw_ground
        for col in range(c["MAX_X"]):
            frame.set(c["MAX_Y"] - c["GROUND_HEIGHT"], col, 1)
            for row in range(c["GROUND_PATTERN_HEIGHT"]):
                frame.set(
                    c["MAX_Y"] - row - 1,
                    col,
                    self.ground_pattern.get(c["GROUND_PATTERN_HEIGHT"] - row - 1, col),
                )
        self.ground_pattern.rotate()

        # draw_dino (draws zero pixels too)
        sprite = self.dino_sprite()
        dino_draw_y = c["MAX_Y"] - c["DINO_HEIGHT"]
        for i in range(c["DINO_WIDTH"]):
            for j in range(c["DINO_HEIGHT"]):
                frame.set(
                    dino_draw_y + j - self.dino.y_position, i + self.cfg.dino_x, sprite.get(j, i)
                )

        for obs in self.obstacles:
            if obs.kind == SMALL_CACTUS:
                self.draw_with_collision(fw.small_cactus, c["CACTUS_HEIGHT"], obs.x, obs.y)
            elif obs.kind == LARGE_CACTUS:
                self.draw_with_collision(fw.large_cactus, c["CACTUS_HEIGHT"], obs.x, obs.y)
            else:
                self.draw_with_collision(fw.bird, c["BIRD_HEIGHT"], obs.x, obs.y)

        was_hit, self.hit = self.hit, self.collision and self.invincible
        if self.hit:
            # Count each overlap once even though it spans several ticks
            self.hits += not was_hit
            self.collision = False
        if self.collision or self.hit:
            if self.collision:
                self.dino.die()
            # draw_dino_death
            ducking = self.dino.state in (DUCKING, DEAD_DUCKING)
            eye_y = c["DEAD_DUCK_EYE_Y"] if ducking else c["DEAD_EYE_Y"]
            d = c["DEAD_EYE_DIAMETER"]
            for i in range(d):
                for j in range(d):
                    row = dino_draw_y + eye_y + j - self.dino.y_position
                    # Guard: dino may die mid-jump with the eye above the frame top
                    if 0 <= row < c["MAX_Y"]:
                        # The eye keeps its offset from the dino's left edge
                        eye_x = c["DEAD_EYE_X"] - c["DINO_X"] + self.cfg.dino_x
                        frame.set(row, eye_x + i, fw.dead_eye.get(j, i))
            if self.collision and fw.game_over:
                self.draw_game_over()

    def draw_game_over(self):
        """Mirrors Game::draw_text: overlay lit pixels only, clipped to the display."""
        c, go = self.c, self.fw.game_over
        x = go["x"]
        for ch in go["text"]:
            rows = go["font"].get(ch, go["font"].get(" ", [0] * c["FONT_H"]))
            for r in range(c["FONT_H"]):
                for col in range(c["FONT_W"]):
                    if (rows[r] >> (c["FONT_W"] - 1 - col)) & 1:
                        px, py = x + col, go["top_row"] + r
                        if 0 <= px < c["MAX_X"] and 0 <= py < c["MAX_Y"]:
                            self.frame.set(py, px, 1)
            x += c["FONT_W"] + c["FONT_SPACING"]


def jump_profile(fw, cfg):
    """Heights of the dino for each tick of a jump, using the real jump code."""
    dino = Dino(fw, cfg)
    heights = []
    dino.try_set_input_state(JUMPING)
    while dino.state == JUMPING and len(heights) < 200:
        heights.append(dino.y_position)
        dino.try_set_input_state(RUNNING)
    return heights


#############################################################################
# EMULATOR (state machine, timing, input latching, display)
#############################################################################

TITLE, GAME, DEATH = "TITLE", "GAME", "DEATH"

LED_ON = (255, 60, 40)
LED_OFF = (28, 18, 18)
BG = (8, 8, 10)
PANEL_BG = (20, 20, 26)
TEXT = (210, 210, 220)
DIM = (120, 120, 135)
HILITE = (255, 200, 60)
WARN = (255, 110, 90)
HEX_ON = (255, 40, 40)
LEVEL_COLOR = (110, 190, 255)


class Emulator:
    def __init__(self, fw, cfg, seed, scale):
        self.fw, self.cfg = fw, cfg
        self.rng = random.Random(seed)
        self.scale = scale
        self.max_x, self.max_y = fw.c["MAX_X"], fw.c["MAX_Y"]

        self.panel_h = 370
        self.width = self.max_x * scale
        self.height = self.max_y * scale + self.panel_h
        self.screen = pygame.display.set_mode((self.width, self.height))
        pygame.display.set_caption("DinoJump fan emulator")
        self.font = pygame.font.SysFont("monospace", 15)
        self.big_font = pygame.font.SysFont("monospace", 40, bold=True)

        self.selected = 0
        self.paused = False
        self.invincible = False
        self.highscore = 0
        self.shown_score = 0
        self.led_cache_key = None
        self.led_surface = None
        self.to_title()

    # --- state transitions ---------------------------------------------------

    def to_title(self):
        self.state = TITLE
        self.game = None
        self.shown = self.fw.title

    def start_game(self, now):
        self.state = GAME
        self.game = Game(self.fw, self.cfg, self.rng)
        self.game.invincible = self.invincible
        # Runs that used invincibility don't count towards the high score
        self.run_was_invincible = self.invincible
        self.shown = self.game.frame.copy()
        self.next_tick = now
        self.inputs_enabled_at = now + self.cfg.input_start_delay_ms
        self.jump_latch = self.duck_latch = False
        self.shown_score = 0

    def to_death(self, now):
        self.state = DEATH
        self.death_end = now + self.cfg.death_screen_ms
        self.shown = self.game.frame.copy()
        if not self.run_was_invincible:
            self.highscore = max(self.highscore, min(self.game.score, 9999))

    # --- game loop -----------------------------------------------------------

    def game_tick(self):
        game = self.game
        # The flash at this instant shows the frame computed on the previous tick
        self.shown = game.frame.copy()
        inp = JUMP if self.jump_latch else DUCK if self.duck_latch else NEUTRAL
        self.jump_latch = self.duck_latch = False
        game.tick(inp)
        self.shown_score = game.score

    def poll_buttons(self, now):
        """Input ESP32 sends button state every 20 ms; game ESP32 latches trues."""
        if self.state != GAME or now < self.inputs_enabled_at:
            return
        keys = pygame.key.get_pressed()
        if keys[pygame.K_SPACE] or keys[pygame.K_w]:
            self.jump_latch = True
        if keys[pygame.K_s]:
            self.duck_latch = True

    def handle_key(self, key, mods, now):
        if key in (pygame.K_ESCAPE, pygame.K_q):
            return False
        if key == pygame.K_RETURN and self.state == TITLE:
            self.start_game(now)
        elif key == pygame.K_r:
            self.start_game(now)
        elif key == pygame.K_p:
            self.paused = not self.paused
            if not self.paused and self.state == GAME:
                self.next_tick = now
        elif key == pygame.K_n and self.paused and self.state == GAME:
            self.step_once(now)
        elif key == pygame.K_c:
            print(self.cfg.cpp_snippet(), flush=True)
        elif key == pygame.K_i:
            self.invincible = not self.invincible
            if self.game:
                self.game.invincible = self.invincible
                self.run_was_invincible |= self.invincible
        elif key in (pygame.K_TAB, pygame.K_UP, pygame.K_DOWN):
            back = key == pygame.K_UP or (key == pygame.K_TAB and mods & pygame.KMOD_SHIFT)
            self.selected = (self.selected + (-1 if back else 1)) % len(TUNABLES)
        elif key in (pygame.K_LEFT, pygame.K_RIGHT):
            attr, _, step, lo, hi = TUNABLES[self.selected]
            delta = step if key == pygame.K_RIGHT else -step
            setattr(self.cfg, attr, max(lo, min(hi, getattr(self.cfg, attr) + delta)))
        # Quick taps shorter than a frame still register, like the 20 ms poll + latch
        elif self.state == GAME and now >= self.inputs_enabled_at:
            if key in (pygame.K_SPACE, pygame.K_w):
                self.jump_latch = True
            elif key == pygame.K_s:
                self.duck_latch = True
        return True

    def step_once(self, now):
        self.game_tick()
        if self.game.collision:
            self.to_death(now)

    def run(self):
        clock = pygame.time.Clock()
        running = True
        while running:
            now = pygame.time.get_ticks()
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    running = self.handle_key(event.key, event.mod, now) and running

            self.poll_buttons(now)

            if self.state == GAME and not self.paused:
                while now >= self.next_tick and self.state == GAME:
                    self.next_tick += self.cfg.tick_ms
                    self.step_once(now)
            elif self.state == DEATH and now >= self.death_end:
                self.to_title()

            self.draw(now)
            clock.tick(250)
        print(self.cfg.cpp_snippet())

    # --- rendering -----------------------------------------------------------

    def led_surface_for(self, frame):
        key = tuple(frame.cols)
        if key != self.led_cache_key:
            s = self.scale
            surf = pygame.Surface((self.max_x * s, self.max_y * s))
            surf.fill(BG)
            radius = max(1, s // 2 - 1)
            for c in range(self.max_x):
                col = frame.cols[c]
                for r in range(self.max_y):
                    color = LED_ON if (col >> r) & 1 else LED_OFF
                    pygame.draw.circle(surf, color, (c * s + s // 2, r * s + s // 2), radius)
            self.led_cache_key, self.led_surface = key, surf
        return self.led_surface

    def text(self, msg, pos, color=TEXT, font=None):
        self.screen.blit((font or self.font).render(msg, True, color), pos)

    def draw(self, now):
        self.screen.fill(BG)
        self.screen.blit(self.led_surface_for(self.shown), (0, 0))

        top = self.max_y * self.scale
        pygame.draw.rect(self.screen, PANEL_BG, (0, top, self.width, self.panel_h))

        # Hex displays on the input box
        self.text("SCORE", (20, top + 12), DIM)
        self.text(f"{self.shown_score:04d}", (20, top + 30), HEX_ON, self.big_font)
        self.text("HIGH", (20, top + 85), DIM)
        self.text(f"{self.highscore:04d}", (20, top + 103), HEX_ON, self.big_font)

        if self.state == TITLE:
            status = "TITLE - press Enter (START)"
        elif self.state == DEATH:
            status = f"DEATH - back to title in {max(0, self.death_end - now) / 1000:.1f}s"
        else:
            status = "GAME" + (" - PAUSED (N = step)" if self.paused else "")
            if now < self.inputs_enabled_at:
                status += " - buttons ignored for 1s after START"
        if self.cfg.levels:
            level = self.cfg.level(self.game.score if self.game else 0)
            status += f" - level {level}/{len(self.cfg.level_table) - 1}"
        if self.invincible:
            status += " - INVINCIBLE"
        if self.game and (self.invincible or self.game.hits):
            status += f" - {self.game.hits} hits"
            if self.game.hit:
                status += " - HIT!"
        bottom = top + self.panel_h
        self.text(status, (20, bottom - 66), HILITE)
        self.text("Enter start   Space/W jump   S duck   I invincible   R restart   P pause   N step", (20, bottom - 44), DIM)
        self.text("Up/Down select setting   Left/Right adjust   C print C++ constants   Q quit", (20, bottom - 24), DIM)

        # Tuning panel, two columns
        x0 = 230
        col_w = (self.width - x0) // 2
        per_col = (len(TUNABLES) + 1) // 2
        active = self.game.eff if self.game else self.cfg.at_score(0)
        for idx, (attr, label, *_rest) in enumerate(TUNABLES):
            x = x0 + (idx // per_col) * col_w
            y = top + 12 + (idx % per_col) * 18
            sel = idx == self.selected
            from_level = self.cfg.levels and attr in LEVEL_PARAMS
            # Level-controlled rows show the value currently in effect
            value = getattr(active if from_level else self.cfg, attr)
            if attr == "jump_model":
                shown = JUMP_MODELS[value]
            elif attr == "levels":
                shown = "on" if value else "off"
            else:
                shown = value
            unused = (
                attr in ORIGINAL_JUMP_PARAMS and self.cfg.jump_model == SMOOTH_PARABOLA
            ) or (attr in SMOOTH_JUMP_PARAMS and self.cfg.jump_model != SMOOTH_PARABOLA)
            self.text(
                f"{'>' if sel else ' '} {label:<19}{shown:>9}",
                (x, y),
                HILITE if sel else DIM if unused else LEVEL_COLOR if from_level else TEXT,
            )

        # Derived difficulty stats
        cfg, c = active, self.fw.c
        profile = jump_profile(self.fw, cfg)
        airtime = len(profile) + 1  # +1 for the landing tick
        peak = max(profile, default=c["GROUND_HEIGHT"])
        gap_ticks = cfg.obstacle_cooldown + 1
        tick_s = cfg.tick_ms / 1000
        travel_ticks = (c["MAX_X"] - 1 - (cfg.dino_x + c["DINO_WIDTH"])) / cfg.obstacle_speed
        stats = [
            f"{1 / tick_s:.1f} ticks/s; jump {airtime} ticks ({airtime * tick_s:.2f}s), "
            f"covers {airtime * cfg.obstacle_speed}px, peak y={peak}",
            f"obstacle every {gap_ticks} ticks ({gap_ticks * tick_s:.2f}s, "
            f"{gap_ticks * cfg.obstacle_speed}px apart); "
            f"reaction window {travel_ticks * tick_s:.2f}s",
        ]
        y = top + 12 + per_col * 18 + 6
        for line in stats:
            self.text(line, (x0, y), DIM)
            y += 18
        warnings = []
        dino_top_row = c["MAX_Y"] - c["DINO_HEIGHT"] - peak
        if dino_top_row < 0:
            warnings.append(f"dino leaves the display at the top ({-dino_top_row} rows)")
        bird_w = c["BIRD_WIDTH"]
        if gap_ticks * cfg.obstacle_speed < bird_w:
            warnings.append("obstacles can overlap and collide with each other (instant death)")
        max_period = c.get("MAX_EXPECTED_PERIOD", 350)
        if cfg.fan_period_ms >= max_period:
            warnings.append(f"period >= MAX_EXPECTED_PERIOD ({max_period}ms): 2nd strip skips flashing")
        if self.cfg.levels:
            warnings.append("blue settings are set by the level; turn levels off to tune them")
        for line in warnings:
            self.text("! " + line, (x0, y), LEVEL_COLOR if line.startswith("blue") else WARN)
            y += 18

        pygame.display.flip()


#############################################################################
# ENTRY POINT
#############################################################################


def parse_args(cfg):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--firmware-dir", type=Path, default=FIRMWARE_DIR)
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--scale", type=int, default=8, help="screen pixels per LED")
    parser.add_argument(
        "--level-table",
        metavar="S,C,A;S,C,A;...",
        help="difficulty levels as obstacle px/tick, obstacle cooldown, jump airtime",
    )
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="override a setting, e.g. --set fan_period_ms=200 --set obstacle_cooldown=12. "
        "Names: " + ", ".join(f.name for f in dataclasses.fields(cfg) if f.type in (int, "int")),
    )
    return parser.parse_args()


def main():
    args = parse_args(Config())
    fw = Firmware(args.firmware_dir)
    cfg = Config.from_firmware(fw.c, fw.level_table)
    valid = {f.name for f in dataclasses.fields(cfg) if f.type in (int, "int")}
    for item in args.set:
        name, _, value = item.partition("=")
        if name not in valid:
            sys.exit(f"unknown setting {name!r}; valid: {', '.join(sorted(valid))}")
        setattr(cfg, name, int(value))
    if args.level_table:
        cfg.level_table = tuple(
            tuple(int(v) for v in level.split(",")) for level in args.level_table.split(";")
        )
        if any(len(level) != 3 for level in cfg.level_table):
            sys.exit("--level-table needs 3 values per level: speed,cooldown,airtime")
    cfg.start_level = min(cfg.start_level, len(cfg.level_table) - 1)

    pygame.init()
    Emulator(fw, cfg, args.seed, args.scale).run()
    pygame.quit()


if __name__ == "__main__":
    main()
