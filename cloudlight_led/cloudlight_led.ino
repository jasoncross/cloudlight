/* 
Cloudlight LED Controller

Couple this with an IR Receiver over i2c for control

Inspired by Lighting Cloud Mood Lamp By James Bruce / http://www.makeuseof.com/

Sound sampling code originally by Adafruit Industries.  Distributed under the BSD license.
This paragraph must be included in any redistribution.
*/

#include <Wire.h>
#include "FastLED.h"
#include "EEPROM.h"       //used to store user settings in EEPROM (settings will persist on reboots)

// How many leds in your strip?
#define NUM_LEDS 100
#define DATA_PIN 2

#define EEADDR 166 // Start location to write EEPROM data.


// Mode enumeration - if you want to add additional party or colour modes, add them here; you'll need to map some IR codes to them later; 
// and add the modes into the main switch loop
enum Mode { ALLOFF, SOUNDCLOUD,FLATCOLOR,FLOATCOLOR,FLOATCOLORROTATE,STATICCOLOR,COLORRUN, MUSICREACT, AUTOSTORM, THUNDERBURST, 
            ROLLING, CRACK, RUMBLE, ZAP, ACID,REDCOLOR,GREENCOLOR,BLUECOLOR,WHITECOLOR,RAINBOWCYCLE};

// Set default settings here
Mode defaultMode = SOUNDCLOUD;
int defaultColorMode = 0;
int defaultActionSpeed = 5;
int defaultBrightnessFactor = 10;
int defaultSaveCounter = 2000; // used to limit how often the settings save - decrement to speed up but could wear EEPROM faster

Mode mode;
Mode lastMode = ALLOFF;
Mode resumeMode = ALLOFF;
int colorMode;
int lastColorMode;
int counter;
bool colorRotate = false;
int rotateColor;
int colorRunStartHue = 0;
int actionSpeed;
int brightness = 255;
int brightnessFactor;  // 10 level brightness factor
bool settingsChanged = false;
int saveCounter = defaultSaveCounter;


///////
//    COLOR MODES
//    0 - CLOUD
//    1 - RAINBOW
//    2 - RED
//    3 - ORANGE
//    4 - LIGHT ORANGE
//    5 - GOLD
//    6 - YELLOW
//    7 - GREEN
//    8 - LIGHT GREEN
//    9 - CYAN
//    10 - TEAL
//    11 - SKY BLUE
//    12 - BLUE
//    13 - DARK BLUE
//    14 - LYON BLUE
//    15 - PURPLE
//    16 - MAGENTA
//    17 - WHITE
//    18 - WARM WHITE
//    19 - PINK
//    20 - GREEN WHITE
//    21 - COOL WHITE


// Mic settings, shouldn't need to adjust these. 
#define MIC_PIN   A0  // Microphone is attached to this analog pin
#define DC_OFFSET  0  // DC offset in mic signal - if unusure, leave 0
#define NOISE     40  // Noise/hum/interference in mic signal
#define SAMPLES   10  // Length of buffer for dynamic level adjustment
byte
  volCount  = 0;      // Frame counter for storing past volume data
int
  vol[SAMPLES];       // Collection of prior volume samples
int      n, total = 30;
float average = 0;

bool quick_transition = false;
  
// used to make basic mood lamp colour fading feature
int rainbow_h;
int rainbow_direction = 1;

int pixelBrightness[NUM_LEDS];
uint32_t pixelColor[NUM_LEDS];
int pixelDirection[NUM_LEDS];

// music sampling
#define MAXVOLTS 5
const int sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;
float voltAverage;
int voltSampleCount = 0;
int flipper = 1;

// Define the array of leds
CRGB leds[NUM_LEDS];

//////////////////////////////////////////////////////////////////////
///*****                  Color Values & Labels               *****///
//////////////////////////////////////////////////////////////////////
   
#define C_OFF         0x000000
#define C_RED         0xFF0000
#define C_ORANGE      0xFF8000
#define C_LIGHTORANGE 0xFFA500
#define C_GOLD        0xFFD700
#define C_YELLOW      0xFFFF00
#define C_GREEN       0x00FF00
#define C_LIGHTGREEN  0x7CFC00
#define C_CYAN        0x00FFFF
#define C_TEAL        0x008080
#define C_SKYBLUE     0x00BFFF
#define C_BLUE        0x0000FF
#define C_DARKBLUE    0x00008B
#define C_LYONBLUE    0x8A2BE2
#define C_PURPLE      0x800080 
#define C_MAGENTA     0xFF00FF
#define C_WHITE       0xFFFFFF
#define C_WARMWHITE   0xFFFFF0
#define C_PINK        0xFFB6C1
#define C_GREENWHITE  0xF0FFF0
#define C_COOLWHITE   0xF0FFFF

