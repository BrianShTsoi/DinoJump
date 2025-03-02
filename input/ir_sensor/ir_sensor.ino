const int SAMPLE_SIZE = 10;
const int IR_PIN_0 = A0;
const int IR_PIN_1 = A1;
const int IR_PIN_2 = A2;
const int IR_0_THRESHOLD = 500;
const int IR_1_THRESHOLD = 500;
const int IR_2_THRESHOLD = 500;

void setup() {
  Serial.begin(9600);  
}

void loop() {
  // print_val();
  print_min_of_samples();
  // detect_ducking();
  // detect_jumping();
  delay(5);
}

void print_min_of_samples() {
  int min0 = min_of_samples(IR_PIN_0); 
  char buf[32] = {0};
  sprintf(buf, "0:%d", min0);
  Serial.println(buf);

  if (min0 > IR_0_THRESHOLD) {
    Serial.println("H0");
  } else {
    Serial.println("L0");
  }

  int min1 = min_of_samples(IR_PIN_1); 
  sprintf(buf, "1:%d", min1);
  Serial.println(buf);

  if (min1 > IR_1_THRESHOLD) {
    Serial.println("H1");
  } else {
    Serial.println("L1");
  }

  int min2 = min_of_samples(IR_PIN_2); 
  sprintf(buf, "2:%d", min2);
  Serial.println(buf);

  if (min2 > IR_2_THRESHOLD) {
    Serial.println("H2");
  } else {
    Serial.println("L2");
  }
}

void detect_ducking() {
  int min1 = min_of_samples(IR_PIN_1); 
  int min2 = min_of_samples(IR_PIN_2); 

  if (min1 < IR_1_THRESHOLD && min2 < IR_2_THRESHOLD) {
    Serial.println("D");
  }
}

int min_of_samples(const int pin) {
  uint32_t min = 0xFFFFFFFF; 

  for (int i = 0; i < SAMPLE_SIZE; i++) {
    int val = analogRead(pin);
    if (min > val) {
      min = val;
    }
  }

  return min;
}


void print_val() {
  // analogRead takes ~100us
  int read_val = analogRead(IR_PIN_1);
  char buf[32] = {0};
  sprintf(buf, "1:%d", read_val);
  Serial.println(buf);

  read_val = analogRead(IR_PIN_2);
  sprintf(buf, "2:%d", read_val);
  Serial.println(buf);
}

enum State { IDLE, STANDING, JUMPING, JUMPED };
State currentState = IDLE;

const int JUMP_COOLDOWN = 500;
long jump_start_time = 0;

void detect_jumping(){
  int min = min_of_samples(IR_PIN_0);

    long curr_time = millis();
    bool jump_cooldowned = curr_time - jump_start_time > JUMP_COOLDOWN;

  // if detect something previously -> don't detect anything currently = jump -> go back to detecting
  // if something was detected previously
  switch(currentState){
    case IDLE: //no person here
      //check if person appeared
      if(min > IR_0_THRESHOLD){ //person appeared
        currentState = STANDING;
      } else {
        currentState = IDLE;
      }
      break;
    case STANDING: //person standing there
      //check if person jumped
      if(min < IR_0_THRESHOLD && jump_cooldowned){ //person disappeared -> jumped
        currentState = JUMPING;
        jump_start_time = curr_time;
        Serial.println("J");
      } else {
        currentState = STANDING;
      }
      break;
    case JUMPING: // person jumping
      if(min < IR_0_THRESHOLD){ //person still jumping
        currentState = JUMPING;
      } else {
        currentState = JUMPED;
      }
      break;
    case JUMPED: //person jumped -> only register jumping once
      currentState = STANDING; //back to standing
      break;
  }

}


