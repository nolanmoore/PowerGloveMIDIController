/*
 * Power Glove UHID - Teensy Sketch
 *
 * The basic Arduino example.  Turns on an LED on for one second,
 * then off for one second, and so on...  We use pin 13 because,
 * depending on your Arduino board, it has either a built-in LED
 * or a built-in resistor so that you need only an LED.
 *
 * http://www.arduino.cc/en/Tutorial/Blink
 */

#include <Keypad.h>
#include <EEPROM.h>

// Keypad
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 6;
char PGKeys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'0','1','2','3','4','M'},
  {'5','6','7','8','9','X'},
  {'U','L','D','R','C','-'},
  {'-','-','S','P','B','A'}
};
byte rowPins[KEYPAD_ROWS] = {2, 3, 17, 16};
byte colPins[KEYPAD_COLS] = {5, 6, 7, 8, 11, 12};
Keypad pgKeypad = Keypad( makeKeymap(PGKeys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
char lastButton = '-';
long lastPressed = 0;

// Flex sensors
int flexPins[4] = {A6,A7,A8,A9};
const int numReadings = 20;
int flexValueReadings[4][numReadings];
int readIndex[4] = {0,0,0,0};             // 0 -> numReadings
int flexTotal[4] = {0,0,0,0};             // the running total
int flexValue[4] = {0,0,0,0};             // the average
int flexClosed[4] = {0,0,0,0};            // default closed fist values
int flexRelaxed[4] = {0,0,0,0};           // default relaxed values

boolean reversed[4] = {false, false, false, true};

boolean closedValuesSet = false;
boolean relaxedValuesSet = false;

// MIDI 
byte program = 0;

// LED
int ledPin = 4;
int ledState = HIGH;
unsigned long ledTimer;

// MIDI values
// the MIDI channel number to send messages
const int channel = 1;
// the MIDI continuous controller for each analog input
const int controller[4] = {64, 5, 1, 11};
// store previously sent values, to detect changes
int previous[4] = {-1, -1, -1, -1};
elapsedMillis msec = 0;

void processADC();
void setRelaxedValue();
void setClosedValue();
void setMinMaxValue(int v);
void setFlexStateValues();

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  ledTimer = millis();

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < numReadings; j++) {
      flexValueReadings[i][j] = 0;
    }
  }
  
  setFlexStateValues();
}

void loop() {
  processADC();
  char pressed = pgKeypad.getKey();
  if (pressed) {
    if (pressed == '1') {
      setRelaxedValue();
    } else if (pressed == '2') {
      setClosedValue();
    } else if (pressed == '0') {  // Save current state values to EEPROM
      for (int i = 0; i < 4; i++) {
        EEPROM.update(2*i, flexRelaxed[i]);
        EEPROM.update(2*i+8, flexClosed[i]);
      }
    } else if (pressed == '5') {
      usbMIDI.sendProgramChange(1, channel);
    } else if (pressed == '6') {
      usbMIDI.sendProgramChange(10, channel);
    } else if (pressed == '7') {
      usbMIDI.sendProgramChange(13, channel);
    } else if (pressed == '8') {
      usbMIDI.sendProgramChange(52, channel);
    } else if (pressed == '9') {
      usbMIDI.sendProgramChange(55, channel);
    }
  }
      
  if (msec >= 50) {
    msec = 0;

    int n[4];
    for (int i = 0; i < 4; i++) {
      if (reversed[i]) {
        n[i] = map(flexValue[i], flexClosed[i], flexRelaxed[i], 0, 127);
      } else {
        n[i] = map(flexValue[i], flexRelaxed[i], flexClosed[i], 0, 127);
      }
      if (n[i] < 0) {
        n[i] = 0;
      } else if (n[i] > 127) {
        n[i] = 127;
      }
      if (n[i] != previous[i]) {
        usbMIDI.sendControlChange(controller[i], n[i], channel);
        previous[i] = n[i]; 
      }
    }
  }

  if (millis() - ledTimer >= 500) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    ledTimer = millis();
  }
}

void processADC() {
  for (int i = 0; i < 4; i++) {
    flexTotal[i] -= flexValueReadings[i][readIndex[i]];
    flexValueReadings[i][readIndex[i]] = analogRead(flexPins[i]);
    flexTotal[i] += flexValueReadings[i][readIndex[i]];

    readIndex[i]++;
    if (readIndex[i] >= numReadings) {
      readIndex[i] = 0;
    }

    flexValue[i] = flexTotal[i] / numReadings;
  }
}

void setRelaxedValue() {
  for (int i = 0; i < 4; i++) {
    flexRelaxed[i] = flexValue[i];
  }
}

void setClosedValue() {
  for (int i = 0; i < 4; i++) {
    flexClosed[i] = flexValue[i];
  }
}

void setFlexStateValues() {
  while (!closedValuesSet || !relaxedValuesSet) {
    processADC();
    char pressed = pgKeypad.getKey();
    
    if (pressed) {
      if (pressed == '1') {
        setRelaxedValue();
        relaxedValuesSet = true;
      } else if (pressed == '2') {
        setClosedValue();
        closedValuesSet = true;
      } else if (pressed == '0') {
        for (int i = 0; i < 4; i++) {
          byte hbyte = EEPROM.read(2*i+1);
          byte lbyte = EEPROM.read(2*i);
          flexRelaxed[i] = (int)hbyte << 8 + lbyte;
          hbyte = EEPROM.read(2*i+9);
          lbyte = EEPROM.read(2*i+8);
          flexClosed[i] = (int)hbyte << 8 + lbyte;
        }
        closedValuesSet = true;
        relaxedValuesSet = true;
      }
    }
  }
}
