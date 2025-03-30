#include "bit_array_2d.hpp"
#include "game.hpp"
#include "hologram_fan.hpp"
#include "title_frame.hpp"

#define FAN_PIN D6
#define RESET_PIN D7
#define IR_PIN D8
#define BEAM_BREAK_PIN D9
#define JUMP_PIN D10
#define DUCK_PIN D11
#define START_PIN D12

Game game;
HologramFan display;
bool jumped = false;

void setup() {
  Wire.begin();
  Wire.setClock(800000);

  pinMode(IR_PIN, INPUT);
  pinMode(BEAM_BREAK_PIN, INPUT_PULLUP);
  pinMode(START_PIN, INPUT_PULLUP);
  pinMode(JUMP_PIN, INPUT);
  pinMode(DUCK_PIN, INPUT);

  pinMode(FAN_PIN, OUTPUT);

  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(5);
  digitalWrite(RESET_PIN, HIGH);

  display.begin();

  attachInterrupt(digitalPinToInterrupt(JUMP_PIN), handleJump, RISING);
}

void loop() {
  title_loop();
  
  game_loop();
}

void title_loop() {
  while(digitalRead(START_PIN) == HIGH) {
    delay(1);
  }
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(FAN_PIN, HIGH);
  delay(1000);
  
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
    }
    delay(1);
  }
}

void handleJump() {
  jumped = true;
}