//
// The colors in the table are repeated several time to give them a
// frequency of being shown. Same is true for the OFF state.
// This is done to give the HPLeds a more realistic light show.
// 
const uint32_t basicColors[22][10] = {{   C_WHITE,  C_WHITE,   C_WHITE,   C_WHITE,   C_BLUE, C_YELLOW, C_PURPLE, 0xADD8E6, 0xFAFBA5, 0xCC99FF},    // STORM - whites with blues, yellows, purples
                                      {   C_RED,  C_ORANGE,   C_YELLOW,   C_GREEN,   C_BLUE, C_CYAN, C_PURPLE, C_WHITE, C_MAGENTA, C_OFF},    // Random
                                      {     C_RED,     C_RED,     C_RED,  0xDC143C, 0xB22222,  0xFF6347, 0xFF4500,   0xDB7093, C_RED, C_RED},    // Red
                                      {  C_ORANGE,  C_ORANGE,  C_ORANGE,  0xFF4500, 0xFB9B3A,  0xFFBE7D, 0xFCD2A7,   0xFF8C00, 0xFF8C00, C_OFF},    // Orange
                                      {  C_LIGHTORANGE,  C_LIGHTORANGE,  C_LIGHTORANGE,  C_LIGHTORANGE, 0xFB9B3A,  0xFFBE7D, 0xFCD2A7,   C_ORANGE, C_ORANGE, C_OFF},    // Light Orange
                                      {  C_GOLD,  C_GOLD,  C_GOLD,  C_GOLD, C_GOLD,  C_YELLOW, C_YELLOW,   C_LIGHTORANGE, C_LIGHTORANGE, C_OFF}, // gold
                                      {  C_YELLOW,  C_YELLOW,  C_YELLOW,  C_WHITE, 0xFDFD43,  0xFFFF82, 0xFFFFBA,   C_OFF, C_OFF, C_OFF},    // Yellow
                                      {   C_GREEN,   C_GREEN,   C_GREEN,  C_WHITE, 0x57FC57,  0x80FC80, 0xBDFFB1,   C_LIGHTGREEN, 0x00FF00, C_OFF},    // Green
                                      {   C_LIGHTGREEN,   C_LIGHTGREEN,   C_LIGHTGREEN,  C_WHITE, 0x00FF00,  C_GREEN, 0xADFF2F,   0x9ACD32, 0x7FFF00, C_OFF}, // light green
                                      {  C_CYAN,    C_CYAN,    C_CYAN,  C_WHITE, 0x38FFFF,  0x71FDFD, 0xA4FDFD,   0x008B8B, 0x008080, C_OFF},    // Cyan
                                      {  C_TEAL,    C_TEAL,    C_TEAL,  C_WHITE, 0x20B2AA,  0x20B2AA, 0x008B8B,   0x40E0D0, 0x40E0D0, C_OFF}, // teal
                                      { C_SKYBLUE,    C_SKYBLUE,    C_SKYBLUE,  C_WHITE, C_BLUE,  0x87CEEB, 0x87CEFA,   0xADD8E6, C_BLUE, C_OFF}, // sky blue
                                      {    C_BLUE,    C_BLUE,    C_BLUE,  C_WHITE, 0xACACFF,  0x7676FF, 0x5A5AFF,   C_SKYBLUE, 0x0000CD, C_OFF},    // Blue
                                      { C_DARKBLUE,    C_DARKBLUE,    C_DARKBLUE,  C_BLUE, 0x191970,  0x000080, 0x4B0082,   C_SKYBLUE, 0x0000CD, C_OFF}, // Dark blue
                                      { C_LYONBLUE,    C_LYONBLUE,    C_LYONBLUE,  C_BLUE, C_DARKBLUE,  0x4169E1, 0x4B0082,   0x6A5ACD, 0x7B68EE, C_OFF}, // lyon blue
                                      {  C_PURPLE,  C_PURPLE,  C_PURPLE,  C_WHITE, 0x9400D3,  0x9932CC, 0x9370DB,   0xBA55D3, 0xDDA0DD, C_OFF},    // Purple
                                      { C_MAGENTA, C_MAGENTA, C_MAGENTA,  C_WHITE, 0xFB3BFB,  0xFD75FD, 0xFD9EFD,   0xFF1493, 0xDA7066, C_OFF},    // Megenta 
                                      {   C_WHITE,   C_WHITE,   C_WHITE,  C_WHITE, C_WHITE,  C_COOLWHITE, C_WARMWHITE,   C_WHITE, C_GREENWHITE, C_OFF},  // white
                                      { C_WARMWHITE,   C_WARMWHITE,   C_WARMWHITE,  C_WARMWHITE, C_WARMWHITE,  C_COOLWHITE, C_WHITE,   C_WHITE, C_GREENWHITE, C_OFF}, // warm white
                                      { C_PINK,   C_PINK,   C_PINK,  C_PINK, C_WARMWHITE,  0xFFC0CB, C_WHITE,   0xFF1493, C_WARMWHITE, C_OFF}, // pink
                                      { C_GREENWHITE,   C_GREENWHITE,   C_GREENWHITE,  C_GREENWHITE, C_GREENWHITE,  C_WARMWHITE, C_WHITE,   C_WHITE, C_COOLWHITE, C_OFF}, // green white
                                      { C_COOLWHITE,   C_COOLWHITE,   C_COOLWHITE,  C_COOLWHITE, C_COOLWHITE,  C_WARMWHITE, C_WHITE, C_WHITE, C_GREENWHITE, C_OFF}};   // cool White

