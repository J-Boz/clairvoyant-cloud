/**
 * 
 * Refactored by: Jonathan Gillespie
 * For: Calirvoyant Cloud IOT Project
 * 
 * Original Base Code: 'CloudCloud'
 * LED Control
 * Sarah Al-Mutlaq @ SparkFun Electronics
 * Shawn Hymel @ SparkFun Electronics
 * September 18, 2015
 *
 * Accepts commands from Thing on how to control LEDs.
 * 
 */

// Debug
#define DEBUG    1

#include <Adafruit_NeoPixel.h>
#include <AltSoftSerial.h>
#include <avr/power.h>
#include "MiniStates.h"

// Pin definitions
#define CLOUD_PIN       6

// Other constants
const int TWINKLE_DELAY = 400;    // ms
const int IN_BUF_MAX = 7;         // bytes
const byte SERIAL_SOF = 0xA5;
const byte SERIAL_EOF = 0x5A;
const int TIMEOUT = 100;          // ms
const int DISCO_DELAY = 85;       // ms
const byte COMM_RGB = 1;
const byte COMM_DISCO = 2;
const byte COMM_BLUE_SKY = 3;
const byte COMM_NIGHT = 4;
const byte COMM_OVERCAST = 5;
const byte COMM_GOLDEN = 6;
const byte COMM_SNOW = 7;
const byte COMM_LIGHTNING = 8;

// Global variables
//SoftwareSerial softy(4, 5); // RX, TX
AltSoftSerial softy;  // RX = 8, TX = 9, do not use pin 10
unsigned long led_time;

                                             // was 300 instead of 120.
Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(120, CLOUD_PIN, NEO_GRB + NEO_KHZ800);

LEDState led_state;
NightState night_state;
int night_i;
int storm_state;
int storm_delay;
int disco_state;
int disco_delay;
byte in_msg[IN_BUF_MAX];
uint8_t bytes_received;
uint8_t color_r;
uint8_t color_g;
uint8_t color_b;

/****************************************************************
 * Setup
 ***************************************************************/
void setup() {
  
  // Set up debug serial console
#if DEBUG
  Serial.begin(9600);
  Serial.println("LED Control");
#endif

  // Set up software serial
  softy.begin(9600);
  
  // Configure LED pins
  pinMode(CLOUD_PIN, OUTPUT);

  // Define initial state
  led_time = millis();
  led_state = LED_DISCO;
  storm_state = 0;
  disco_state = 0;
  disco_delay = DISCO_DELAY;
  night_state = NIGHT_0;
  night_i = 1;
  color_r = 0;
  color_g = 0;
  color_b = 0;
  
  // Set initial random delay
  pinMode(A0, INPUT);
  randomSeed(analogRead(A0));
  storm_delay = random(900, 1500);
  
  // Clear LED strips
  ledStrip.begin(); 
  ledStrip.show();

  clearCloud();
}

/****************************************************************
 * Loop
 ***************************************************************/
void loop() {
  doLED();
  bytes_received = receiveMessage(in_msg, TIMEOUT);
  if ( bytes_received > 0 ) {
#if DEBUG
    Serial.println(in_msg[0]);
#endif
    switch ( in_msg[0] ) {
      case COMM_RGB:
        led_state = LED_RGB;
        color_r = in_msg[1];
        color_g = in_msg[2];
        color_b = in_msg[3];
        rgb(color_r, color_g, color_b);
        break;
      case COMM_DISCO:
        led_state = LED_DISCO;
        break;
      case COMM_BLUE_SKY:
        led_state = LED_BLUE_SKY;
        break;
      case COMM_NIGHT:
        night_state = NIGHT_0;
        led_state = LED_NIGHT;
        break;
      case COMM_OVERCAST:
        led_state = LED_OVERCAST;
        break;
      case COMM_GOLDEN:
        led_state = LED_GOLDEN;
        break;
      case COMM_SNOW:
        led_state = LED_SNOW;
        break;
      case COMM_LIGHTNING:
        clearCloud();
        led_state = LED_LIGHTNING;
        break;
      default:
        break;
    }
  }
}

/****************************************************************
 * Receiver
 ***************************************************************/
