// Simple NeoPixel test.  Lights just a few pixels at a time so a
// 1m strip can safely be powered from Arduino 5V pin.  Arduino
// may nonetheless hiccup when LEDs are first connected and not
// accept code.  So upload code first, unplug USB, connect pixels
// to GND FIRST, then +5V and digital pin 6, then re-plug USB.
// A working strip will show a few pixels moving down the line,
// cycling between red, green and blue.  If you get no response,
// might be connected to wrong end of strip (the end wires, if
// any, are no indication -- look instead for the data direction
// arrows printed on the strip).

#include <Adafruit_NeoPixel.h>
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include <avr/sleep.h>

#define LOG 0
#define USB Serial

// #define DFPLAYER Serial1
//#define PIN 21 // HUZZAH32
#define PIN 4 // Arduino Uno
#define N_LEDS 16
#define WORM_LENGTH 8
#define INTERRUPT_PIN 2
#define STATUS_LED 13
#define SONG_LENGTH 13000 // 13 seconds
#define BRIGHTNESS 32 // 0-255  32=1/8


Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, PIN, NEO_GRB + NEO_KHZ800);
uint32_t position = 0;
uint8_t brightness;
uint32_t c;
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);
int switchedHigh = -1;
bool initialized = false;
unsigned long playingUntil = 0;
bool autoStart = false;

SoftwareSerial DFPLAYER(5, 6); // RX, TX

void setup() {
  DFPLAYER.begin(9600);
  USB.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  digitalWrite(STATUS_LED, 1);

  USB.println(F("init"));

  readInterruptStatus();
  start();
}

void loop() {
  readInterruptStatus();
  readDFPLayerStatus();
  rainbow(3);
  rainbowCycle(2);
  rainbow(3);
  rainbowCycle(1);
}

void start() {
  randomSeed(analogRead(0));

  if (!myDFPlayer.begin(DFPLAYER)) { // start serial connection with Dfplayer Mini
    USB.println(F("Unable to begin:"));
    USB.println(F("1.Please recheck the connection!"));
    USB.println(F("2.Please insert the SD card!"));
    while(true);
  }
  USB.println(F("DFPlayer Mini online."));

  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms

  USB.println(F("DFPlayer volume 20"));
  myDFPlayer.volume(20);  //Set volume value. From 0 to 30
  
  if (autoStart) {
    USB.println(F("Play the first mp3"));
    myDFPlayer.play(1);  //Play the first mp3
    playingUntil = millis() + SONG_LENGTH;
  }

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  initialized = true;
}

void readDFPLayerStatus() {
  if (myDFPlayer.available()) {
    printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
  }
}

void readInterruptStatus() {
  int previousValue = switchedHigh;
  int value = digitalRead(INTERRUPT_PIN);

  if (previousValue == -1 || previousValue != value) {
    USB.print(F("switchedHigh -> ")); USB.println(value);
    switchedHigh = value;
    if (previousValue != -1) {
      onInterruptChanged();
    }
  }
}

void repeat() {
  USB.println(F("repeat"));

  if (!initialized) {
    USB.println(F("not initialized"));
    return;
  }

  if (millis() < playingUntil) {
    USB.println(F("STOP"));
    playingUntil = 0;
    myDFPlayer.stop();
    return;
  }

  myDFPlayer.play(1);  //Play the first mp3
  playingUntil = millis() + SONG_LENGTH;
}

void onInterruptChanged() {
  USB.print(F("onInterruptChanged switchedHigh:")); USB.println(switchedHigh);
  repeat();
}

void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      USB.println(F("Time Out!"));
      break;
    case WrongStack:
      USB.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      USB.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      USB.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      USB.println(F("Card Online!"));
      break;
    case DFPlayerPlayFinished:
      USB.print(F("Number:"));
      USB.print(value);
      USB.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      USB.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          USB.println(F("Card not found"));
          break;
        case Sleeping:
          USB.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          USB.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          USB.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          USB.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          USB.println(F("Cannot Find File"));
          break;
        case Advertise:
          USB.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}


void gotoSleep() {
  USB.print(F("gotoSleep switchedHigh:")); USB.println(switchedHigh);
  digitalWrite(STATUS_LED, 0);
  sleep_enable();
  attachInterrupt(0, wakeUp, !switchedHigh);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  delay(100); // wait for LED to turn off
  sleep_cpu();

  // next line that is executed is after wakeup
  switchedHigh = digitalRead(INTERRUPT_PIN);
}

void wakeUp() {
  USB.println(F("wakeUp"));
  digitalWrite(STATUS_LED, 1);
  sleep_disable();
  detachInterrupt(0);
}


void delayWithStatusCheck(uint8_t wait) {
  for (int i=0; i<wait; i++) {
    delay(1);
    readDFPLayerStatus();
    readInterruptStatus();
  }
}



/////////////////////////
//      NEOPIXEL       //
/////////////////////////


// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delayWithStatusCheck(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delayWithStatusCheck(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, (Wheel(((i * 256 / strip.numPixels()) + j) & 255)));
    }
    strip.show();
    delayWithStatusCheck(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