void whatColor(int colorChoice) {
  Serial.print("Color selected: ");
  switch(colorChoice){
    case 0: 
      Serial.print("cloud mix"); break;
    case 1: 
      Serial.print("rainbow mix"); break;
    case 2: 
      Serial.print("red"); break;
    case 3: 
      Serial.print("orange"); break;
    case 4: 
      Serial.print("light orange"); break;
    case 5: 
      Serial.print("gold"); break;
    case 6: 
      Serial.print("yellow"); break;
    case 7: 
      Serial.print("green"); break;
    case 8: 
      Serial.print("light green"); break;
    case 9: 
      Serial.print("cyan"); break;
    case 10: 
      Serial.print("teal"); break;
    case 11: 
      Serial.print("sky blue"); break;
    case 12: 
      Serial.print("blue"); break;
    case 13: 
      Serial.print("dark blue"); break;
    case 14: 
      Serial.print("lyon blue"); break;
    case 15: 
      Serial.print("purple"); break;
    case 16: 
      Serial.print("magenta"); break;
    case 17: 
      Serial.print("white"); break;
    case 18: 
      Serial.print("warm white"); break;
    case 19: 
      Serial.print("pink"); break;
    case 20: 
      Serial.print("green white"); break;
    case 21: 
      Serial.print("cool white"); break;
  }
  Serial.println("");
}

uint32_t flatColors(int colorChoice) {
  if (colorChoice == 2) {
    //Serial.println("Color: red");
    return C_RED;
  } else if (colorChoice == 4) {
    //Serial.println("Color: green");
    return C_GREEN;
  } else if (colorChoice == 6) {
    //Serial.println("Color: blue");
    return C_BLUE;
  }
}

void setup() { 

  analogReference(EXTERNAL); // setting AREF to voltage divided input
  
  // this line sets the LED strip type - refer fastLED documeantion for more details https://github.com/FastLED/FastLED
  FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
  // starts the audio samples array at volume 15. 
  memset(vol, 15, sizeof(vol));
  Serial.begin(115200);
  Wire.begin(9);                // Start I2C Bus as a Slave (Device Number 9)
  Wire.onReceive(receiveEvent); // register event
  randomSeed(analogRead(0));
  
  readSettingsFromEEPROM();
}

void readSettingsFromEEPROM() {
  // Read EEPROM

  bool defaultReset = false;
  
  int EEAddr = EEADDR;
  EEPROM.get(EEAddr,mode); EEAddr +=sizeof(mode);
  EEPROM.get(EEAddr,colorMode); EEAddr +=sizeof(colorMode);
  EEPROM.get(EEAddr,actionSpeed); EEAddr +=sizeof(actionSpeed);
  EEPROM.get(EEAddr,brightnessFactor); EEAddr +=sizeof(brightnessFactor);
  
  Serial.println("EEPROM settings read:");

  Serial.print("mode: ");
  Serial.println(mode);
  Serial.print("colorMode: ");
  Serial.println(colorMode);
  Serial.print("actionSpeed: ");
  Serial.println(actionSpeed);
  Serial.print("brightnessFactor: ");
  Serial.println(brightnessFactor);
  
  if (mode < 0) {
    mode = defaultMode;
    defaultReset = true;
    Serial.print("mode:");
    Serial.println(mode);
  }
  if (colorMode < 0 || colorMode > 9) {
    colorMode = defaultColorMode;
    defaultReset = true;
  }
  if (actionSpeed < 0 || actionSpeed > 9) {
    actionSpeed = defaultActionSpeed;
    defaultReset = true;
  }
  if (brightnessFactor < 0 || brightnessFactor > 10) {
    brightnessFactor = defaultBrightnessFactor;
    defaultReset = true;
  }

  if (defaultReset == true) {
    Serial.println("Default settings reset.");
    writeSettingsToEEPROM();
  }
  
}


void writeSettingsToEEPROM() {
  int EEAddr = EEADDR;
  Serial.println("EEPROM settings saved.");
  /*if (mode == ALLOFF) { // if turned off, save prior mode
    EEPROM.put(EEAddr,resumeMode); EEAddr +=sizeof(resumeMode);
    Serial.print("mode: ");
    Serial.println(resumeMode);
  } else {
    EEPROM.put(EEAddr,mode); EEAddr +=sizeof(mode);
    Serial.print("mode: ");
    Serial.println(mode);
  }*/
  EEPROM.put(EEAddr,mode); EEAddr +=sizeof(mode);
  Serial.print("mode: ");
  Serial.println(mode);
  EEPROM.put(EEAddr,colorMode); EEAddr +=sizeof(colorMode);
  EEPROM.put(EEAddr,actionSpeed); EEAddr +=sizeof(actionSpeed);
  EEPROM.put(EEAddr,brightnessFactor); EEAddr +=sizeof(brightnessFactor);
  

  Serial.print("colorMode: ");
  Serial.println(colorMode);
  Serial.print("actionSpeed: ");
  Serial.println(actionSpeed);
  Serial.print("brightnessFactor: ");
  Serial.println(brightnessFactor);

  settingsChanged = false;
}

// used to seed random brightness and colors
void seedLEDs() {
    // seed some values
  //each LED in an array for brightness
  for (int i=0;i<NUM_LEDS;i++) {
    // set each one to a brightness
    pixelBrightness[i] = random(0,(brightnessValue()+1));
    //and a direction
    pixelDirection[i] = random(0,2);
  }
}

//used to seed a rainbow spectrum
void seedColors(int colorChoice) {
  int colorPos = 0;
  int colorDir = 0;
  for (int a=0;a<NUM_LEDS;a++){
    if (colorChoice == 1) { // rainbow
      pixelColor[a] = basicColors[colorChoice][colorPos];
      if (colorDir == 0)
        colorPos++;
      else
        colorPos--;
      if (colorPos == 10) {
        colorDir = 1;
        colorPos = 8;
      } else if (colorPos == -1) {
        colorDir = 0;
        colorPos = 1;
      }
    } else {
      pixelColor[a] = basicColors[colorChoice][random(0,10)];
    }
  }
}

