#include <IRremote.h>

#define IR_SEND_PIN 3
#define IR_RECEIVE_PIN 2

IRsend irsend;
IRrecv irrecv(IR_RECEIVE_PIN);

bool prev_foot_detected = false;

void setup() {
    Serial.begin(9600);
    IrReceiver.begin(IR_RECEIVE_PIN);
    IrSender.begin(IR_SEND_PIN);
}

void loop() {
    IrSender.sendNEC(0x00FF00FF, 32);
    delay(10);

    if (IrReceiver.decode()) {
        const String protocol_str = IrReceiver.getProtocolString();
        if (protocol_str == "NEC") {
            if (prev_foot_detected) {
                Serial.println("J");
            }
            prev_foot_detected = false;
            // Serial.println("Beam Ok!");
        } else {
            prev_foot_detected = true;
            // Serial.println("Beam Broken! Unknown protocol");
        }
        IrReceiver.resume();
    } else {
        prev_foot_detected = true;
        // Serial.println("Beam Broken! Failed to decode");
    }

    delay(100);
}
