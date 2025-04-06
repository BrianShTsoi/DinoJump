#include "bit_array_2d.hpp"
#include "game.hpp"
#include "hologram_fan.hpp"
#include "title_frame.hpp"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define FAN_PIN D6
#define RESET_PIN D7
#define IR_PIN D8
#define BEAM_BREAK_PIN D9
#define JUMP_PIN D10
#define DUCK_PIN D11
#define START_PIN D12

// The upper limit in which we flash both PCBs
// With motor set to 129, the actual measured period varies between 260ms and 300ms
const unsigned long MAX_EXPECTED_PERIOD = 310;

Game game;
HologramFan display;
bool jumped = false;

// BLE enables wireless logging with several characters!
// TODO: We could bundle all this into a class to abstract it away, but who has time for that
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
char ble_msg[1024] = {0};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  // This takes 1ms
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

  // This takes ~17ms
  display.begin();

  attachInterrupt(digitalPinToInterrupt(JUMP_PIN), handleJump, RISING);

  ble_init();
}

void loop() {
  title_loop();
  game_loop();
}

void ble_init() {
  BLEDevice::init("ESP32");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
}

void ble_update() {
    if (deviceConnected) {
        pCharacteristic->setValue(ble_msg);
        pCharacteristic->notify();
        Serial.print("BLE sent: ");
        Serial.println(ble_msg);
        delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }

  
}

void reset_display() {
  // this takes 1ms
  Wire.begin();
  Wire.setClock(800000);

  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(5);
  digitalWrite(RESET_PIN, HIGH);

  // Takes ~17ms
  display.begin();
}

void title_loop() {
  while(digitalRead(START_PIN) == HIGH) {
    delay(1);
  }
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(FAN_PIN, HIGH);
  delay(1000);
  
  unsigned long prev = 0;
  unsigned long curr = millis();
  unsigned long period = 0;
  unsigned long time_passed = 0;

  while (digitalRead(START_PIN) == HIGH) {
    bool ir_detect = digitalRead(IR_PIN) == LOW;
    bool bb_connected = digitalRead(BEAM_BREAK_PIN) == HIGH;

    if (ir_detect && bb_connected) {
      reset_display();

      bool first_time = prev == 0;

      curr = millis();
      time_passed = curr - prev;
      prev = curr;

      // Skip first time as time_passed is not a valid value
      if (first_time) {
        continue;
      }

      display.flash_frame(LINE_TITLE_FRAME, 0);

      if (time_passed < MAX_EXPECTED_PERIOD) {
        unsigned long next_flash_time = curr + time_passed / 2;
        while (millis() < next_flash_time);
        display.flash_frame(TITLE_LINE_FRAME, 1);
        sprintf(ble_msg, "2! %lu: %lu", time_passed, curr);
      } else {
        sprintf(ble_msg, "1! %lu: %lu", time_passed, curr);
      }

      ble_update();
    }

    delay(1);

  }
}

void game_loop() {
  unsigned long prev = 0;
  unsigned long curr = 0;
  unsigned long period = 0;
  unsigned long time_passed = 0;

  while (true) {
    bool ir_detect = digitalRead(IR_PIN) == LOW;
    bool bb_connected = digitalRead(BEAM_BREAK_PIN) == HIGH;

    if (ir_detect && bb_connected) {
      reset_display();

      bool first_time = prev == 0;

      curr = millis();
      time_passed = curr - prev;
      prev = curr;

      // Skip first time as time_passed is not a valid value
      if (first_time) {
        continue;
      }

      display.flash_frame(game.get_frame(), 0);
  
      game_update();

      if (time_passed < MAX_EXPECTED_PERIOD) {
        unsigned long next_flash_time = curr + time_passed / 2;
        while (millis() < next_flash_time);

        display.flash_frame(game.get_frame(), 1);
        game_update();

        sprintf(ble_msg, "2! %lu: %lu", time_passed, curr);
      } else {
        sprintf(ble_msg, "1! %lu: %lu", time_passed, curr);
      }

      ble_update();

    }

    delay(1);
  }
}

// Updating game states take ~0.4ms
void game_update() {
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

void handleJump() {
  jumped = true;
}