// used to reset LED brightness
void resetLEDs() {
  //reset brightness
  for (int i=0;i<NUM_LEDS;i++) {
    // set each one to a brightness
    pixelBrightness[i] = brightnessValue();
    //and a direction
    pixelDirection[i] = 0;
  }
}

void receiveEvent(int bytes) {
  
  // Here, we set the mode based on the IR signal received. Check the debug log when you press a button on your remote, and 
  // add the hex code here (you need 0x prior to each command to indicate it's a hex value)
  while(Wire.available())
   { 
      unsigned int received = Wire.read();
      Serial.print("Receiving IR hex: ");
      Serial.println(received,HEX);
      switch(received){
        case 0x7D: // resume button
          resumeActions(); break;
        case 0xFD: // off button
          mode = ALLOFF; break;
        case 0x2F:
          mode = SOUNDCLOUD; break;
        case 0xEF:
          mode = ACID; break;
        case 0x6F: // rainbow cycle
          mode = RAINBOWCYCLE; break;
        case 0xAF: // color run
          mode = COLORRUN; break;
        case 0x8F:  //music reactive
          mode = MUSICREACT; break;
        case 0xC5:
          brightnessUp();break;
        case 0x45:
          brightnessDown();break;
        case 0x17:
          speedUp();break;
        case 0x37:
          speedDown();break;
        case 0xCF:
          setColorMode(0); break;
        case 0x4F:
          setColorMode(1); break;
        case 0xE5:
          setColorMode(2); break;
        case 0xD5:
          setColorMode(3); break;
        case 0xF5:
          setColorMode(4); break;
        case 0xC7:
          setColorMode(5); break;
        case 0xE7:
          setColorMode(6); break;
        case 0x65:
          setColorMode(7); break;
        case 0x55:
          setColorMode(8); break;
        case 0x75:
          setColorMode(9); break;
        case 0x47:
          setColorMode(10); break;
        case 0x67:
          setColorMode(11); break;
        case 0x5D:
          setColorMode(12); break;
        case 0x6D:
          setColorMode(13); break;
        case 0x4D:
          setColorMode(14); break;
        case 0x87:
          setColorMode(15); break;
        case 0xA7:
          setColorMode(16); break;
        case 0xDD:
          setColorMode(17); break;
        case 0xED:
          setColorMode(18); break;
        case 0xCD:
          setColorMode(19); break;
        case 0x7:
          setColorMode(20); break;
        case 0x27:
          setColorMode(21); break;
        case 0xDF:
          mode = FLATCOLOR; break;
        case 0x5F:
          mode = STATICCOLOR; break;
        case 0x9F:
          mode = FLOATCOLOR; break;
        case 0x1F:
          mode = FLOATCOLORROTATE; break;
        case 0xF:
          mode = AUTOSTORM; break;
        case 0xD7:
          mode = THUNDERBURST; break;
        case 0x57:
          mode = ROLLING; break;
        case 0x97:
          mode = CRACK; break;
        case 0xF7:
          mode = RUMBLE; break;
        case 0x77:
          mode = ZAP; break;
      }
      
   }

}
 
void loop() { 
  // Maps mode names to code functions. 
  // Serial.print("Mode: ");
  // Serial.println(mode);
  switch(mode){
    case SOUNDCLOUD: detect_thunder();off();break;
    case FLATCOLOR: flatColor(colorMode);break;
    case STATICCOLOR: staticColor(colorMode);break;
    case FLOATCOLOR: floatColor(colorMode);break;
    case FLOATCOLORROTATE: floatColor(-1);break;
    case COLORRUN: colorRun(colorMode);break;
    case MUSICREACT:  musicReactive(0);break;
    case AUTOSTORM: autoStorm();break;
    case THUNDERBURST: activateStorm(1);mode=lastMode;break;
    case ROLLING: activateStorm(2);mode=lastMode;break;
    case CRACK: activateStorm(3);mode=lastMode;break;
    case RUMBLE: activateStorm(4);mode=lastMode;break;
    case ZAP: activateStorm(5);mode=lastMode;break;
    case ACID: acid_cloud();off();break;
    case ALLOFF: offMode();break;
    case REDCOLOR: setColorMode(2);break;
    case BLUECOLOR: setColorMode(6);break;
    case GREENCOLOR: floatColor(4);break;
    case WHITECOLOR: single_colour(0, 0);break;
    case RAINBOWCYCLE: rainbowCycle();break;
    default: detect_thunder(); off();break;
  }
  
  // see if we need to save settings
  //Serial.println(settingsChanged);
  saveCounter--;
  if (saveCounter <= 0) {
    if (settingsChanged == true) {
      Serial.println("Settings have changed.");
      writeSettingsToEEPROM();
      settingsChanged = false;
    }
    saveCounter = defaultSaveCounter; // start over again
  }
}

void brightnessUp() {
  if (brightnessFactor < 10) {
    brightnessFactor++;
    Serial.print("Brightness Factor: ");
    Serial.println(brightnessFactor);
    settingsChanged = true;
  }
}

void brightnessDown() {
  if (brightnessFactor > 0){
    brightnessFactor--;
    Serial.print("Brightness Factor: ");
    Serial.println(brightnessFactor);
    settingsChanged = true;
  }
}

