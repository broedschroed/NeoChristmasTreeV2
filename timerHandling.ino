// timerHandling.ino
// Sleep timer via the remote's "Timer" button (gIRcodes[12]).
// Each button press increases the level (1-8, corresponding to 30 minutes to 4 hours
// of automatic shutdown time) and shows the matching "level" of the tree for 3 seconds
// as confirmation. At level 8 (all levels, 4 hours), another press immediately cancels
// the timer and the tree returns to normal operation without delay.
//
// Level concept: The tree consists of 4 blocks of 16 LEDs each (2 plugged-together
// boards x front/back, see ki-dialog). Within each block, position 1 (lowest LED) runs
// up to position 8 (near the tip) and mirrors back down to position 16 on the opposite
// edge. Level L (1..8) therefore combines position L and (17-L) in each of the 4 blocks
// -> level 1 = the 8 lowest LEDs (2 per wing), level 8 = all 64 main LEDs.
// The two tip LEDs (index 64/65) belong to no level.

#define TIMER_DEBOUNCE_MS     500   // minimum gap between two accepted timer presses

unsigned long gTimerLastPressAt = 0;  // millis() of the last accepted timer button press

/*
 * levelOfLed()
 * Returns the level (1..8, 1 = lowest LED) of a main LED (index 0..63).
 * Returns 0 for the tip LEDs (index 64/65) and invalid indices.
 */
int levelOfLed(int ledIdx0) {
  if (ledIdx0 < 0 || ledIdx0 >= 64) return 0;
  int posInBlock = (ledIdx0 % 16) + 1;               // 1..16 position within the 16-LED block
  return (posInBlock <= 8) ? posInBlock : (17 - posInBlock);
}

/*
 * setLevelColor()
 * Sets all LEDs of a single level (1..8) to a color; all other LEDs are left
 * unchanged. Does not call strip.show().
 */
void setLevelColor(int level, uint32_t color) {
  for (int i = 0; i < 64; i++) {
    if (levelOfLed(i) == level) strip.setPixelColor(i, color);
  }
}

/*
 * renderLevelsUpTo()
 * Shows all levels up to and including "level" (1..8) in one color; the rest of the
 * tree (incl. tip LEDs) stays dark. Used for the timer preview.
 */
void renderLevelsUpTo(int level, uint32_t color) {
  for (int i = 0; i < strip.numPixels(); i++) {
    int lvl = levelOfLed(i);
    strip.setPixelColor(i, (lvl > 0 && lvl <= level) ? color : strip.Color(0, 0, 0));
  }
  strip.show();
}

/*
 * resetTimerState()
 * Resets the sleep timer without forcing a pattern reload. Used when the tree is
 * being manually turned on/off anyway.
 */
void resetTimerState() {
  gTimerLevel         = 0;
  gTimerPreviewActive = false;
  gTimerOffAt          = 0;
}

/*
 * cancelTimer()
 * Cancels a running sleep timer (level 8 pressed again) and immediately shows the
 * previously active pattern again.
 */
void cancelTimer() {
  resetTimerState();
  gNeedPatternReload = true;
}

/*
 * handleTimerButton()
 * Called by checkIR() when the Timer button (gIRcodes[12]) is detected.
 * Cheap remotes often send several IR frames in a row per button press; without
 * debouncing this would increase the level by more than 1 per press. So every
 * further timer code is ignored here for TIMER_DEBOUNCE_MS.
 */
void handleTimerButton() {
  unsigned long now = millis();
  if ((long)(now - gTimerLastPressAt) < TIMER_DEBOUNCE_MS) return;
  gTimerLastPressAt = now;

  if (gTimerLevel >= TIMER_MAX_LEVEL) {
    cancelTimer();
    serialPrintf("Timer abgebrochen, Normalbetrieb\n");
    return;
  }

  gTimerLevel++;
  gTimerPreviewActive = true;
  gTimerPreviewUntil  = now + TIMER_PREVIEW_MS;
  gTimerOffAt          = now + (unsigned long)gTimerLevel * TIMER_STEP_MS;

  renderLevelsUpTo(gTimerLevel, strip.Color(255, 160, 0));  // gold: timer preview
  serialPrintf("Timer-Stufe %d, Abschaltung in %lu Minuten\n", gTimerLevel, gTimerLevel * 30UL);
}

/*
 * updateTimerState()
 * Called on every loop() iteration: ends the 3s preview (patternwork() takes over
 * again afterwards) or, once due, triggers the automatic shutdown (same mechanism
 * as the OFF button in checkIR()).
 */
void updateTimerState() {
  if (gTimerLevel == 0) return;

  if (gTimerPreviewActive) {
    if ((long)(millis() - gTimerPreviewUntil) >= 0) {
      gTimerPreviewActive = false;
      gNeedPatternReload  = true;
    }
    return;
  }

  if ((long)(millis() - gTimerOffAt) >= 0) {
    if (analogRead(USB5VSENSE) < 1000) {
      powerOff();
    } else {
      strncpy(gSelectedPatternId, "offpattern", 31);
      gNeedPatternReload = true;
    }
    resetTimerState();
  }
}
