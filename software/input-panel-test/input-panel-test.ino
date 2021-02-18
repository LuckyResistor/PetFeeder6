//
// Pet Feeder Input Panel Demo
// ====================================================================================
// Copyright (c)2021 by Lucky Resistor. https://luckyresistor.me/
//


#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LiquidCrystal.h>

#include <cstdint>


// Create an instance of the LCD driver.
Adafruit_LiquidCrystal lcd(0);

// Input events
enum class Input : uint8_t {
  None = 0, // No input 
  Push, // Push button pressed.
  Left, // Rotated left
  Right // Rotated right
};


// The connections from the input panel to the pins of the MCU.
//
const uint8_t statusLed = 13;
const uint8_t led1 = 5;
const uint8_t led2 = 6;
const uint8_t led3 = 10;
const uint8_t led4 = 11;
const uint8_t displayEnable = 12;
const uint8_t buttonPush = 9;
const uint8_t buttonRotB = 0;
const uint8_t buttonRotA = 1;

// Array to address the LEDs via index
const uint8_t leds[] = {led1, led2, led3, led4};

// Working set:

// The last state of the rotary encoder.
uint8_t lastRotaryState = 0;
// The sequence of the last rotary states.
uint8_t lastRotaryStates[4];
// The last state of the push button.
bool lastPushButtonState = false;
// The time when the push button was pressed.
uint32_t pushButtonPressedTime = 0;

// The maximum number of input events.
const uint8_t inputMaximum = 32;
// An array with the last 32 input events.
Input inputs[inputMaximum];
// The current number of input events.
volatile uint8_t inputCount = 0;

// Counter for the demo.
uint32_t counter = 0;

// Enable the LED at the given index.
//
void setLedEnabled(uint8_t ledIndex, bool enabled) {
  const auto pin = leds[ledIndex];
  if (enabled) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  } else {
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
  }
}


// Push an input event in the queue.
//
void pushInput(Input input)
{
  if (inputCount < inputMaximum) {
    inputs[inputCount] = input;
    inputCount += 1;
  }
}

// Check for inputs.
//
Input getInput()
{
  noInterrupts();
  Input result = Input::None;
  if (inputCount > 0) {
    result = inputs[0];
    memmove(inputs, inputs+1, inputMaximum-1);
    inputCount -= 1;
  }
  interrupts();
  return result;
}


// Get the current state of the rotary encoder.
//
uint8_t getRotaryState()
{
  const bool rotA = (digitalRead(buttonRotA) == LOW);
  const bool rotB = (digitalRead(buttonRotB) == LOW);
  return (rotA ? 0b01 : 0b00) | (rotB ? 0b10 : 0b00);
}


// Get the current state of the push button.
//
uint8_t getPushButtonState()
{
  return (digitalRead(buttonPush) == LOW);
}


// Process the button states everytime the signal changes on one of the inputs.
//
void processButtons()
{
  const uint8_t rotaryState = getRotaryState();
  if (lastRotaryState != rotaryState) {
    lastRotaryState = rotaryState;
    // It has moved. Push this state into the array
    lastRotaryStates[3] = lastRotaryStates[2];
    lastRotaryStates[2] = lastRotaryStates[1];
    lastRotaryStates[1] = lastRotaryStates[0];
    lastRotaryStates[0] = rotaryState;
    // If we reached the zero state, check if we made a proper rotation.
    if (rotaryState == 0b00) {
      if (lastRotaryStates[1] == 0b10 && lastRotaryStates[2] == 0b11 && lastRotaryStates[3] == 0b01) {
        pushInput(Input::Left);
      } else if (lastRotaryStates[1] == 0b01 && lastRotaryStates[2] == 0b11 && lastRotaryStates[3] == 0b10) {
        pushInput(Input::Right);
      }
    }

    // display the rotary input
    setLedEnabled(1, (rotaryState & 0b01) != 0);
    setLedEnabled(2, (rotaryState & 0b10) != 0);
  }

  const bool pushButtonState = getPushButtonState();
  if (pushButtonState != lastPushButtonState) {
    lastPushButtonState = pushButtonState;
    if (pushButtonState) {
      // pushed, note the time.
      pushButtonPressedTime = millis();
    } else {
      // released
      const int32_t duration = static_cast<int32_t>(millis() - pushButtonPressedTime);
      if (duration > 50 && duration < 2000) {
        // valid button press.
        pushInput(Input::Push);
      }
    }
    // display button input.
    setLedEnabled(3, pushButtonState);
  }
}


void setup() {
  
  // Setup hardware
  pinMode(statusLed, OUTPUT);
  digitalWrite(statusLed, LOW);
  pinMode(displayEnable, OUTPUT);
  digitalWrite(displayEnable, LOW);
  pinMode(buttonPush, INPUT);
  pinMode(buttonRotB, INPUT);
  pinMode(buttonRotA, INPUT);

  // Initialize the rotary state.
  lastRotaryState = getRotaryState();

  // Attach button interrupts
  attachInterrupt(digitalPinToInterrupt(buttonPush), processButtons, CHANGE);
  attachInterrupt(digitalPinToInterrupt(buttonRotB), processButtons, CHANGE);
  attachInterrupt(digitalPinToInterrupt(buttonRotA), processButtons, CHANGE);

  // Test all LEDs
  for (uint8_t i = 0; i < 4; ++i) {
    for (uint8_t j = 0; j < 4; ++j) {
      setLedEnabled(j, true);
      delay(50);
      setLedEnabled(j, false);
    }
  }

  // Enable the display.
  digitalWrite(displayEnable, HIGH);
  delay(500); // wait until the chip is ready.
  // Initialize the display library.
  lcd.begin(20, 4);
  // Increase the speed on the I2C bus.
  Wire.setClock(400000);
  // Enable the backlight.
  lcd.setBacklight(HIGH);

  // Write a welcome message.
  lcd.setCursor(3, 0);
  lcd.print("Lucky Resistor");
  lcd.setCursor(2, 1);
  lcd.print("Pet Feeder Demo");

  // Enable the green light
  setLedEnabled(0, true);

  // Set the position and enable a blinking cursor.
  lcd.setCursor(0, 3);
  lcd.cursor();
  lcd.blink();
}


void updateCounter(uint32_t value) 
{
  lcd.setCursor(0, 2);
  lcd.print(value);
  lcd.print("          ");
}


void writeInputEvent(char c)
{
  static uint8_t pos = 0;
  lcd.setCursor(pos, 3); 
  lcd.print(c);
  pos += 1;
  if (pos >= 20) {
    pos = 0;     
  }
  lcd.setCursor(pos, 3); 
}


void processInputs()
{
  auto input = getInput();
  while (input != Input::None) {
    if (input == Input::Push) {
      counter = 0;
      updateCounter(counter);
      writeInputEvent('P');
    } else if (input == Input::Left) {
      counter -= 1;
      updateCounter(counter);
      writeInputEvent('L');
    } else if (input == Input::Right) {
      counter += 1;
      updateCounter(counter);
      writeInputEvent('R');
    }
    input = getInput();
  }
}


void loop() {
  processInputs();
  delay(1);
}