int brightnessValue() {
  return (5 + (25 * brightnessFactor));
}

void speedDown() {
  // speed is from delays so a higher value is slower
  if (actionSpeed < 9) {
    actionSpeed++;
    Serial.print("Action Speed: ");
    Serial.println(actionSpeed);
    settingsChanged = true;
  }
}

void speedUp() {
  // speed is from delays so a lower value is faster
  if (actionSpeed > 0) {
    actionSpeed--;
    Serial.print("Action Speed: ");
    Serial.println(actionSpeed);
    settingsChanged = true;
  }
}

// updates the color mode
void setColorMode(int colorChoice) {
  colorMode = colorChoice;
  whatColor(colorChoice);
  settingsChanged = true;
}

// Makes all the LEDs a single colour, see https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-rainbow-with-desc.jpg for H values
// This function is useful if you do not need to manipulate the saturation
void single_colour(int H){
  single_colour(H, 255);
}

// All a single flat color
void flatColor(int colorChoice) {
  if(lastMode != mode){
    Serial.println("Mode: flat color");
    // first on this mode
    resetLEDs();
    lastMode = mode;
  }
  for (int a=0;a<NUM_LEDS;a++){
    if(colorMode == 1) // rainbow color
      seedColors(1);
    else
      pixelColor[a] = basicColors[colorChoice][0]; // first color
    pixelBrightness[a] = brightnessValue();
    leds[a] = pixelColor[a];
    leds[a] %= pixelBrightness[a];
  }
  FastLED.show(); 
  delay(10);
}

// mix of colors from choice but not moving
void staticColor(int colorChoice) {
  if(lastMode != mode){
    Serial.println("Mode: static colors");
    // first on this mode
    resetLEDs();
    seedColors(colorChoice);
    lastMode = mode;
  }
  if (colorChoice != lastColorMode) { // update colors if a new mode
    seedColors(colorChoice);
    lastColorMode = colorChoice;
  }
  for (int a=0;a<NUM_LEDS;a++){
    pixelBrightness[a] = brightnessValue();
    leds[a] = pixelColor[a];
    leds[a] %= pixelBrightness[a];
  }
  FastLED.show(); 
  delay(10);
}

// resume function
void resumeActions() {
  if (mode == ALLOFF) {
    mode = resumeMode; lastMode = ALLOFF; 
  }
}

// All a single color but floating brightness
void floatColor(int colorChoice){
  if(lastMode != mode){
    Serial.println("Mode: float color");
    if (colorChoice == -1) {
      colorRotate = true;
      rotateColor = random(2,18);
      colorChoice = rotateColor;
      whatColor(colorChoice);
      //colorMode = rotateColor;
    } else {
      colorRotate = false;
    }
    //Serial.println("Seeding");
    // first on this mode
    seedLEDs();
    //Serial.println(pixelBrightness[0]);
    for (int a=0;a<NUM_LEDS;a++){
      pixelColor[a] = basicColors[colorChoice][random(0,10)];
      //pixelColor[a] = 0xFF0000;
      //leds[a] = basicColors[colorChoice][random(0,10)];
      //leds[a] = 0xFF0000;
    }
  } else {
    // not the first time in this mode
    if (colorChoice == -1)
      colorChoice = rotateColor; // override
  }
  lastMode = mode;
  if (colorRotate == true) {
    if (counter < (200 * (actionSpeed + 1))) { // variable speed
      counter++;
    } else {
      counter = 0;
      rotateColor = random(2,18);
      colorChoice = rotateColor;
      //colorMode = rotateColor;
      whatColor(colorChoice);
    }
  } else {
    counter = 0;
  }
  // set colors
  for (int a=0;a<NUM_LEDS;a++){
    leds[a] = pixelColor[a];
    /*if (colorChoice == 2) // red
      leds[a] = 0xFF0000;
    else if (colorChoice == 4) // green
      leds[a] = 0x00FF00;
    else if (colorChoice == 6) // blue
      leds[a] = 0x0000FF;
    else // white
      leds[a] = 0xFFFFFF;*/
    leds[a] %= pixelBrightness[a];
  }
  FastLED.show(); 
  delay(10);
  //change brightness
  for (int i=0;i<NUM_LEDS;i++){
    if (pixelDirection[i] == 0) {
      if (pixelBrightness[i] < (brightnessValue()-1)) {
        pixelBrightness[i]++;
        pixelBrightness[i]++;
      } else {
        pixelDirection[i] = 1;
      }
    } else {
      if (pixelBrightness[i] > 2) {
        pixelBrightness[i]--;
        pixelBrightness[i]--;
      } else {
        pixelDirection[i] = 0;
        //get new color
        pixelColor[i] = basicColors[colorChoice][random(0,10)];
      }
    }
  }
  //Serial.println(pixelBrightness[0]);
}

// For white color, set S (saturation) as 0
void single_colour(int H, int S){
  for (int i=0;i<NUM_LEDS;i++){
    leds[i] = CHSV( H, S, brightness);
  }
  //avoid flickr which occurs when FastLED.show() is called - only call if the colour changes
  if(lastMode != mode){
    FastLED.show(); 
    lastMode = mode;
  }
  delay(50);
}

