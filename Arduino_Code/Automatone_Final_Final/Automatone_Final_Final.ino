#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

const int pitchPot = A0;
const int mouthPotR = A1;
const int mouthPotL = A2;
const int volumePot = A3;
const int switchLowPos = 5;
const int switchMidPos = 4;
const int switchHighPos = 6;

int lowNoteLimit[10] =  {200, 451, 481, 541, 601, 661, 721, 781, 841, 901};
int highNoteLimit[10] = {450, 480, 540, 600, 661, 720, 780, 840, 900, 1000};
int scaleAA[10] = {0, 1, 4, 5, 7, 9, 10, 12, 13, 16};
int scaleBB[10] = {0, 1, 3, 5, 6, 8, 10, 12, 13, 15};
int bend = 0;
int noteVal = 0;
int16_t pitch = 0;

long pitchPotVal = 0;
int16_t mouthVal = 0; //will be sum of the two mouth pots
int mouthValMapped = 0;
int volumePotVal = 0;
int volumePotValMapped = -5;
int prevVolumePotValMapped = -5;
bool switchLowPosVal = 0;
bool switchMidPosVal = 0;
bool switchHighPosVal = 0;

bool prevSwitchLowPosVal = 0;
bool prevSwitchMidPosVal = 0;
bool prevSwitchHighPosVal = 0;
int prevPitch = -1;
int prevPitchPotVal = 0;
int prevMouthVal = 0;
int prevVolumePotVal = 0;
int pitchPotValDiff = 0;

const int diffPitchPots = 40;
const int diffMouthPots = 0;
const int diffVolumePot = 40;

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 150; // time to ignore a change in notes

int noteScale = 1;

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

void setup() {
  MIDI.begin(MIDI_CHANNEL_OMNI);

  pinMode(switchLowPos, INPUT);
  pinMode(switchMidPos, INPUT);
  pinMode(switchHighPos, INPUT);
}


void loop() {
  MIDI.read();
  readSensors();

  //select play mode
  if (!switchLowPosVal && !prevSwitchLowPosVal) { //if witch is on low and was on low before
    freeformMode(); //play in freeform
  } 
  if (!switchMidPosVal && !prevSwitchMidPosVal) { //if witch is on mid and was on mid before
    noteScale = 1; //play in scale mode A
    playScale();
  }
  if (!switchHighPosVal && !prevSwitchHighPosVal) { //if witch is on high and was on high before
    noteScale = 2; //play in scale mode B
    playScale();
  } 
}

void resetModes() {
  bend = 0;
  noteVal = 0;
  pitch = 0;
  pitchPotVal = 0;
  mouthVal = 0; //will be sum of the two mouth pots
  mouthValMapped = 0;
  volumePotVal = 0;
  volumePotValMapped = 125;
  prevVolumePotValMapped = 125;

  //prevPitch = -1;
  prevPitchPotVal = 0;
  prevMouthVal = 0;
  prevVolumePotVal = 0;
  pitchPotValDiff = 0;
}

void freeformMode() { //factory style slider method

  //read Pitch Pot
  for (int u = 0; u < 20; u++) { //average data to filter
    pitchPotVal = pitchPotVal + analogRead(pitchPot);
    //delay(5);
  }
  pitchPotVal = pitchPotVal / 20;
  pitchPotValDiff = pitchPotVal - prevPitchPotVal;
  pitch = map(pitchPotVal, 400, 900, -1000, 1000); //pitchbend
  pitch = constrain(pitch, -1000, 1000) * 8;

  volumePotValMapped = map(volumePotVal, 0, 1023, -15, 15); //map volume pot to adjust the key mid point
  volumePotValMapped = constrain(volumePotValMapped, -15, 15);

  if (pitchPotVal > 400) { //slider is being pushed down on
    if (prevPitchPotVal < 400) { //if it wasnt pressed before
      MIDI.sendNoteOn(60 + volumePotValMapped, 127, 1); //note ON
      delay(20);
      prevVolumePotValMapped = volumePotValMapped;
    }

    //if (abs(pitchPotValDiff) > diffPitchPots) { //if difference is big enough do pitchbend
    MIDI.sendPitchBend(pitch, 1);
    delay(20);
    //}
  } else {
    MIDI.sendNoteOff(60 + prevVolumePotValMapped, 0, 1); //of no slider push then note OFF
    delay(20);

  }

  mouthValMapped = map(mouthVal, 85, 1023, 55, 127);
  mouthValMapped=constrain(mouthValMapped,55,127);
  MIDI.sendControlChange(16, mouthValMapped, 1);
  delay(20);
  prevPitchPotVal = pitchPotVal;
}


void playScale() {
  // READ AND AVERAGE 20 PITCH POT VALUES
  for (int u = 1; u <= 20; u++) { //average data to filter
    pitchPotVal = pitchPotVal + analogRead(pitchPot);
  }
  pitchPotVal = pitchPotVal / 20;

  for (int i = 0; i < 10; i++) {
    if (pitchPotVal >= lowNoteLimit[i] && pitchPotVal <= highNoteLimit[i]) {
      pitch = i;
    }
  }

  mouthValMapped = map(mouthVal, 85, 1023, 50, 127);
  MIDI.sendControlChange(16, mouthValMapped, 1);
  delay(20);
  //volumePotValMapped = map(volumePotVal, 0, 1023, -8, 8); //map volume pot to adjust the key mid point
  //volumePotValMapped = constrain(volumePotValMapped, -8, 8);

  if (pitchPotVal >= 100) { //Do this if pitch pot is being pressed on
    if (prevPitch != pitch) { //Do this if note changed
      if ((millis() - lastDebounceTime) > debounceDelay) {
        if (noteScale == 1) {
          MIDI.sendNoteOn(53 + volumePotValMapped + scaleAA[pitch], 127, 1); //(pitch, 1);
          delay(20);
          MIDI.sendNoteOff(53 + volumePotValMapped + scaleAA[prevPitch], 0, 1);
          delay(20);
        } else if (noteScale == 2) {
          MIDI.sendNoteOn(51 + volumePotValMapped + scaleBB[pitch], 127, 1); //(pitch, 1);
          delay(20);
          MIDI.sendNoteOff(51 + volumePotValMapped + scaleBB[prevPitch], 0, 1);
          delay(20);
        }
        prevPitch = pitch;
        lastDebounceTime = millis();
      }
    } else {
      //Do nothing
    }
  } else {
    lastDebounceTime = millis();
    MIDI.sendControlChange(123, 0, 1);
    delay(20);
    prevPitch = -1;
    bend = 0;
  }
}

void readSensors() {

  // READ PITCH BEND POT Is handled differently in each mode
  //so it is not read here but instead is read inside the specificmode setion

  //Read Mouth Pots
  mouthVal = (analogRead(mouthPotR) + analogRead(mouthPotL)) / 2; //sum of both mouth pots

  //Read Volume Pot
  //volumePotVal = analogRead(volumePot);
  //delay(5);
  //Read Low/Mid/High Switch
  prevSwitchLowPosVal = switchLowPosVal;
  prevSwitchMidPosVal = switchMidPosVal;
  prevSwitchHighPosVal = switchHighPosVal;
  switchLowPosVal = digitalRead(switchLowPos);
  switchMidPosVal = digitalRead(switchMidPos);
  switchHighPosVal = digitalRead(switchHighPos);
}
