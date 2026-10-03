/*
 * Example code for the ESP32 Tree Developer Board
 * This code is only an example of how the board can be used. In addition to controlling the Neopixels,
 * which is not particularly challenging, the example mainly illustrates the programmatic interaction
 * with the power‑management system. It also highlights the specifics of using the IR library in parallel
 * with the Neopixel LEDs, as well as the ability to learn commands from other IR remote controls.
 *
 * Several lighting patterns are displayed on the tree, and the user can switch between them either via
 * a button or an IR remote control or hardware switch. The remote can also be used to adjust brightness and to turn the
 * tree on and off.
 *
 * Special notes on power management:
 * The board distinguishes between USB power and battery power.
 *
 * BATTERY OPERATION
 * When running on battery, switching off via the button or the remote control results in a complete
 * shutdown of the power supply. For this purpose, a double‑click on the power button is triggered
 * in software via a FET. In an emergency, the power button can also be double‑clicked manually.
 * If the button is pressed only once, this is detected and the software triggers the required
 * double‑click via the FET. After shutdown, the board can only be powered on again using the power button.
 *
 * USB OPERATION
 * When powered via USB, the voltage regulator cannot be switched off. Pressing the power button or
 * turning the device off via the IR remote therefore only turns off the tree LEDs by displaying a
 * black pattern. Pressing the power button again or receiving the IR power‑on command restores the
 * previously active pattern.
 *
 * MODE SWITCHING
 * When USB power is removed, the board performs an electronic shutdown by generating a double‑click
 * via the FET.
 */


// ---- INCLUDES ----
#include <IRremote.hpp>             // infrared functions
#include "AllPattern.h"             // definition of all Patterns
#include "IRcodes.h"                // default IR-codes and constants
#include  <Adafruit_NeoPixel.h>     // for Use with WS2812-LEDsdas
#include <Preferences.h>            // for non valotile saving of IR-Codes, pattern and brightness
#include <WiFi.h>                   // only for deactivating WLAN to save power
#include "PatternEngine.h"
/* Arduino-Includes - uncomment in other IDEs
#include "serialprintf.ino"         // I like sprintf
#include "IRhandling.ino"           // learn and check IR-signals
#include "myPrefHandling.ino"       // load and save used preferences
#include "pattering.ino"            // display the patterns in their actual state on LEDs
*/

const uint8_t LED_COUNT  = 66; // how much LEDs are on the tree?
const uint8_t BRIGHTNESS = 20; // initial brightness, attention: be aware of high power cunsumtion when using high brightness

// ---- Setting up PINS ----
// digital output
const uint8_t LED_PIN    = 16; // output to WS2812-LEDs
const uint8_t PWRFETPIN  = 15; // output to FET to simulate power-switch
const uint8_t STAT_LED   = 21; // output to LED for state-info
// digital input
const uint8_t IRRECPIN   = 38; // input data from IR-Data-Pin of VS1838B
const uint8_t SWLEARN    = 10; // input from learn/mode-switch
// analog input
const uint8_t USB5VSENSE = 05; // analog read of USB-voltage
const uint8_t PWRSWSENSE = 04; // analog read to check the state of power-switch
const uint8_t ROOMLIGHT  = 02; // analog read of ambient brightness

#define PWRSWSENSE_THRESHOLD 100  // analog threshold for power-switch detection

#define APHOSTNAME "ChristmasTree4"

// ---- AUTO-BRIGHTNESS via LDR (IO2: LDR to VCC, 100k to GND) ----
#define LDR_DARK          1000   // ADC value in darkness  → adjust!
#define LDR_BRIGHT        3500   // ADC value in bright light → adjust!
#define AUTO_BR_MIN           3  // LED brightness in darkness  (0–255)
#define AUTO_BR_MAX         255  // LED brightness in full light (0–255)
#define AUTO_BR_INTERVAL    500  // measurement interval in ms

// ---- GLOBAL ----
Preferences preferences;                  // stores IR codes, pattern ID and brightness
int gPatStep = 0;                         // current animation step of the selected pattern

// Pattern ID as string instead of int index
char gSelectedPatternId[32] = "offpattern"; // active pattern; "offpattern" = tree off

// Shared state flags (volatile for task-safe access)
volatile bool gNeedPatternReload = true;  // true → loop reloads the pattern from LittleFS

Adafruit_NeoPixel strip(LED_COUNT,
                        LED_PIN,
                        NEO_GRB + NEO_KHZ800);
int gBrightness = BRIGHTNESS;             // user-set maximum brightness (IR/Preferences)
unsigned long gLastLdrCheck = 0;          // timestamp of the last LDR measurement
unsigned int gIRcodes[12];               // 12 IR codes: pattern 0–7, off, on, darker, brighter


/*
 * Reads the LDR at ROOMLIGHT and adjusts the strip brightness to the ambient light.
 * gBrightness (set via IR) acts as an upper limit — manual dimming stays in effect.
 */