void rainbowCycle(){
  if(lastMode != mode){
    Serial.println("Mode: rainbow cycle");
  }
  lastMode = mode;
  //mood lamp that cycles through colours
  for (int i=0;i<NUM_LEDS;i++){
    leds[i] = CHSV( rainbow_h, 255, brightness);
  }
  if(rainbow_h >254){
    rainbow_direction = -1; //reverse once we get to 254
  }
  else if(rainbow_h < 0){
    rainbow_direction = 1;
  }
    
  rainbow_h += rainbow_direction;
  FastLED.show();
  delay(actionSpeed * 10);
}

void colorRun(int colorChoice){
  if(lastMode != mode){
    Serial.println("Mode: colorRun");
  }
  lastMode = mode;
  //mood lamp that cycles through colours
  if (colorRunStartHue > 254) // reset if too high
    colorRunStartHue = 0;
  int nowHue;
  for (int i=0;i<NUM_LEDS;i++){
    nowHue = colorRunStartHue + (i*2);
    if (nowHue > 254)
      nowHue = nowHue - 255;
    leds[i] = CHSV( nowHue, 255, brightness);
    //Serial.println(nowHue);
  }
  FastLED.show();
  colorRunStartHue++;
  delay(actionSpeed * 10);
}

void detect_thunder() {
  if(lastMode != mode){
    Serial.println("Mode: storm on sound detection");
  }
  lastMode = mode;
  
  n   = analogRead(MIC_PIN);                        // Raw reading from mic 
  n   = abs(n - 512 - DC_OFFSET); // Center on zero
  n   = (n <= NOISE) ? 0 : (n - NOISE);             // Remove noise/hum
  vol[volCount] = n;                      // Save sample for dynamic leveling
  if(++volCount >= SAMPLES) volCount = 0; // Advance/rollover sample counter
 
  total = 0;
  for(int i=0; i<SAMPLES; i++) {
    total += vol[i];
  }
  
  // If you're having trouble getting the cloud to trigger, uncomment this block to output a ton of debug on current averages. 
  // Note that this WILL slow down the program and make it less sensitive due to lower sample rate.
  
  /*
  for(int t=0; t<SAMPLES; t++) {
    //initial data is zero. to avoid initial burst, we ignore zero and just add the current l
    Serial.print("Sample item ");
    Serial.print(t);
    Serial.print(":");
    Serial.println(vol[t]);
  }
  Serial.print("total");
  Serial.println(total);
  Serial.print("divided by sample size of ");
  Serial.println(SAMPLES);
  
 
  Serial.print("average:");
  Serial.println(average);

  Serial.print("current:");
  Serial.println(n);

  */
  
  average = (total/SAMPLES)+2;
  if(n>average * 1.2){
    Serial.println("TRIGGERED");
    off();
    activateStorm(0);
  }
}

void autoStorm () {
  off();
  if(lastMode != mode){
    resumeMode = lastMode;
    Serial.println("Mode: auto storm");
    counter = 0;
  }
  lastMode = mode;
  if (counter > random((actionSpeed * 200)*.6,(actionSpeed * 200)*1.3)) {
    activateStorm(0);
    counter = 0;
  }
  counter++;
}

void activateStorm(int stormSelect) {

  off();

    if (!stormSelect > 0)
      stormSelect = random(1,6);
   
    //I've programmed multiple types of lightning. Each cycle, we pick a random one. 
    switch(stormSelect){
      //switch(5){
  
      case 1:
        thunderburst();
        
         Serial.println("Thunderburst");
         delay(random(10,500));
        break;
       
      case 2:
        rolling();
        Serial.println("Rolling");
        break;
        
      case 3:
        crack();
        
        Serial.println("Crack");
        delay(random(50,250));
        break;

      case 4:
        Serial.println("Rumble");
        rumble();
        //Serial.println("End: rumble");
        delay(random(10,500));
        //Serial.println("End: delay");
        break;
      case 5:
        zap();
        Serial.println("Zap");
         delay(random(10,500));
        break;
    }
}

void offMode() {
  if(lastMode != mode){
    resumeMode = lastMode;
    Serial.println("Mode: off");
  }
  lastMode = mode;
  off();
}
 
// utility function to turn all the lights off.  
void off(){
  for (int i=0;i<NUM_LEDS;i++){
    leds[i] = CHSV( 0, 0, 0);
  }
  FastLED.show();
}

void refresh() {
  FastLED.show();
  delay(50);
}

void acid_cloud(){
    // a modification of the rolling lightning which adds random colour. trippy. 
    //iterate through every LED
    for(int i=0;i<NUM_LEDS;i++){
      if(random(0,100)>90){
        leds[i] = CHSV( random(0,255), 255, brightness); 

      }
      else{
        leds[i] = CHSV(0,0,0);
      }
    }
    FastLED.show();
    delay(random(5,100));
    off();
    
  //}
}

