#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#include "bit_array_2d.hpp"
#include "game.hpp"
#include "hologram_fan.hpp"
#include "title_frame.hpp"

#define RESET_PIN D7
#define IR_PIN D8
#define BEAM_BREAK_PIN D9
#define JUMP_PIN D10
#define DUCK_PIN D11
#define START_PIN D12

const uint16_t digitToSegment[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
  0x6D, 0x7D, 0x07, 0x7F, 0x6F};

Game game;
HologramFan display;
Adafruit_LEDBackpack score;
bool jumped = false;

void setup() {
  Wire.begin();
  Wire.setClock(1000000);

  pinMode(IR_PIN, INPUT);
  pinMode(BEAM_BREAK_PIN, INPUT_PULLUP);
  pinMode(START_PIN, INPUT_PULLUP);
  pinMode(JUMP_PIN, INPUT);
  pinMode(DUCK_PIN, INPUT);

  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(5);
  digitalWrite(RESET_PIN, HIGH);

  display.begin();

  score.begin(0x70);  
  display_number(score, 0);

  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(JUMP_PIN), handleJump, RISING);
}

void loop() {
  title_loop();
  
  game_loop();
}

// Function to display a number on the hex display
void display_number(Adafruit_LEDBackpack &matrix, int number) {
  // Serial.println(number);
  matrix.clear();
  if (number < 0 || number > 9999)
      return; // Ensure number is within range
  int counts[4] = {0, 1, 3, 4};
  for (int i = 3; i >= 0; i--) {
      int digit = number % 10;
      matrix.displaybuffer[counts[i]] = digitToSegment[digit];
      number /= 10;
  }
  matrix.writeDisplay();
}

void title_loop() {
  while (digitalRead(START_PIN) == HIGH) {
    if (digitalRead(IR_PIN) == LOW) {
      display.flash_frame(TITLE_FRAME, digitalRead(BEAM_BREAK_PIN) == HIGH ? 1 : 0);
    }
    delay(1);
  }
}

void game_loop() {
  while (true) {
    if (digitalRead(IR_PIN) == LOW) {
      // Normally flashing a frame takes ~0.1s
      display.flash_frame(game.get_frame(), digitalRead(BEAM_BREAK_PIN) == HIGH ? 1 : 0);
  
      // Updating game states take ~0.4ms
      if (jumped) {
        game.input(Input_State::JUMP);
        jumped = false;
      } else if (digitalRead(DUCK_PIN) == HIGH) {
        game.input(Input_State::DUCK);
      } else {
        game.input(Input_State::NEUTRAL);
      }
      game.update_obstacles();
      game.update_frame();
      display_number(score, game.get_score());
    }
    delay(1);
  }
}

void handleJump() {
  jumped = true;
}
