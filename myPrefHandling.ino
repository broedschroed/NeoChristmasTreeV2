// myPrefHandling.ino
// Loading and saving IR codes, pattern ID and brightness in the NVS preferences.
// Instead of the integer pattern index, the string ID key "IRp" is now used.

/*
 * getPreferences()
 * Loads IR codes, last saved pattern ID and brightness from flash.
 */
void getPreferences() {
  preferences.begin("ircodes", false);

  gIRcodes[0]  = preferences.getUInt("IR1", defaultIRcodes[0]);
  gIRcodes[1]  = preferences.getUInt("IR2", defaultIRcodes[1]);
  gIRcodes[2]  = preferences.getUInt("IR3", defaultIRcodes[2]);
  gIRcodes[3]  = preferences.getUInt("IR4", defaultIRcodes[3]);
  gIRcodes[4]  = preferences.getUInt("IR5", defaultIRcodes[4]);
  gIRcodes[5]  = preferences.getUInt("IR6", defaultIRcodes[5]);
  gIRcodes[6]  = preferences.getUInt("IR7", defaultIRcodes[6]);
  gIRcodes[7]  = preferences.getUInt("IR8", defaultIRcodes[7]);
  gIRcodes[8]  = preferences.getUInt("IR9", defaultIRcodes[8]);
  gIRcodes[9]  = preferences.getUInt("IR0", defaultIRcodes[9]);
  gIRcodes[10] = preferences.getUInt("IRd", defaultIRcodes[10]);
  gIRcodes[11] = preferences.getUInt("IRl", defaultIRcodes[11]);

  // Load pattern ID as string (key "IRp"), fallback "randompattern"
  String savedId = preferences.getString("IRp", "randompattern");
  strncpy(gSelectedPatternId, savedId.c_str(), 31);
  gSelectedPatternId[31] = '\0';

  gBrightness = preferences.getUInt("gBr", BRIGHTNESS);
  gPatStep    = 0;
  gNeedPatternReload = true;
}

/*
 * writePreferences()
 * Saves IR codes, current pattern ID and brightness.
 */
void writePreferences() {
  preferences.putUInt("IR1", gIRcodes[0]);
  preferences.putUInt("IR2", gIRcodes[1]);
  preferences.putUInt("IR3", gIRcodes[2]);
  preferences.putUInt("IR4", gIRcodes[3]);
  preferences.putUInt("IR5", gIRcodes[4]);
  preferences.putUInt("IR6", gIRcodes[5]);
  preferences.putUInt("IR7", gIRcodes[6]);
  preferences.putUInt("IR8", gIRcodes[7]);
  preferences.putUInt("IR9", gIRcodes[8]);
  preferences.putUInt("IR0", gIRcodes[9]);
  preferences.putUInt("IRd", gIRcodes[10]);
  preferences.putUInt("IRl", gIRcodes[11]);

  // Save pattern ID as string
  preferences.putString("IRp", gSelectedPatternId);
  preferences.putUInt("gBr", gBrightness);
}