void musicReactive(int reactMode) {
  if(lastMode != mode){
    Serial.println("Mode: music reactive");
    voltSampleCount = 1; // reset
  }
  lastMode = mode;

  if (flipper == 1)
    reactMode = 0;
  else
    reactMode = 1;
  
  unsigned long startMillis= millis();  // Start of sample window
  unsigned int peakToPeak = 0;   // peak-to-peak level
   
  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;

  double maxVolts = 5.00;
  
  // collect data for 50 mS
   while (millis() - startMillis < sampleWindow)
   {
      sample = analogRead(MIC_PIN);
      if (sample < 1024)  // toss out spurious readings
      {
         if (sample > signalMax)
         {
            signalMax = sample;  // save just the max levels
         }
         else if (sample < signalMin)
         {
            signalMin = sample;  // save just the min levels
         }
      }
   }
   peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude
   double volts = (peakToPeak * 5.0) / 1024;  // convert to volts
   
   /*if (voltSampleCount >= SAMPLES)
    voltSampleCount = 1;  // reset them
   voltAverage = ((voltAverage * (voltSampleCount - 1)) + volts) / voltSampleCount;
   voltSampleCount++;

   // adjust volts for the baseline average
   volts = volts-voltAverage;
   if (volts < 0)
    volts = 0; // can't go negative
   maxVolts = ((double) MAXVOLTS) - volts; 

   Serial.print(voltSampleCount);
   Serial.print(":");
   Serial.print(voltAverage);
   Serial.print(":");
   Serial.print(volts);
   Serial.print(":");
   Serial.println(maxVolts);*/
 
   //Serial.println(volts);
   int hueNow = 255 * ((volts-.2)/(maxVolts-.2));
   int stopPoint = NUM_LEDS * ((volts-.2)/(maxVolts-.2));
   if (stopPoint == 1)
    stopPoint = 0; // quiet the first one
   /*Serial.print(volts);
   Serial.print(":");
   Serial.println(hueNow);*/
   if (reactMode == 0) {
     for (int i=0;i<NUM_LEDS;i++) {
      if (i < stopPoint)
        leds[i] = CHSV( 255-hueNow, 255, 255);
      else
        leds[i] = CHSV(0,0,0);
     }
   } else {
    for (int i=NUM_LEDS;i>0;i--) {
      if (i > NUM_LEDS - stopPoint)
        leds[i-1] = CHSV( 255-hueNow, 255, 255);
      else
        leds[i-1] = CHSV(0,0,0);
     }
   }
   FastLED.show();
   if (stopPoint <= 1)
    flipper = flipper * -1;
}

void rolling(){
  // a simple method where we go through every LED with 1/10 chance
  // of being turned on, up to 10 times, with a random delay wbetween each time

  uint32_t nowColor = basicColors[colorMode][random(0,10)]; 
  
  for(int r=0;r<random(2,10);r++){
    //iterate through every LED
    for(int i=0;i<NUM_LEDS;i++){
      if(random(0,100)>90){
        //leds[i] = CHSV( 0, 0, brightness); 
        leds[i] = nowColor;

      }
      else{
        //dont need reset as we're blacking out other LEDs her 
        leds[i] = CHSV(0,0,0);
      }
    }
    FastLED.show();
    delay(random(5,100));
    off();
    
  }
}

void crack(){
   //turn everything white briefly
   uint32_t nowColor = basicColors[colorMode][random(0,10)]; 
   for(int i=0;i<NUM_LEDS;i++) {
      leds[i] = nowColor;  
   }
   FastLED.show();
   delay(random(10,200));
   off();
   
   //Maybe flash a bit at end
  if (random(0,3) == 0) {
    for (int i=1;i<random(2,5);i++) {
      off();
      delay(random(10,100));
      for(int i=0;i<NUM_LEDS;i++) {
        leds[i] = nowColor;  
      }
      FastLED.show();
      delay(random(10,100));
    }
  }
}

void thunderburst(){

  // this thunder works by lighting two random lengths
  // of the strand from 10-20 pixels. 
  int rs1 = random(0,NUM_LEDS/2);
  int rl1 = random(10,20);
  int rs2 = random(rs1+rl1,NUM_LEDS);
  int rl2 = random(10,20);

  uint32_t nowColor = basicColors[colorMode][random(0,10)]; 
  
  //repeat this chosen method a few times, adds a bit of realism
  for(int r = 0;r<random(3,6);r++){
    
    for(int i=0;i< rl1; i++){
      leds[i+rs1] = nowColor;
    }
    
    if(rs2+rl2 < NUM_LEDS){
      for(int i=0;i< rl2; i++){
        leds[i+rs2] = nowColor;
      }
    }
    
    FastLED.show();
    //stay illuminated for a set time
    delay(random(10,50));
    
    off();
    delay(random(10,50));
  }
}
  
void rumble(){

// this thunder works by lighting two random lengths but in a certain area of the strand

  uint32_t nowColor = basicColors[colorMode][random(0,10)]; 
  int stringLength;
  int startPoint;
  int flashLength;
  int moveChance;
  
  //repeat this chosen strands a few times, adds a bit of realism
  for(int r = 0;r<random(2,8);r++){
    // get a length of string
    //Serial.print("stringLength: ");
    stringLength = random(1,10);
    //Serial.println(stringLength);
    // and a start point
    startPoint = random(1,(NUM_LEDS - stringLength));
    //Serial.print("startPoint: ");
    //Serial.println(startPoint);
  
    // Flash there a bit
    nowColor = basicColors[colorMode][random(0,10)];
    //Serial.print("flashLength: ");
    flashLength = random(1,8);
    // don't let it go beyond end of string
    if (flashLength + startPoint > NUM_LEDS)
      flashLength = NUM_LEDS - startPoint;
    else if (startPoint < NUM_LEDS) 
      flashLength = startPoint - 1;
    //Serial.println(flashLength);
    for(int i=0;i<random(1,7);i++){
      for(int b=startPoint;b<=startPoint+stringLength;b++){
        if (b >= 0 && b < NUM_LEDS)
          leds[b] = nowColor;
      }
      FastLED.show();
      //Serial.println("flash");
      delay(random(50,200));
      off();
      delay(random(50,500));
      // possibly move between flashes
      moveChance = random(0,3);
      if (moveChance == 1) {
        startPoint = startPoint + 2;
        //Serial.println("moving up");
      } else if (moveChance == 2) {
        startPoint = startPoint - 2;
        //Serial.println("moving down");
      } else {
        //Serial.println("staying here");
      }
    }
    delay(random(300,900));
  }
}