uint8_t receiveMessage(byte msg[], unsigned long timeout) {
  
  boolean valid;
  uint8_t buf_idx;
  unsigned long start_time;
  byte in_buf[IN_BUF_MAX];
  byte r;
  
  // Wait until we get a valid message or time out
  start_time = millis();
  valid = false;
  while ( !valid ) {
    
    // Wait until we see a Start of Frame (SOF) or time out
    memset(in_buf, 0, IN_BUF_MAX);
    while ( !softy.available() ) {
      if ( (millis() - start_time > timeout) && (timeout > 0) ) {
        return 0;
      }
    }
    r = softy.read();
    if ( r != SERIAL_SOF ) {
      continue;
    }

    // Read buffer
    delay(2);  // Magical delay to let the buffer fill up
    buf_idx = 0;
    while ( softy.available() > 0 ) {
      if ( buf_idx >= IN_BUF_MAX ) {
        buf_idx = IN_BUF_MAX - 1;
      }
      in_buf[buf_idx] = softy.read();
#if DEBUG
      Serial.print("0x");
      Serial.print(in_buf[buf_idx], HEX);
      Serial.print(" ");
#endif
      buf_idx++;
      delay(2);  // Magical delay to let the buffer fill up
    }
#if DEBUG
    Serial.println();
#endif

    if ( in_buf[buf_idx - 1] == SERIAL_EOF ) {
      valid = true;
    }
  }
  
  // Copy our message (don't copy checksum or EOF bytes)
  memcpy(msg, in_buf, buf_idx - 1);
  
  return buf_idx - 1;
}

/****************************************************************
 * LED Functions
 ***************************************************************/

// Update LED strips
void doLED() {
  
  // Display LEDs based on the current weather
  switch ( led_state ) {

    // RGB
    case LED_RGB:
      break;

    // Disco
    case LED_DISCO:
      disco();
      break;
    
    // Blue sky
    case LED_BLUE_SKY:
      blueSky();
      break;
      
    // Night
    case LED_NIGHT:
      nighttime();
      break;
      
    // Overcast
    case LED_OVERCAST:
      overcast();
      break;
      
    // Golden
    case LED_GOLDEN:
      sunSet();
      break;
      
    // Snow
    case LED_SNOW:
      //clearCloud();
      whiteClouds();
      break;
      
    // Lightning
    case LED_LIGHTNING:
      lightningStorm();
      break;
      
    // Do nothing
    default:
      break;
  }
}

// Display static RGB value
void rgb(uint8_t r, uint8_t g, uint8_t b) {
#if DEBUG
  Serial.print("RGB: ");
  Serial.print(r);
  Serial.print(",");
  Serial.print(g);
  Serial.print(",");
  Serial.print(b);
#endif
  for ( int i = 0; i < 120; i++ ) {
    ledStrip.setPixelColor(i, r, g, b);
  }
  ledStrip.show();
}

/****************************************************************
 * LED Strip - Disco Functions
 ***************************************************************/
