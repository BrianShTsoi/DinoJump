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

Game game;
HologramFan display;
bool jumped = false;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
// uint32_t value = 0;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

char log_buf[4096] = {0};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

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

  ble_init();
}

void loop() {
  title_loop();
  
    // game_loop();
}

void ble_init() {

  // BLE BEGIN
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
  // BLE END
  
}

void ble_iter() {
    if (deviceConnected) {
        // pCharacteristic->setValue((uint8_t*)&value, 4);
        // pCharacteristic->notify();
        // value++;
        pCharacteristic->setValue(log_buf);
        pCharacteristic->notify();
        Serial.print("BLE sent: ");
        Serial.println(log_buf);
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

void title_loop() {
  // while(digitalRead(START_PIN) == HIGH) {
  //   delay(1);
  // }
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(FAN_PIN, HIGH);
  delay(1000);
  
  unsigned long prev = 0;
  unsigned long curr = millis();
  while (digitalRead(START_PIN) == HIGH) {
    if (digitalRead(IR_PIN) == LOW) {
      curr = millis();
      unsigned long ir_time_interval = curr - prev;
      sprintf(log_buf, "%lu: %lu", ir_time_interval, curr);
      prev = curr;
      ble_iter();

      display.flash_frame(TITLE_FRAME, digitalRead(BEAM_BREAK_PIN) == HIGH ? 0 : 1);
    }
    delay(1);
  }
}

void game_loop() {
  while (true) {
    if (digitalRead(IR_PIN) == LOW) {
      // Normally flashing a frame takes ~0.1s
      display.flash_frame(game.get_frame(), digitalRead(BEAM_BREAK_PIN) == HIGH ? 0 : 1);
  
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
