/*
Kibble Dispenser by Dominic Gerber
https://github.com/dogerber/kibble_dispenser
*/

#define CODE_VERSION "0.1"

#include <Adafruit_MotorShield.h>
#include <EasyButton.h>
#include <SPI.h>              // display
#include <Wire.h>             // display
#include <Adafruit_GFX.h>     // display
#include <Adafruit_SSD1306.h> // display

// Global Parameters
bool do_beep = true;
const unsigned long min_wait_time_ms = 600000;         // [ms] minimum time to wait to dispense
const unsigned long max_wait_time_ms = 3600000;        // [ms] maximum time to wait
const int kibbles_total_available = 9;                 // how many fit into the dispenser [default: 9]
int motor_speed = 90;                                  // [0-255] 0 (off) to 255 (max speed)
const int max_dispenser_timeouts = 2;                  // after this many dispense tries, i stop trying
const unsigned long max_time_dispensing_ms = 6 * 1000; // time after it stops dispensing if no kibble falls out
const int max_time_back_move = 10 * 1000;              // [ms] maximum time of backwards movement (for when triggered by program)//todo reset to 10

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *myMotor = AFMS.getMotor(1); // Select which 'port' M1, M2, M3 or M4. In this case, M1

// Display setup
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// On an Arduino UNO:       A4(SDA), A5(SCL)
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Buttons
#define BUTTON_ONE_PIN 11
#define BUTTON_TWO_PIN 12
EasyButton button1(BUTTON_ONE_PIN);
EasyButton button2(BUTTON_TWO_PIN);

// PINS, Variables
const byte ledPin = 13;
const byte interruptPin = 2; // connected to IR
const int buzzerPin = 9;

// random triggering of dispensing
unsigned long wait_time;

volatile bool change_detected = false;
bool reset_done = false; // this is set true if the machine reset the plunger, waiting to be filled, press green button to confirm filling
volatile bool back_move_toggle = false;
bool back_move_toggle_before = false;
unsigned long time_back_move_started = millis();

int dispenser_timeout_counter = 0;
unsigned long time_last_dispense = 0;

unsigned long time_last_display_refresh = millis();
const int display_refresh_time = 1000 * 1; // refresh rate of screen

int kibbles_dispensed = 0;       // resets when filling dispenser
int kibbles_dispensed_total = 0; // just counts up

/*
Function to dispense a kibble. Once started it is stopped by the IR sensor (which detects a kibble falling down)
Can also be stoped by a timeout (max_time_dispensing_ms) and a button press.
// running like this is bad, as basically it is blocking behaviour.
*/
void dispense_kibble()
{
  time_last_dispense = millis(); // not down when function was last triggered

  // check if kibbles are still available
  if (kibbles_dispensed >= kibbles_total_available)
  {
    // error sound
    if (do_beep)
    {
      tone(buzzerPin, 200);
      delay(200);
      tone(buzzerPin, 200);
      delay(200);
      noTone(buzzerPin);
    }
    return;
  }

  bool do_run = true;
  long time_start = millis();

  // set motorspeed
  myMotor->setSpeed(motor_speed);
  int motor_speed_i = motor_speed;
  int count = 0;

  while (do_run)
  {
    myMotor->run(FORWARD);

    if (change_detected) // succesfull dispensed a kibble
    {
      do_run = false;
      change_detected = false;
      kibbles_dispensed++;
      kibbles_dispensed_total++;
      dispenser_timeout_counter = 0;
    }

    // button1 pressed, abort
    if ((time_start - millis() > 100) &&
        (!button1.read()))
    {
      do_run = false;
    }

    // timeout
    if (millis() - time_start > max_time_dispensing_ms)
    {
      do_run = false;
      dispenser_timeout_counter++;
      if (do_beep)
      {
        tone(buzzerPin, 500);
        delay(200);
        noTone(buzzerPin);
      }
    }
  }

  // stop motor
  myMotor->run(RELEASE);
  if (do_beep)
  {
    tone(buzzerPin, 100);
    delay(100);
    noTone(buzzerPin);
  }
}

void check_ir_led()
{
  // ISP, intterupt function
  change_detected = true;
}

// orange button
void onButton2Pressed()
{
  dispense_kibble(); // it would be better to not do this, because this is blocking behaviour
}

// green button
void onButton1Pressed()
{
  if (reset_done)
  {
    kibbles_dispensed = 0;
    dispenser_timeout_counter = 0;
    reset_done = false;
  }
  else
  {
    back_move_toggle = !back_move_toggle;
    kibbles_dispensed = 0; // reset counter, as now the user should have filled the dispenser
    dispenser_timeout_counter = 0;
  }
}