void disco() {

  int i;
  int red;
  int green;
  int blue;

#if DEBUG
  Serial.print("Disco: ");
  Serial.println(disco_state);
#endif
  
  if ( (millis() - led_time) >= disco_delay ) {
    switch ( disco_state ) {

      case 0: 
        red = random(150);
        green = random(250);
        blue = random(200);
        for (i = 0; i < 50; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 1:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      case 2:
        red = random(250);
        green = random(100);
        blue = random(200);
        for (i = 200; i < 300; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;
  
      case 3:
        red = random(250);
        green = random(250);
        blue = random(250);
        for (i = 200; i < 300; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 4:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      case 5:
        red = random(250);
        green = random(250);
        blue = random(250);
        for (i = 200; i < 300; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 6:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;
  
      case 7:
        red = random(250);
        green = random(250);
        blue = random(250);
        for (i = 60; i < 110; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 8:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;
  
      case 9:
        red = random(250);
        green = random(250);
        blue = random(250);
        for (i = 0; i < 110; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 10:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;
  
      case 11:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 125; i < 175; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 12:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;
  
      case 13:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 125; i < 175; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 14:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;
  
      case 15:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 200; i < 300; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 16:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;
  
      case 17:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 0; i < 110; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 18:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      case 19:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 200; i < 300; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 20:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      case 21:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 60; i < 110; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 22:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      case 23:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 0; i < 50; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 24:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      case 25:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 60; i < 110; i++){
          ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 26:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      case 27:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 125; i < 175; i++){
        ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 28:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;
  
      case 29:
        red = random(200);
        green = random(200);
        blue = random(200);
        for (i = 125; i < 175; i++){
        ledStrip.setPixelColor(i, red, green, blue);
        }
        ledStrip.show();
        break;

      case 30:
        for (i = 0; i < 300; i++){
          ledStrip.setPixelColor(i, 0, 0, 0);
        }
        ledStrip.show();
        break;

      default:
        disco_state = 0;
        break;
    }
  }

  disco_state = (disco_state + 1) % 31;
  disco_delay = DISCO_DELAY;
  led_time = millis();
}

/****************************************************************
 * LED Strip - Weather Functions
 ***************************************************************/

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return ledStrip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return ledStrip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return ledStrip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

// Clear cloud function:
void clearCloud(){
  for (int i = 0; i < 120; i++){ //for all the LEDs
    ledStrip.setPixelColor(i, 0, 0, 0); //turn off in cloud one
  }
  ledStrip.show(); //show what was set in cloud one
}

// Bluesky function
void blueSky() {
  for(int i=0; i<120; i++) {    //for all of the LEDs 
    ledStrip.setPixelColor(i, 0, 170, 175);   //set LEDs a sky blue color in cloud one
  }
  ledStrip.show();
}

// White clouds function
void whiteClouds() {
  for(int i=0; i<120; i++) {   //for all of the LEDs 
    ledStrip.setPixelColor(i, 100, 100, 100);  //set LEDs white-ish in cloud one
  }
  ledStrip.show();
}


// Overcast funtion
void overcast() {
  for(int i=0; i<120; i++) {   //for all of the LEDs 
            ledStrip.setPixelColor(i, 70, 64, 75);  //set LEDs grey-ish in cloud one
   }
   ledStrip.show();
}

// Night function
void nighttime() {
  
  switch ( night_state ) {
    
    case NIGHT_0:
      for(int i=0; i<120; i++) {    //for all of the LEDs 
         ledStrip.setPixelColor(i, 0, 0, 102);  //set LEDs dark blue in cloud one
      }
      ledStrip.show();
      night_i = 1;
      night_state = NIGHT_1;
      led_time = millis();
      break;
       
    case NIGHT_1:
      if ( (millis() - led_time) >= 50 ) {
        ledStrip.setPixelColor((night_i-1), 255, 255, 255);  //set LEDs white in cloud one
        ledStrip.show();
        night_state = NIGHT_2;
        led_time = millis();
      }
      break;
     
    case NIGHT_2:
      if ( (millis() - led_time) >= 50 ) {
        ledStrip.setPixelColor(night_i, 255, 255, 255);  //set LEDs white in cloud one
        ledStrip.show();
        night_state = NIGHT_3;
        led_time = millis();
      }
      break;
       
    case NIGHT_3:
      if ( (millis() - led_time) >= 50 ) {
        ledStrip.setPixelColor((night_i+1), 255, 255, 255);  //set LEDs white in cloud one
        ledStrip.show();
        night_state = NIGHT_4;
        led_time = millis();
      }
      break;
      
    case NIGHT_4:
      if ( (millis() - led_time) >= TWINKLE_DELAY ) {
        ledStrip.setPixelColor((night_i-1), 0, 0, 102);  //set LEDs white in cloud one
        ledStrip.show();
        night_state = NIGHT_5;
        led_time = millis();
      }
      break;
      
    case NIGHT_5:
      if ( (millis() - led_time) >= 50 ) {
        ledStrip.setPixelColor(night_i, 0, 0, 102);  //set LEDs white in cloud one
        ledStrip.show();
        night_state = NIGHT_6;
        led_time = millis();
      }
      break;
      
    case NIGHT_6:
      if ( (millis() - led_time) >= 50 ) {
        ledStrip.setPixelColor((night_i+1), 0, 0, 102);  //set LEDs white in cloud one
        ledStrip.show();
        night_state = NIGHT_1;
        led_time = millis();
        night_i = (night_i + 15) % 120;  // Increment/reset index
      }
      break;
      
      default:
        night_state = NIGHT_0;
        break;
  }
}

// Sun set function:
void sunSet(){  
  int i;
  for(i=0; i<30; i++) {   //for the first 30 LEDs
    ledStrip.setPixelColor(i, 150, 20, 0);   //set LEDs red-ish in cloud one
  }
  for(i=31; i<60; i++) {  //for LEDs 31 to 60
    ledStrip.setPixelColor(i, 150, 90, 0);  //set LEDs orange-red in cloud one
  }
  for(i=61; i<90; i++) {  //for LEDs 61 to 90
    ledStrip.setPixelColor(i, 150, 50, 0);  //set LEDs red-orage in cloud one
  }
  for(i=91; i<120; i++) {  //for LEDs 91 to 120
    ledStrip.setPixelColor(i, 150, 50, 0);  //set LEDs red-orage in cloud one
  }
  
//  for(i=261; i<300; i++) {  //for LEDs 261 to the last LED
//     ledStrip.setPixelColor(i, wheel(((i * 256 / 300)) & 255));  //set LEDs to the first part of the color wheel (red-yellow) in cloud one
//  }

    ledStrip.show(); //show all the colors that were set in cloud one
}

// Lightning storm function
void lightningStorm() {

#if DEBUG
  Serial.print("Storm: ");
  Serial.println(storm_state);
#endif
  
  //Various types of lightning functions, where the first two 
  //numbers represent the area that the flash could randomly show 
  //up in, the next number is usually how long the flash is (in milisec)
  //the forth number is sometimes the size of the flash, and the
  //last is the color setting for the flash
  if ( (millis() - led_time) >= storm_delay ) {
    switch ( storm_state ) {
      case 0:
        jumpingFlash_a(50, 80, 50, ledStrip.Color(255,255,255));
        break;
      case 1:
        jumpingFlash_a(50, 80, 50, ledStrip.Color(255,255,255));
        break;
      case 2:
        scrollingFlash_a(20, 65, 50, 5, ledStrip.Color(255,255,255));   // SCROLLING <><>
        break;
      case 3:
        singleFlash_a(100, 120, 50, 15, ledStrip.Color(255,255,255));
        Serial.println("case 3 - singleFlash");
        break;
      case 4:
        singleFlash_a(100, 120, 50, 15, ledStrip.Color(255,255,255));
        break;
      case 5:
        singleFlash_a(100, 120, 50, 15, ledStrip.Color(255,255,255));
        break;
      case 6:
        scrollingFlash_a(0, 50, 50, 15, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 7:
        singleFlash_a(50, 100, 50, 5, ledStrip.Color(200,200,255));
        break;
      case 8:
        scrollingFlash_a(100, 120, 50, 15, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 9:
        jumpingFlash_a(100, 120, 50, ledStrip.Color(255,255,255));
        break;
      case 10:
        jumpingFlash_a(100, 120, 50, ledStrip.Color(255,255,255));
        break;
      case 11:
        multipleFlashs_a(20, 120, 150, 300, 50, 5, ledStrip.Color(255,255,255));
        break;
      case 12:
        flickerFlash_a(50, 120, 50, 25, ledStrip.Color(200,200,255));
        break;
      case 13:
        multipleFlashs_a(20, 120, 150, 300, 50, 5, ledStrip.Color(255,255,255));
        break;
      case 14:
        scrollingFlash_a(10, 60, 100, 15, ledStrip.Color(225,200,255));           // SCROLLING <><>
        break;
      case 15:
        flickerFlash_a(75, 120, 40, 25, ledStrip.Color(255,255,255));
        break;
      case 16:
        flickerFlash_a(75, 120, 40, 25, ledStrip.Color(255,255,255));
        break;
      case 17:
        flickerFlash_a(75, 120, 40, 25, ledStrip.Color(255,255,255));
        break;
      case 18:
        jumpingFlash_a(80, 120, 50, ledStrip.Color(255,255,255));
        break;
      case 19:
        flickerFlash_a(75, 120, 40, 25, ledStrip.Color(255,255,255));
        break;
      case 20:
        flickerFlash_a(50, 120, 50, 25, ledStrip.Color(200,200,255));
        break;
      case 21:
        scrollingFlash_a(80, 110, 100, 10, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 22:
        wholeCloudFlash_a(40, 100, ledStrip.Color(255,255,255));
        break;
      case 23:
        multipleFlashs_a(20, 120, 175, 300, 50, 5, ledStrip.Color(255,255,255));
        break;
      case 24:
        scrollingFlash_a(20, 65, 50, 3, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 25:
        flickerFlash_a(0, 100, 50, 50, ledStrip.Color(200,200,255));
        break;
      case 26:
        flickerFlash_a(0, 100, 50, 50, ledStrip.Color(200,200,255));
        break;
      case 27:
        singleFlash_a(75, 120, 40, 3, ledStrip.Color(255,255,255));
        break;
      case 28:
        singleFlash_a(100, 120, 50, 15, ledStrip.Color(255,255,255));
        break;
      case 29:
        scrollingFlash_a(20, 65, 50, 5, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 30:
        multipleFlashs_a(20, 120, 200, 300, 50, 5, ledStrip.Color(255,255,255));
        break;
      case 31:
        singleFlash_a(10, 120, 50, 30, ledStrip.Color(255,255,255));
        break;
      case 32:
        flickerFlash_a(0, 100, 50, 50, ledStrip.Color(200,200,255));
        break;
      case 33:
        scrollingFlash_a(100, 120, 50, 15, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 34:
        jumpingFlash_a(50, 120, 50, ledStrip.Color(255,255,255));
        break;
      case 35:
        jumpingFlash_a(0, 120, 50, ledStrip.Color(255,255,255));
        break;
      case 36:
        multipleFlashs_a(20, 120, 200, 300, 50, 5, ledStrip.Color(255,255,255));
        break;
      case 37:
        scrollingFlash_a(10, 60, 100, 7, ledStrip.Color(225,200,255));           // SCROLLING <><>
        break;
      case 38:
        singleFlash_a(75, 120, 40, 3, ledStrip.Color(255,255,255));
        break;
      case 39:
        singleFlash_a(75, 120, 40, 3, ledStrip.Color(255,255,255));
        break;
      case 40:
        jumpingFlash_a(80, 120, 50, ledStrip.Color(255,255,255));
        break;
      case 41:
        jumpingFlash_a(50, 80, 50, ledStrip.Color(255,255,255));
        break;
      case 42:
        scrollingFlash_a(20, 100, 50, 5, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 43:
        flickerFlash_a(0, 100, 50, 50, ledStrip.Color(200,200,255));
        break;
      case 44:
        scrollingFlash_a(40, 100, 50, 5, ledStrip.Color(255,255,255));           // SCROLLING <><>
        break;
      case 45:
        multipleFlashs_a(20, 120, 150, 300, 50, 5, ledStrip.Color(200,200,255));
        break;
      case 46:
        wholeCloudFlash_a(40, 100, ledStrip.Color(255,255,255));
        break;
      default:
        break;
    }
    storm_state = (storm_state + 1) % 47;
    storm_delay = random(900, 1500);
    led_time = millis();
  }
}

/****************************************************************
 * LED Strip - Lightning Functions
 ***************************************************************/

void singleFlash_a(int areaMin, int areaMax, int duration, int Size, uint32_t color) {
  int i;
  int flashStrike = random(areaMin, areaMax);
  for ((i = flashStrike - Size); (i < flashStrike); i++) {
   ledStrip.setPixelColor(i, 255,255,255);//color);
 }
 ledStrip.show();
 delay(duration);
 for ((i = flashStrike - Size); (i < flashStrike); i++) {
   ledStrip.setPixelColor(i, 0, 0, 0);
 }
 ledStrip.show();
}


void flickerFlash_a(int areaMin, int areaMax, int duration, int Size, uint32_t color) {
  int i;
  int flashStrike = random(areaMin, areaMax);
  for ((i = flashStrike - Size); (i < flashStrike); i++) {
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(50);
 for ((i = flashStrike - Size); (i < flashStrike); i++) {
   ledStrip.setPixelColor(i, 0, 0, 0);
 }
 ledStrip.show();
 delay(50);
 flashStrike = random(areaMin, areaMax);
  for ((i = flashStrike - Size); (i < flashStrike); i++) {
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(duration);
 for ((i = flashStrike - Size); (i < flashStrike); i++) {
   ledStrip.setPixelColor(i, 0, 0, 0);
 }
 ledStrip.show();
}




void scrollingFlash_a(int areaMin, int areaMax, int duration, int Size, uint32_t color) {
  int i;
  int flashStrike = random(areaMin, areaMax);
  for ((i = flashStrike - Size); (i < flashStrike); i++) {
    ledStrip.setPixelColor(i, color);
    ledStrip.show();
  }
  for ((i = flashStrike - Size); (i < flashStrike); i++) {
    ledStrip.setPixelColor(i, 0, 0, 0);
    ledStrip.show();
  }
  for ((i = flashStrike - Size); (i < flashStrike); i++) {
    ledStrip.setPixelColor(i, color);
    ledStrip.show();
  }
  delay(duration);
  for ((i = flashStrike - Size); (i < flashStrike); i++) {
    ledStrip.setPixelColor(i, 0, 0, 0);
    ledStrip.show();
  }
} 


void multipleFlashs_a(int areaOneMin, int areaOneMax, int areaTwoMin, int areaTwoMax, int duration, int Size, uint32_t color){
 int i;
 int oneStrike = random(areaOneMin, areaOneMax);
 int twoStrike = random(areaTwoMin, areaTwoMax);
 for ((i = oneStrike - Size); (i < oneStrike); i++) {
   ledStrip.setPixelColor(i, color);
   ledStrip.show();
 }
 for ((i = twoStrike - Size); (i < twoStrike); i++) {
   ledStrip.setPixelColor(i, color);
   ledStrip.show();
 }
 delay(duration);
 for ((i = oneStrike - Size); (i < oneStrike); i++) {
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
 for ((i = twoStrike - Size); (i < twoStrike); i++) {
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
}



void jumpingFlash_a(int areaStart, int areaEnd, int duration, uint32_t color) {
 int i;
 for (i = areaStart; (i < areaStart + 15); i++){
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(duration);
 for (i = areaStart; (i < areaStart + 15); i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
 for ((i = areaEnd - 15); i < areaEnd; i++){
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(duration);
 for ((i = areaEnd - 15); i < areaEnd; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
 }
 ledStrip.show();
 delay(duration);
 for ((i = areaEnd - 15); i < areaEnd; i++){
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(duration + 50);
 for ((i = areaEnd - 15); i < areaEnd; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
}


void wholeCloudFlash_a(int durationShort, int durationLong, uint32_t color) {
  int i;
  for (i = 60; i < 75; i++){
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(durationShort);
 for (i = 60; i < 75; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
 }
    ledStrip.show();
    delay(durationLong);
 for (i = 60; i < 75; i++){
   ledStrip.setPixelColor(i, color);
   ledStrip.show();
 }
 delay(durationShort);
 for (i = 60; i < 75; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
  for (i = 100; i < 115; i++){
   ledStrip.setPixelColor(i, color);
   ledStrip.show();
 }
 for (i = 100; i < 115; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
 for (i = 100; i < 115; i++){
   ledStrip.setPixelColor(i, color);
   ledStrip.show();
 }
 delay(durationShort);
 for (i = 100; i < 115; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
 for (i = 115; i < 120; i++){
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(durationShort);
 for (i = 115; i < 120; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
 }
 ledStrip.show();
 delay(durationShort);
 for (i = 115; i < 120; i++){
   ledStrip.setPixelColor(i, color);
 }
 ledStrip.show();
 delay(durationShort);
 for (i = 115; i < 120; i++){
   ledStrip.setPixelColor(i, 0, 0, 0);
   ledStrip.show();
 }
}

