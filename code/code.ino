/*
This is a test sketch for the Adafruit assembled Motor Shield for Arduino v2
It won't work with v1.x motor shields! Only for the v2's with built in PWM
control

For use with the Adafruit Motor Shield v2
---->	http://www.adafruit.com/products/1438
*/

#include <Adafruit_MotorShield.h>
#include <EasyButton.h>

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield();

// Select which 'port' M1, M2, M3 or M4. In this case, M1
Adafruit_DCMotor *myMotor = AFMS.getMotor(1);


#define WAIT_TIME_MS 1000 * 60 * 60  //todo clear up
#define PUSH_TIME_MS 500

// Buttons
#define BUTTON_ONE_PIN 11
#define BUTTON_TWO_PIN 12
EasyButton button1(BUTTON_ONE_PIN);
EasyButton button2(BUTTON_TWO_PIN);



// variables
const byte ledPin = 13;
const byte interruptPin = 2;  // connected to IR

const int buzzerPin = 9;

volatile bool change_detected = false;
bool back_move_toggle = false;

int motor_speed = 125;                          //[0-255] 0 (off) to 255 (max speed)
const int max_time_dispensing_ms = 60 * 1000;  // time after it stops dispensing if no kibble falls out
long time_last_dispense;

// functions

void dispense_kibble() {

  bool do_run = true;
  long time_start = millis();

  // set motorspeed
  myMotor->setSpeed(motor_speed);
  int motor_speed_i = motor_speed;
  int count = 0;

  while (do_run) {
    myMotor->run(FORWARD);
    //todo remove? delay(PUSH_TIME_MS);

    // speed up?
    // if (motor_speed_i<255){
    //   count++;
    //   motor_speed_i = motor_speed + count/100;
    //   myMotor->setSpeed(motor_speed_i);
    //   Serial.println(motor_speed_i);
    // }

    if (change_detected) {
      do_run = false;
      change_detected = false;
    }

    // timeout
    if (millis() - time_start > max_time_dispensing_ms) {
      do_run = false;
      tone(buzzerPin, 500);
      delay(200);
      noTone(buzzerPin);
    }
  }

  // stop motor
  myMotor->run(RELEASE);
  tone(buzzerPin, 100);
  delay(100);
  noTone(buzzerPin);
}


void check_ir_led() {
  // ISP, intterupt function
  change_detected = true;
}



void onButton2Pressed() {  // orange button
  // myMotor->setSpeed(GLOBAL_SPEED);
  // myMotor->run(FORWARD);
  // delay(PUSH_TIME_MS);
  // myMotor->run(RELEASE);

  dispense_kibble();
}

void onButton1Pressed() { //todo fix this mess
  back_move_toggle = !back_move_toggle;
  if (!back_move_toggle) {
    //delay(PUSH_TIME_MS); // stop movement
    myMotor->run(RELEASE);
    Serial.println("backwards stopped");
  } 
  else{ // start movement
    myMotor->setSpeed(motor_speed);
    myMotor->run(BACKWARD);
      Serial.println("backwards started");

  }
}




void setup() {
  Serial.begin(9600);  // set up Serial library at 9600 bps
  Serial.println("kibble_dispenser_code.ino started");


  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Lege den Interruptpin als Inputpin mit Pullupwiderstand fest
  pinMode(interruptPin, INPUT_PULLUP);
  // Lege die ISR 'blink' auf den Interruptpin mit Modus 'CHANGE':
  // "Bei wechselnder Flanke auf dem Interruptpin" --> "Führe die ISR aus"
  attachInterrupt(digitalPinToInterrupt(interruptPin), check_ir_led, FALLING);  // was RISING


  // Motor stuff
  if (!AFMS.begin()) {  // create with the default frequency 1.6KHz
                        // if (!AFMS.begin(1000)) {  // OR with a different frequency, say 1KHz
    Serial.println("Could not find Motor Shield. Check wiring.");
    while (1)
      ;
  }
  Serial.println("Motor Shield found.");

  // Set the speed to start, from 0 (off) to 255 (max speed)
  myMotor->setSpeed(motor_speed);
  myMotor->run(FORWARD);
  // turn on motor
  myMotor->run(RELEASE);


  // Initialize the button1
  button1.begin();
  button2.begin();


  // Add the callback function to be called when the button is pressed.
  button1.onPressed(onButton1Pressed);
  button2.onPressed(onButton2Pressed);

  time_last_dispense = millis();

  // end of setup sound
  if (true) {
    tone(buzzerPin, 500);  // Sende 1KHz Tonsignal
    delay(100);
    tone(buzzerPin, 1000);  // Sende 1KHz Tonsignal
    delay(100);
    noTone(buzzerPin);
  }
}

void loop() {
  uint8_t i;

  // Continuously read the status of the buttons
  button1.read();
  button2.read();

  //Serial.println("tick");

  // if (millis()-time_last_dispense > WAIT_TIME_MS){
  //   time_last_dispense = millis();


  // // try to dispense
  //   myMotor->run(FORWARD);
  // delay(PUSH_TIME_MS);

  //   myMotor->run(BACKWARD);
  // delay(PUSH_TIME_MS);

  //   myMotor->run(FORWARD);
  // delay(PUSH_TIME_MS);
  // myMotor->run(RELEASE);
  // }



  if (change_detected) {
    // ir triggered outside of dispense routine
    tone(buzzerPin, 500);  // Sende 1KHz Tonsignal
    delay(100);            // 1 sec Pause
    noTone(buzzerPin);     // Ton stoppen
    Serial.println("beeping");
    change_detected = false;
  }
}
