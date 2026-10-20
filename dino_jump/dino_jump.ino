#include "bit_array_2d.hpp"
#include "game.hpp"
#include "hologram_fan.hpp"
#include "title_frame.hpp"

#include <esp_now.h>
#include <WiFi.h>

/* Display Pins */
#define RESET_PIN D7
#define BEAM_BREAK_PIN D9

#define LED_BUILTIN 13

enum State {
  IDLE,
  TITLE,
  GAME,
  DEATH,
};

const unsigned long MAX_EXPECTED_PERIOD = 350;
const unsigned long DEATH_SCREEN_PERIOD = 3500;

typedef struct {
  bool jump_timeout;
  bool duck_advance;
} input_message;

typedef struct {
  int score;
  bool death;
} game_message;

input_message incoming;
game_message outgoing;

// uint8_t inputMAC[] = {0x74, 0x4D, 0xBD, 0xA2, 0x0D, 0x38};
uint8_t inputMAC[] = {0xE4, 0xB0, 0x63, 0xAD, 0x8A, 0x28};

Game game;
HologramFan display;
State currentState = IDLE;
bool advance = false;
bool jumped = false;
bool ducked = false;
bool beam_break_rising = false;

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  input_message msg;
  memcpy(&msg, data, sizeof(msg));
  // Latch: only set to true, never clear — let consumers clear after reading
  if (msg.jump_timeout) incoming.jump_timeout = true;
  if (msg.duck_advance) incoming.duck_advance = true;

  // digitalWrite(LED_BUILTIN, HIGH);
  // Serial.print("[onReceive] ");
  // Serial.print(incoming.jump_timeout);
  // Serial.print(" ");
  // Serial.println(incoming.duck_advance);
  // digitalWrite(LED_BUILTIN, LOW);
}

void onSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("[onSend] ");
  // Serial.print("Message: ");
  // Serial.print(outgoing.score);
  // Serial.print(" Send status: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(800000);

  // pin initialization
  pinMode(BEAM_BREAK_PIN, INPUT_PULLUP);

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(5);
  digitalWrite(RESET_PIN, HIGH);

  display.begin();

  attachInterrupt(digitalPinToInterrupt(BEAM_BREAK_PIN), handle_beam_break_rising, RISING);

  // ESP-NOW establishment
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Delay to ensure peer has initialzied ESP-NOW
  delay(1000);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, inputMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(inputMAC)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
    }
  }

  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSend);

  incoming.jump_timeout = false;
  incoming.duck_advance = false;
}

void loop() {
 Serial.println("loop");
 switch (currentState) {
   case IDLE:
     Serial.println("idle");
     idle_loop();
     break;
   case TITLE: 
     Serial.println("title");
     title_loop();
     break;
   case GAME:
     Serial.println("game");
     game_loop();
     break;
   case DEATH:
     Serial.println("death");
     death_loop();
     break;
 }
}

void idle_loop() {
  while (!incoming.duck_advance) {
    Serial.println("not advancing, still idle");
  }
  incoming.duck_advance = false;

  delay(5);

  currentState = TITLE;
}

void title_loop() {
  beam_break_rising = false;

  bool timedout = false;

  unsigned long prev = 0;
  unsigned long curr = millis();
  unsigned long time_passed = 0;
  while (!incoming.duck_advance && !incoming.jump_timeout) {
    if (beam_break_rising) {
      bool first_time = prev == 0;
      curr = millis();
      time_passed = curr - prev;
      prev = curr;

      // Skip first time as time_passed is not a valid value
      if (first_time) {
        continue;
      }
      display.flash_frame(LINE_TITLE_FRAME, 1);

      if (time_passed < MAX_EXPECTED_PERIOD) {
        unsigned long next_flash_time = curr + time_passed / 2;
        while (millis() < next_flash_time);
        display.flash_frame(TITLE_LINE_FRAME, 0);
      }

      beam_break_rising = false;
    }

    delay(1);

    if (incoming.jump_timeout) {
      timedout = true;
    }
  }

  if (timedout) {
    incoming.jump_timeout = false;
    incoming.duck_advance = false;
    currentState = IDLE;
  } else {
    incoming.jump_timeout = false;
    incoming.duck_advance = false;
    currentState = GAME;
  }

  delay(5);
}

void game_loop() {
  beam_break_rising = false;

  Game curr_game;

  unsigned long prev = 0;
  unsigned long curr = millis();
  unsigned long time_passed = 0;
  int prev_score = -1;
  while (!curr_game.get_collision()) {
    int curr_score = curr_game.get_score();
    if (curr_score != prev_score) {
      outgoing.score = curr_score;
      outgoing.death = false;
      esp_now_send(inputMAC, (uint8_t *)&outgoing, sizeof(outgoing));
      prev_score = curr_score;
    }
    // Serial.println("Game loop");
    if (beam_break_rising) {
      bool first_time = prev == 0;
      curr = millis();
      time_passed = curr - prev;
      prev = curr;

      // Skip first time as time_passed is not a valid value
      if (first_time) {
        continue;
      }
      display.flash_frame(curr_game.get_frame(), 1);
  
      if (incoming.jump_timeout) {
        curr_game.input(Input_State::JUMP);
      } else if (incoming.duck_advance) {
        curr_game.input(Input_State::DUCK);
      } else {
        curr_game.input(Input_State::NEUTRAL);
      }
      incoming.jump_timeout = false;
      incoming.duck_advance = false;
      curr_game.update_obstacles();
      curr_game.update_frame();


      if (time_passed < MAX_EXPECTED_PERIOD) {
        unsigned long next_flash_time = curr + time_passed / 2;
        while (millis() < next_flash_time);
        display.flash_frame(curr_game.get_frame(), 0);

      }

      // Update game regardless of flashing as we don't want to slow down the game
      if (incoming.jump_timeout) {
        curr_game.input(Input_State::JUMP);
      } else if (incoming.duck_advance) {
        curr_game.input(Input_State::DUCK);
      } else {
        curr_game.input(Input_State::NEUTRAL);
      }
      incoming.jump_timeout = false;
      incoming.duck_advance = false;
      curr_game.update_obstacles();
      curr_game.update_frame();

      beam_break_rising = false;
    }
    delay(1);
  }

  Serial.println("Collision occured");
  game = curr_game;
  currentState = DEATH;
}

void death_loop() {
  beam_break_rising = false;

  unsigned long prev = 0;
  unsigned long curr = millis();
  unsigned long time_passed = 0;
  unsigned long death_screen_start = millis();
  while (millis() - death_screen_start < DEATH_SCREEN_PERIOD) {
    if (beam_break_rising) {
      bool first_time = prev == 0;
      curr = millis();
      time_passed = curr - prev;
      prev = curr;

      // Skip first time as time_passed is not a valid value
      if (first_time) {
        continue;
      }
      display.flash_frame(game.get_frame(), 1);

      if (time_passed < MAX_EXPECTED_PERIOD) {
        unsigned long next_flash_time = curr + time_passed / 2;
        while (millis() < next_flash_time);
        display.flash_frame(game.get_frame(), 0);

      }

      beam_break_rising = false;
    }
  }

  // Communicate game over to Base
  outgoing.death = true;
  esp_now_send(inputMAC, (uint8_t *)&outgoing, sizeof(outgoing));

  incoming.duck_advance = false;
  incoming.jump_timeout = false;

  currentState = TITLE;
}

void handle_state_input() {
  advance = true;
}

void handle_jump() {
  jumped = true;
}

void handle_beam_break_rising() {
  beam_break_rising = true;
}