void updateAutoBrightness() {
  unsigned long now = millis();
  if (now - gLastLdrCheck < AUTO_BR_INTERVAL) return;
  gLastLdrCheck = now;

  int raw    = analogRead(ROOMLIGHT);
  int clamped = constrain(raw, LDR_DARK, LDR_BRIGHT);
  int autoBr  = map(clamped, LDR_DARK, LDR_BRIGHT, AUTO_BR_MIN, AUTO_BR_MAX);
  autoBr      = constrain(autoBr, AUTO_BR_MIN, gBrightness);
  strip.setBrightness(autoBr);
  serialPrintf("ROOMLIGHT=%d autoBr=%d\n", raw, autoBr);
}


// little function to emulate power-off switching the voltage regulator (double-click)
void powerOff() {
    digitalWrite(PWRFETPIN, HIGH);
    delay(250);
    digitalWrite(PWRFETPIN, LOW);
    delay(300);
    digitalWrite(PWRFETPIN, HIGH);
    delay(250);
    digitalWrite(PWRFETPIN, LOW);
    delay(300);
    Serial.println("powered off");
}


// ---- SETUP ----
void setup() {

  btStop();  // always turn off Bluetooth, saves power
  // WiFi is NOT disabled here — setupWiFi() in webserver.ino
  // decides based on USB5VSENSE whether to start the AP or turn WiFi off

  // setting up hardware-pins
  pinMode(IRRECPIN, INPUT);
  pinMode(STAT_LED, OUTPUT);
  pinMode(PWRFETPIN, OUTPUT);
  digitalWrite(PWRFETPIN, LOW);
  pinMode(SWLEARN, INPUT_PULLUP);

  // setup serial to use for debugging
  Serial.begin(115200);
  delay(500);
  Serial.println("Good Morning, Dave...");

  // load IR-codes, last used pattern and last brightness-value
  getPreferences();

  // setup LEDs
  strip.begin();                      // initialize NeoPixel strip object (REQUIRED)
  strip.setBrightness(gBrightness);   // set brightness before displaying
  strip.show();                       // activate LEDs
  digitalWrite(STAT_LED, LOW);        // power state-LED off

  // start IR-receiving
  IrReceiver.begin(IRRECPIN, DISABLE_LED_FEEDBACK); // activate the IR pin

  // check if learn/mode-switch is pressed on powering on -> if yes, start the procedure to learn new IR-codes
  if (!digitalRead(SWLEARN)) {
    learnIR();
  }

  // Initialize LittleFS and migrate default patterns if needed
  initFilesystem();

  // Load pattern index from LittleFS (fills gPatternIds/gPatternNames/gPatternCount)
  loadPatternIndex();

  // Start WiFi AP if USB voltage is present, otherwise WiFi off
  setupWiFi();

  // Load first pattern
  loadPatternById(gSelectedPatternId);
  gNeedPatternReload = false;
}


// ---- MAIN ----
void loop() {

  // test if an IR-command is received
  checkIR();

  // Reload if the webserver or IR requested a new pattern
  if (gNeedPatternReload) {
    loadPatternById(gSelectedPatternId);
    gNeedPatternReload = false;
    gPatStep = 0;
  }

  // Output the current animation step of the loaded pattern to the LEDs
  patternwork();

  // If the learn/mode switch is pressed: move to the next pattern and save to flash
  if (!digitalRead(SWLEARN)) {
    digitalWrite(STAT_LED, HIGH);
    delay(100);
    while (!digitalRead(SWLEARN)) {}
    digitalWrite(STAT_LED, LOW);

    // Forward through the pattern list (excluding offpattern, which is always the last element)
    int nextIdx = gPatternListIndex + 1;
    if (nextIdx >= gPatternCount - 1) nextIdx = 0;  // -1: skip offpattern
    gPatternListIndex = nextIdx;
    strncpy(gSelectedPatternId, gPatternIds[nextIdx], 31);
    gSelectedPatternId[31] = '\0';
    gNeedPatternReload = true;
    gPatStep = 0;
    writePreferences();
  }

  // Power button: toggles between offpattern and the last saved pattern
  if (analogRead(PWRSWSENSE) < PWRSWSENSE_THRESHOLD) {
    if (strcmp(gSelectedPatternId, "offpattern") != 0) {
      strncpy(gSelectedPatternId, "offpattern", 31);
      gNeedPatternReload = true;
    } else {
      getPreferences();  // loads the last saved pattern into gSelectedPatternId
      gNeedPatternReload = true;
    }
    delay(1000);  // debounce
    serialPrintf( "USB5VSENSE %d\n", analogRead(USB5VSENSE));
  }

  // If the tree is off and USB power is missing → actual shutdown
  if ((analogRead(USB5VSENSE) < 1000) && (strcmp(gSelectedPatternId, "offpattern") == 0)) {
    powerOff();
    serialPrintf( "USB5VSENSE %d --> should power off!\n", analogRead(USB5VSENSE));
  }

  updateAutoBrightness();
}
