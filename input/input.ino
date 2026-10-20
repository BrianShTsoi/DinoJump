#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>
#include <esp_now.h>
#include <Preferences.h>
#include <WiFi.h>

const int DUCK_INPUT_PIN_1 = A1;
const int DUCK_INPUT_PIN_2 = A2;

const int JUMP_INPUT_BEAM_PIN_1 = D2; 
const int JUMP_INPUT_BEAM_PIN_2 = D3;
const int JUMP_INPUT_BEAM_PIN_3 = D4;

const int SW_PIN = D13;
const int DUCK_BUTTON_INPUT_PIN = D8;
const int JUMP_BUTTON_INPUT_PIN = D9;

const int SPIN_BUTTON_PIN = D6;
const int START_BUTTON_PIN = D7;

const int MOTOR_OUTPUT_PIN = D5;

const int IR_1_THRESHOLD = 300;
const int IR_2_THRESHOLD = 300;
const int SAMPLE_SIZE = 10;

#define LED_BUILTIN 13

const unsigned long TIMEOUT_LIMIT = 60000;
const unsigned long DEBOUNCE_MS = 50;

enum GlobalState { IDLE, READY, RUNNING };

GlobalState global_state = IDLE;
bool prev_foot_detected = false;

// Must match input_message in dino_jump.ino
typedef struct {
  bool jump_timeout;
  bool duck_advance;
  bool start;     // START pressed: start a game, or restart from the death screen
  bool power_off; // SPIN pressed while on: turn the fan and display off
} input_message;

typedef struct {
  int score;
  bool death;
} game_message;

input_message outgoing;
game_message incoming;

uint8_t gameMAC[] = {0x3C, 0x84, 0x27, 0xC2, 0xDF, 0x90};

Adafruit_LEDBackpack score_hex = Adafruit_LEDBackpack();
Adafruit_LEDBackpack highscore_hex = Adafruit_LEDBackpack();

Preferences preferences;
unsigned int highscore = 0;

const uint16_t digitToSegment[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                     0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void displayNumber(Adafruit_LEDBackpack &matrix, int number) {
  matrix.clear();
  if (number < 0 || number > 9999)
    return;
  int counts[4] = {0, 1, 3, 4};
  for (int i = 3; i >= 0; i--) {
    int digit = number % 10;
    matrix.displaybuffer[counts[i]] = digitToSegment[digit];
    number /= 10;
  }
  matrix.writeDisplay();
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  Serial.print("[onReceive] ");
  memcpy(&incoming, data, sizeof(incoming));
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println(incoming.score);
  digitalWrite(LED_BUILTIN, LOW);
  displayNumber(score_hex, incoming.score);
  if (incoming.death && global_state == RUNNING) {
    global_state = READY;
  }
}

void onSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("[onSend] ");
  Serial.print("Message: ");
  Serial.print(outgoing.jump_timeout);
  Serial.print(" ");
  Serial.print(outgoing.duck_advance);
  Serial.print(" Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void load_highscore() {
  preferences.begin("highscore", false);
  highscore = preferences.getUInt("counter", 0);
  // Reset invalid values that can't be displayed or beaten
  if (highscore > 9999) {
    highscore = 0;
    preferences.putUInt("counter", highscore);
  }
  preferences.end();
}

void setup() {
  Serial.begin(9600);

  // pin initialization
  pinMode(JUMP_INPUT_BEAM_PIN_1, INPUT_PULLUP);
  pinMode(JUMP_INPUT_BEAM_PIN_2, INPUT_PULLUP);
  pinMode(JUMP_INPUT_BEAM_PIN_3, INPUT_PULLUP);

  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(DUCK_BUTTON_INPUT_PIN, INPUT_PULLUP);
  pinMode(JUMP_BUTTON_INPUT_PIN, INPUT_PULLUP);

  pinMode(SPIN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);

  pinMode(MOTOR_OUTPUT_PIN, OUTPUT);

  digitalWrite(MOTOR_OUTPUT_PIN, LOW);

  load_highscore();

  score_hex.begin(0x70);
  highscore_hex.begin(0x71);
  displayNumber(score_hex, 0);
  displayNumber(highscore_hex, highscore);

  // ESP-NOW establishment
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Delay to ensure peer has initialzied ESP-NOW
  delay(1000);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gameMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(gameMAC)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
    }
  }

  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSend);

  outgoing.duck_advance = false;
  outgoing.jump_timeout = false;
  outgoing.start = false;
  outgoing.power_off = false;
}

void loop() {
  switch (global_state) {
    case IDLE:
      idle();
      break;

    case READY:
      ready();
      break;

    case RUNNING:
      running();
      break;

    default:
      break;
  }

  delay(1);
}

void send_input(bool jump_timeout, bool duck_advance, bool start, bool power_off) {
  outgoing.jump_timeout = jump_timeout;
  outgoing.duck_advance = duck_advance;
  outgoing.start = start;
  outgoing.power_off = power_off;
  esp_now_send(gameMAC, (uint8_t *)&outgoing, sizeof(outgoing));
}

// True once per press of SPIN (debounced press edge), so holding the button
// after turning the machine off doesn't turn it straight back on
bool spin_pressed() {
  static bool was_down = false;
  static unsigned long last_change = 0;

  bool down = digitalRead(SPIN_BUTTON_PIN) == LOW;
  if (down != was_down && millis() - last_change > DEBOUNCE_MS) {
    was_down = down;
    last_change = millis();
    return down;
  }
  return false;
}

void power_off() {
  digitalWrite(MOTOR_OUTPUT_PIN, LOW);
  send_input(false, false, false, true);
  displayNumber(score_hex, 0);
  global_state = IDLE;
}

void idle() {
  Serial.println("idle");
  digitalWrite(MOTOR_OUTPUT_PIN, LOW);

  while (!spin_pressed()) {
    delay(1);
  }

  digitalWrite(MOTOR_OUTPUT_PIN, HIGH);

  send_input(false, true, false, false);

  delay(1000);

  global_state = READY;
}

void ready() {
  Serial.println("ready");

  if (incoming.score > (int)highscore) {
    update_highscore(incoming.score);
    displayNumber(highscore_hex, highscore);
  }

  unsigned long first_ready_time = millis();

  bool start = false;
  bool timeout = (millis() - first_ready_time) > TIMEOUT_LIMIT;
  while (!timeout && !start) {
    if (spin_pressed()) {
      power_off();
      return;
    }
    timeout = (millis() - first_ready_time) > TIMEOUT_LIMIT;
    start = digitalRead(START_BUTTON_PIN) == LOW;
  }

  if (start) {
    // duck_advance moves the title screen on; start also restarts from the death screen
    send_input(false, true, true, false);

    global_state = RUNNING;
    incoming.score = 0;
  } else if (timeout) {
    digitalWrite(MOTOR_OUTPUT_PIN, LOW);

    send_input(true, false, false, false);

    global_state = IDLE;
  }
  displayNumber(score_hex, 0);
  
  delay(1000);
}

void running() {
  Serial.println("running");

  while (global_state == RUNNING) {
    if (spin_pressed()) {
      power_off();
      return;
    }
    detect_button_input();
    delay(20);
  }
}

void update_highscore(int score) {
  preferences.begin("highscore", false);
  preferences.putUInt("counter", score);
  preferences.end();
  highscore = score;
}

void detect_button_input(){
  bool duck = digitalRead(DUCK_BUTTON_INPUT_PIN) == LOW;
  bool jump = digitalRead(JUMP_BUTTON_INPUT_PIN) == LOW;

  send_input(jump, duck, false, false);
}