void setup()
{
  Serial.begin(115200); // set up Serial library at 9600 bps
  Serial.print("kibble_dispenser_code.ino started, v");
  Serial.println(CODE_VERSION);

  // init randomseed
  randomSeed(analogRead(1));
  wait_time = random(min_wait_time_ms, max_wait_time_ms);

  // ----- PIN setups
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Interrupts for IR sensor
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), check_ir_led, FALLING); // was RISING

  // --- Motor stuff
  if (!AFMS.begin())
  { // create with the default frequency 1.6KHz
    // if (!AFMS.begin(1000)) {  // OR with a different frequency, say 1KHz
    Serial.println("Could not find Motor Shield. Check wiring.");
    while (1)
      ;
  }
  Serial.println("Motor Shield found.");

  // init display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  if (true)
  { // welcome message
    display.clearDisplay();
    display.setTextSize(1);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);             // Start at top-left corner
    display.print("kibble_dispenser v");
    display.print(CODE_VERSION);
    display.display();
    delay(1000); // Pause for 2 seconds
  }

  // Clear the buffer
  display.clearDisplay();

  // Set the speed to start, from 0 (off) to 255 (max speed)
  myMotor->setSpeed(motor_speed);
  myMotor->run(FORWARD);
  // turn on motor
  myMotor->run(RELEASE);

  // Initialize buttons
  button1.begin();
  button2.begin();
  button1.onPressed(onButton1Pressed);
  button2.onPressed(onButton2Pressed);

  // Initialize Variables
  time_last_dispense = millis();

  // end of setup sound
  if (do_beep)
  {
    tone(buzzerPin, 500); // Sende 1KHz Tonsignal
    delay(100);
    tone(buzzerPin, 1000); // Sende 1KHz Tonsignal
    delay(100);
    noTone(buzzerPin);
  }
}

void loop()
{

  // Continuously read the status of the buttons
  button1.read();
  button2.read();

  // debugging output
  if (true && (millis() % 50) == 0)
  {
    char buff[128];
    // sprintf(buff, "back_move_toggle %d, kibbles_dispensed: %d", back_move_toggle, kibbles_dispensed);
    // Serial.println(buff);
    // sprintf(buff, "millis() %d, millis() - time_back_move_started %d, time_back_move_started %d,", millis(), millis() - time_back_move_started, time_back_move_started);
    // Serial.println(buff);
    sprintf(buff, "time_last_dispense %lu, ((millis() - time_last_dispense))  %lu, wait_time %lu, wait_time/1000 %lu,", time_last_dispense, ((millis() - time_last_dispense)), wait_time, wait_time / 1000);
    Serial.println(buff);
  }

  // add random triggering of the dispenser
  if ((kibbles_dispensed < kibbles_total_available) && (millis() - time_last_dispense > wait_time) && (dispenser_timeout_counter < max_dispenser_timeouts))
  {
    dispense_kibble();
    wait_time = random(min_wait_time_ms, max_wait_time_ms); // determine new wait time
  }

  // update display
  if (millis() - time_last_display_refresh > display_refresh_time)
  {
    display.clearDisplay();
    display.setTextSize(1); // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    char buff[128];

    // tell user to refill
    if (reset_done)
    {
      display.print("REFILL\n Press Green Button \n after filling");
    }
    else if (dispenser_timeout_counter >= max_dispenser_timeouts)
    {
      display.print("FAILED TO DISPENSE\n MULTIPLE TIMES\n Press Green to reset");
    }
    else
    {
      sprintf(buff, "disp: %d / %d,\n tot %d \n ", kibbles_dispensed, kibbles_total_available, kibbles_dispensed_total);
      display.print(buff);

      // show time till next auto dispening if (true)
      {
        sprintf(buff, "time: %lu / %lu min ", ((millis() - time_last_dispense)) / 60000, (wait_time) / 60000); // conversion to minutes /60000
        // Serial.println(buff);
        display.print(buff);
      }
    }

    // refresh display
    display.display();
  }

  // check if all dispensed and if so move back
  if ((!reset_done) && (kibbles_dispensed >= kibbles_total_available))
  {
    back_move_toggle = true;
    reset_done = true;
  }

  // move motor back or stop it
  if (back_move_toggle != back_move_toggle_before)
  {                        // motor state needs to be changed
    if (!back_move_toggle) // if already moving, stop the movement
    {
      myMotor->run(RELEASE);
      back_move_toggle_before = false;
    }
    else
    { // start movement
      myMotor->setSpeed(motor_speed);
      myMotor->run(BACKWARD);
      back_move_toggle_before = true;
      time_back_move_started = millis();
    }
  }

  // check for timeout of back movement
  if (back_move_toggle && (millis() - time_back_move_started > max_time_back_move))
  {
    back_move_toggle = false;
  }

  if (change_detected)
  {
    // ir triggered outside of dispense routine
    if (do_beep)
    {
      tone(buzzerPin, 500);
      delay(100);
      noTone(buzzerPin);
    }

    change_detected = false;
  }
}
