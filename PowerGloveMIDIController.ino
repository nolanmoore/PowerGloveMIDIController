/*
 * Power Glove UHID - MIDI controller
 *
 * A modification of the "PGSensorHub" firmware to use the
 * Power Glove as a MIDI controller. The flex sensors, motion
 * control data and control pad buttons can be used to send
 * any type of MIDI message.
 *
 * https://github.com/nullhan/PowerGloveMIDIController
 */

#include <Keypad.h>
#include <Wire.h>
#include <LSM303.h>
#include <EEPROM.h>
#include "definitions.h"

// General
boolean motionMode = false;

// Keypad
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 6;
char PGKeys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'0','1','2','3','4',BUTTON_PROGRAM},
  {'5','6','7','8','9',BUTTON_ENTER},
  {'U','L','D','R',BUTTON_CENTER,'-'},
  {'-','-',BUTTON_SELECT,BUTTON_START,'B','A'}
};
byte rowPins[KEYPAD_ROWS] = {2, 3, 17, 16};
byte colPins[KEYPAD_COLS] = {5, 6, 7, 8, 11, 12};
Keypad pgKeypad = Keypad( makeKeymap(PGKeys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
char lastButton = '-';
long lastPressed = 0;

// Flex sensors
int flexPins[4] = {A6,A7,A8,A9};          // Ring, Middle, Index, Thumb
const int numReadings = 20;
int flexValueReadings[4][numReadings];    // Array of last numReadings flex sensor readings
int readIndex[4] = {0,0,0,0};             // 0 -> numReadings
int flexTotal[4] = {0,0,0,0};             // the running total
int flexValue[4] = {0,0,0,0};             // the average
int flexClosed[4] = {0,0,0,0};            // default closed fist values
int flexRelaxed[4] = {0,0,0,0};           // default relaxed values

const int posTolerance = 10;              // 1 / posTolerance
int flexPosTols[4] = {0,0,0,0};           // tolerance of position (defined by posTolerance)
int flexPosture = 0;                      // 0xXXXXXXXXUUUUTIMR - LSN is important
boolean reversed[4] = {false, false, false, true};

// IMU
LSM303 compass;

// LED
int ledPin = 4;
int ledState = HIGH;
unsigned long ledTimer;

// MIDI values
const int channel = 1;
int flexController[4] = {64, 5, 1, 11};       // Ring, Middle, Index, Thumb
int flexCtrlPrevious[[4] = {-1, -1, -1, -1};
int imuController[3] = {0, 1, 2}; // AccelX, AccelY, AccelZ
int imuCtrlPrevious[3] = {-1, -1, -1};
elapsedMillis msec = 0;

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  ledTimer = millis();

  Wire.begin();
  compass.init();
  compass.enableDefault();

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < numReadings; j++) {
      flexValueReadings[i][j] = 0;
    }
  }
  
  setFlexLimits();
}

void loop() {
  readFlexData();
  calcFlexPosture();
  
  char pressed = pgKeypad.getKey();
  if (pressed) {
    if (pressed == BUTTON_PROGRAM) {
      changeProgram();
    } else if (pressed == BUTTON_START) {
      changeFlexMode();
    } else if (pressed == BUTTON_SELECT) {
      setFlexLimits();
    } else if (pressed == '0') {  // Save current state values to EEPROM
      for (int i = 0; i < 4; i++) {
        EEPROM.update(2*i, flexRelaxed[i]);
        EEPROM.update(2*i+8, flexClosed[i]);
      }
    }
  }
      
  if (msec >= 50) {
    msec = 0;

    if (!motionMode) {
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
        if (n[i] != flexCtrlPrevious[i]) {
          usbMIDI.sendControlChange(flexController[i], n[i], channel);
          flexCtrlPrevious[i] = n[i]; 
        }
      }
    } else {
      compass.read();
      int imuValue[3];
      imuValue[0] = compass.a.x / 16;
      imuValue[1] = compass.a.y / 16;
      imuValue[2] = compass.a.z / 16;
      for (int i = 0; i < 3; i++) {
        if (imuValue[i] != imuCtrlPrevious[i]) {
          usbMIDI.sendControlChange(imuController[i], imuValue[i], channel);
          imuCtrlPrevious[i] = imuValue[i];
        }
      }
    }
    
  }

  if (millis() - ledTimer >= 500) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    ledTimer = millis();
  }
}

void badValueBlink() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledPin, HIGH);
    delay(250);
    digitalWrite(ledPin, LOW);
    delay(250);
  }
}

void readFlexData() {
  // Get sensor data and average it over last numReadings
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

void calcFlexPosture() {
  flexPosture = 0;
  for (int i = 0; i < 4; i++) {
    if (flexValue[i] < flexClosed[i] + flexPosTols[i]) {  // flexValue[i] >= flexClosed[i] && 
      flexPosture |= 1 << i;
    } else if (flexValue[i] > flexRelaxed[i] - flexPosTols[i]) {  //flexValue[i] <= flexRelaxed[i] && 
      flexPosture &= ~(1 << i);
    } else {
      flexPosture |= 1 << i+8;
    }
  }  
}

void setFlexLimits() {
  digitalWrite(ledPin, HIGH);
  
  boolean closedValuesSet = false;
  boolean relaxedValuesSet = false;

  while (!closedValuesSet || !relaxedValuesSet) {
    readFlexData();
    char pressed = pgKeypad.getKey();
    
    if (pressed) {
      if (pressed == 'B') {
        for (int i = 0; i < 4; i++) {
          flexRelaxed[i] = flexValue[i];
        }
        relaxedValuesSet = true;
      } else if (pressed == 'A') {
        for (int i = 0; i < 4; i++) {
          flexClosed[i] = flexValue[i];
        }
        closedValuesSet = true;
      } else if (pressed == 'S') {
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

  for (int i = 0; i < 4; i++) {
    flexPosTols[i] = (flexRelaxed[i] - flexClosed[i]) / posTolerance;
  }
}

int getKeypadNum() {
  boolean progEntered = false;
  int keys[3] = {0,0,0};
  
  int counter = 0;

  while (!progEntered) {
    char pressed = pgKeypad.getKey();

    if (pressed) {
      if (pressed >= '0' && pressed <= '9') {      
        keys[counter] = pressed - '0';
        counter++;
      } else if (pressed == BUTTON_ENTER || counter == 3) {
        progEntered = true;
      }
    }
  }

  int multiplier = 100;
  int progValue = 0;
  for (int i = 0; i < 3; i++) {
    progValue += keys[i] * multiplier;
    multiplier /= 10;
  }
  
  if (progValue >= 0 && progValue <= 127) {
    return progValue;
  } else {
    return -1;
  }
}

void changeProgram() {
  digitalWrite(ledPin, HIGH);
  
  int program = getKeypadNum();
  if (program >= 0) {
    usbMIDI.sendProgramChange(program, channel);
  } else {
    badValueBlink();
  }
}

void changeFlexMode() {
  digitalWrite(ledPin, HIGH);
  
  int posture = flexPosture;
  int ctrl = getKeypadNum();
  if (posture >= 0 && posture <= 15) {
    switch (flexPosture) {
      case 14:
        flexController[0] = ctrl;
        break;
      case 13:
        flexController[1] = ctrl;
        break;
      case 11:
        flexController[2] = ctrl;
        break;
      case 7:
        flexController[3] = ctrl;
        break;
      default:
        badValueBlink();
        break;
    }
  }
}

