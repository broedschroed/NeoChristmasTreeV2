// pattering.ino
// Generic pattern renderer. Reads exclusively from gActivePattern (PatternEngine.h).
// No more direct references to AllPattern.h arrays.

#include "PatternEngine.h"

/*
 * patternwork()
 * Outputs the current animation step (gPatStep) of the loaded pattern
 * to the LED strip. Once all frames have run through, gPatStep wraps back to 0.
 * Repeats the pattern if LED_COUNT > gActivePattern.ledLen.
 */
void patternwork() {
  // If all frames have run through: start over
  if (gPatStep >= gActivePattern.frameCount) {
    gPatStep = 0;
  }

  // Reference the current frame
  const uint8_t* frame = gActivePattern.frames[gPatStep];
  const int ledLen = gActivePattern.ledLen;

  // Set LEDs: pattern is repeated until all LEDs are supplied
  for (int ledPos = 0; ledPos < strip.numPixels(); ledPos++) {
    int patIdx = ledPos % ledLen;          // repeat the pattern
    uint8_t colorIdx = frame[patIdx];      // color index in colordef[]
    strip.setPixelColor(ledPos, strip.Color(
      colordef[colorIdx][0],
      colordef[colorIdx][1],
      colordef[colorIdx][2]
    ));
  }

  gPatStep++;

  // Only output when the IR receiver is idle (prevents interference)
  if (IrReceiver.isIdle()) {
    strip.show();
  }

  // Speed of the pattern step change
  delay(gActivePattern.wait);
}
