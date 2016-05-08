# PowerGloveMIDIController

This Arduino sketch is designed to be used with my Power Glove UHID hardware to use the Power Glove as a MIDI device. The firmware is designed to work with the Teensy 3 series, and the upload type must be set to USB MIDI. The [Keypad library by Christopher Andrews](https://github.com/Chris--A/Keypad "Keypas library") and the standard EEPROM library are required.

## Controls
As the code currently stands, the glove's four flex sensors and buttons on the control pad are used to send control changes and note commands.

Flex sensors
- Thumb: Expression/Volume (Open -> Closed: FULL -> 0)
- Index: Modulation (0 -> FULL)
- Middle: Undefined
- Ring: Sustain (OFF -> ON)

Buttons
- 0: Update/read open and closed hand position values to/from EEPROM (during play/setup modes respectively)
- 1: Sets open hand values from current position
- 2: Sets closed hand values from current position
- 5: Program change
- 6: Program change
- 7: Program change
- 8: Program change
- 9: Program change

A control scheme to use the accelerometer data still needs to be designed. One idea is to use a digit to enable a motion control mode in which movement in the X, Y and Z axis and pitch, roll and yaw data can be assigned to individual settings.

## Implementation
An averaging scheme (avergae of last 20 values) is currently required for the flex sensor values as the sensors currenly in use are the original ones installed in the gloves in 1989. As such, they have become (or possibly always were) fairly imprecise, and they experience a certain amount of fluctuation. This was discovered after noticing the glove sending a constant stream of commands even when it was sitting still on a desk. If at some point the sensors are replaced with modern, precise versions, the averaging may not be necessary.
