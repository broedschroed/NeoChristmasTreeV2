

/*
 * checkIR()
 * Evaluates received IR commands.
 * IR codes   0–7 select the pattern directly by position in gPatternIds.
 * IR code    8: Off (battery: shut down, USB: offpattern).
 * IR code    9: On (loads last saved pattern via getPreferences).
 * IR codes   10/11: brightness darker/brighter.
 * IR code    12: Timer (see timerHandling.ino).
 */
void checkIR() {
  if (IrReceiver.decode()) {
    digitalWrite(STAT_LED, HIGH);
    IrReceiver.printIRResultShort(&Serial);

    // Direct pattern selection via IR codes 0–7
    for (int slot = 0; slot <= 7; slot++) {
      if (IrReceiver.decodedIRData.command == gIRcodes[slot]) {
        int targetIdx = slot;
        if (targetIdx < gPatternCount - 1) {  // -1: skip offpattern
          strncpy(gSelectedPatternId, gPatternIds[targetIdx], 31);
          gSelectedPatternId[31] = '\0';
          gPatternListIndex = targetIdx;
          gNeedPatternReload = true;
          gPatStep = 0;
        }
      }
    }

    // IR code 8: turn off
    if (IrReceiver.decodedIRData.command == gIRcodes[8]) {
      resetTimerState();  // manually turning off makes a running sleep timer moot
      if (analogRead(USB5VSENSE) < 1000) {
        powerOff();
      } else {
        strncpy(gSelectedPatternId, "offpattern", 31);
        gNeedPatternReload = true;
      }
    }

    // IR code 9: turn on
    if (IrReceiver.decodedIRData.command == gIRcodes[9]) {
      resetTimerState();  // manually turning on makes a running sleep timer moot
      getPreferences();
    }

    // IR code 10: darker
    if (IrReceiver.decodedIRData.command == gIRcodes[10]) {
      gBrightness = (gBrightness > 1) ? gBrightness - 2 : 0;
      strip.setBrightness(gBrightness);
      serialPrintf("gBrightness = %d\n", gBrightness);
    }

    // IR code 11: brighter
    if (IrReceiver.decodedIRData.command == gIRcodes[11]) {
      gBrightness = (gBrightness < BRIGHTNESS - 1) ? gBrightness + 2 : BRIGHTNESS;
      strip.setBrightness(gBrightness);
      serialPrintf("gBrightness = %d\n", gBrightness);
    }

    // IR code 12: Timer (increase sleep timer level / cancel at level 8)
    if (IrReceiver.decodedIRData.command == gIRcodes[12]) {
      handleTimerButton();
    }

    IrReceiver.resume();

    if (strcmp(gSelectedPatternId, "offpattern") != 0) {
      writePreferences();
    }

    digitalWrite(STAT_LED, LOW);
  }
}

/*
 * Read IR-codes from Sender sequential and store it in flash
 * The received codes must be thre times the same. This is necessary, because
 * the cheep chinese remote-controls someteimes ar very creative in sending garbage :(
 * The sequence is: pattern 0 to 7, off, on, darker, lighter, timer
 * Only when all codes are learned, they ar stored in flash
 * The first LED in tree indicates, that learning is in progress.
 * Levels 4 to 6 of the tree (all 4 wings) show which code is currently being learned.
 * After all codes are learned the tree starts normal.
 */
void learnIR () {
  unsigned int tempCode;      // memory for received IR-command, used for validating
  
  // first LED indicates learning
  strip.setPixelColor(0,strip.Color(255,0,0));
  strip.show();

  int codepos = 0;            // id of IR-code that is processed for learning
  while (codepos<MAXIRCODES) {
    // if something is received
    if (IrReceiver.decode()) { 
      // validate, that the received code is not the sended rest of the command before
      if (IrReceiver.decodedIRData.command != tempCode) {
        IrReceiver.printIRResultShort(&Serial);               // debugging
        // temporary save the received command
        tempCode = IrReceiver.decodedIRData.command;
        serialPrintf("Try1: %d at Position %d", tempCode, codepos);
        // continue receiving codes
        IrReceiver.resume();
        delay(300);
        // if something is again received
        if (IrReceiver.decode()) {
          serialPrintf("Try1: %d at Position %d", tempCode, codepos);
          IrReceiver.printIRResultShort(&Serial);
          // when it is the same code as before, continue
          if (tempCode == IrReceiver.decodedIRData.command) {
            IrReceiver.resume();
            delay(300);
            if (IrReceiver.decode()) {
             Serial.print("3. Try: ");
             IrReceiver.printIRResultShort(&Serial);
             // if it was three times the same, than the code is validated
             if (tempCode == IrReceiver.decodedIRData.command) {
                serialPrintf("Accepted: %d at Position %d", tempCode, codepos);
                // save the new IR-code an show step an tree
                gIRcodes[codepos] = IrReceiver.decodedIRData.command;

                // Show progress as a full level (4-6) instead of a single LED: 4 groups of
                // 3 codes each (green/cyan/yellow/magenta fill level 4, then 4+5, then 4+5+6);
                // the 13th code (timer) has no group left and is shown fully white.
                int group      = codepos / 3;   // 0=green, 1=cyan, 2=yellow, 3=magenta, 4=timer
                int posInGroup = codepos % 3;   // 0,1,2 -> level 4,5,6
                uint32_t groupColor;
                switch (group) {
                  case 0:  groupColor = strip.Color(0, 255, 0);     break;
                  case 1:  groupColor = strip.Color(0, 255, 255);   break;
                  case 2:  groupColor = strip.Color(255, 255, 0);   break;
                  case 3:  groupColor = strip.Color(255, 0, 255);   break;
                  default: groupColor = strip.Color(255, 255, 255); break;  // timer code
                }

                if (codepos < 12) {
                  if (posInGroup == 0) {
                    // new group: reset the fill of levels 5/6 from the previous group
                    setLevelColor(5, strip.Color(0, 0, 0));
                    setLevelColor(6, strip.Color(0, 0, 0));
                  }
                  setLevelColor(4 + posInGroup, groupColor);
                } else {
                  // 13th code (timer): levels 4-6 together in white
                  setLevelColor(4, groupColor);
                  setLevelColor(5, groupColor);
                  setLevelColor(6, groupColor);
                }
                strip.show();
                delay(500);
                // let head of tree blink, on both tip-LEDs :)
                strip.setPixelColor(64,strip.Color(255,255,255)); strip.setPixelColor(65,strip.Color(255,255,255)); strip.show();
                delay(500);
                strip.setPixelColor(64,strip.Color(0,0,0)); strip.setPixelColor(65,strip.Color(0,0,0)); strip.show();
                delay(500);
                strip.setPixelColor(64,strip.Color(255,255,255)); strip.setPixelColor(65,strip.Color(255,255,255)); strip.show();
                delay(500);
                strip.setPixelColor(64,strip.Color(0,0,0)); strip.setPixelColor(65,strip.Color(0,0,0)); strip.show();
                // learn again the next code
                codepos++;
              }
            }
          }
        }
      }
      IrReceiver.resume();
    }
  }
  // when all codes ar learned, save them in flash
  writePreferences();
}