void zap() {
  // goes from one point of strand up

  uint32_t nowColor = basicColors[colorMode][random(0,10)]; 
  int stringLength;
  int startPoint;
  int flashLength = 1;
  int moveChance;
  int moveDir = random(0,2);

  stringLength = random(1,NUM_LEDS/2);
  startPoint = random(0,(NUM_LEDS - stringLength)+1);
  /*Serial.print("Start: ");
  Serial.println(startPoint);
  Serial.print("Length: ");
  Serial.println(stringLength);*/

  while (flashLength < stringLength) {
    // light the length
    for (int i = startPoint; i < (startPoint + flashLength); i++) {
      if (moveDir == 0)
        leds[i] = nowColor;
      else
        leds[NUM_LEDS-i] = nowColor;
    }
    FastLED.show();
    delay(random(10,50));
    //Maybe flash a bit as it grows
    if (random(0,5) == 0) {
      for (int i=1;i<random(2,5);i++) {
        off();
        delay(random(10,100));
        for (int i = startPoint; i < (startPoint + flashLength); i++) {
          if (moveDir == 0)
            leds[i] = nowColor;
          else
            leds[NUM_LEDS-i] = nowColor;
        }
        FastLED.show();
        delay(random(10,100));
      }
    }
    //Most likely extend length
    if (random(0,4) != 0)
      flashLength++;
  }

  //Maybe flash a bit at end
  if (random(0,5) != 0) {
    for (int i=1;i<random(2,9);i++) {
      off();
      delay(random(10,100));
      for (int i = startPoint; i < (startPoint + flashLength); i++) {
        if (moveDir == 0)
          leds[i] = nowColor;
        else
          leds[NUM_LEDS-i] = nowColor;
      }
      FastLED.show();
      delay(random(10,100));
    }
  }
}


void sparkle() {
  // pops around an area - NOT DONE

  uint32_t nowColor = basicColors[colorMode][random(0,10)]; 
  int stringLength;
  int startPoint;
  int flashLength = 1;
  int moveChance;
  int moveDir = random(0,2);

  stringLength = random(1,NUM_LEDS/2);
  startPoint = random(0,(NUM_LEDS - stringLength)+1);
  /*Serial.print("Start: ");
  Serial.println(startPoint);
  Serial.print("Length: ");
  Serial.println(stringLength);*/

  while (flashLength < stringLength) {
    // light the length
    for (int i = startPoint; i < (startPoint + flashLength); i++) {
      if (moveDir == 0)
        leds[i] = nowColor;
      else
        leds[NUM_LEDS-i] = nowColor;
    }
    FastLED.show();
    delay(random(10,50));
    //Maybe flash a bit as it grows
    if (random(0,5) == 0) {
      for (int i=1;i<random(2,5);i++) {
        off();
        delay(random(10,100));
        for (int i = startPoint; i < (startPoint + flashLength); i++) {
          if (moveDir == 0)
            leds[i] = nowColor;
          else
            leds[NUM_LEDS-i] = nowColor;
        }
        FastLED.show();
        delay(random(10,100));
      }
    }
    //Most likely extend length
    if (random(0,4) != 0)
      flashLength++;
  }

  //Maybe flash a bit at end
  if (random(0,5) != 0) {
    for (int i=1;i<random(2,9);i++) {
      off();
      delay(random(10,100));
      for (int i = startPoint; i < (startPoint + flashLength); i++) {
        if (moveDir == 0)
          leds[i] = nowColor;
        else
          leds[NUM_LEDS-i] = nowColor;
      }
      FastLED.show();
      delay(random(10,100));
    }
  }
}

void colorRun2(int colorChoice) {
  if(lastMode != mode)
    Serial.println("Mode: color run");
  lastMode = mode;
  uint32_t nowColor = basicColors[colorMode][random(0,10)]; 
  int colorSegments = random(1,5);
  int segLength = random(1,6);
  int segStart = random(0,20);
  int segGap = 20;

  while (mode == COLORRUN) {
    off();
    for (int i=0;i<colorSegments;i++){ // loop through segments
      for (int a=0; a<segLength; a++) { // loop through each segment length
        if (segStart + (segGap*i) + a < NUM_LEDS)  // dont do if exceeds LEDs
          leds[segStart + (segGap*i) + a] = nowColor;
        else
          leds[(segStart + (segGap*i) + a) - NUM_LEDS] = nowColor;
      }
    }
    FastLED.show();
    delay(10);
    segStart = segStart+1;
    if (segStart >= NUM_LEDS)
      segStart = 0;
  }
}

// basically just a debug mode to show off the lightning in all its glory, no sound reactivity. 
void constant_lightning(){
  switch(random(1,10)){
   case 1:
        thunderburst();
        delay(random(10,500));
         Serial.println("Thunderburst");
        break;
       
      case 2:
        rolling();
        Serial.println("Rolling");
        break;
        
      case 3:
        crack();
        delay(random(50,250));
        Serial.println("Crack");
        break;
        
    
  }  
}
  
