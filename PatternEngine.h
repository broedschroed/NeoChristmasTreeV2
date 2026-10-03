// PatternEngine.h
// Shared data structures for pattern rendering, filesystem and webserver.
// Must be the first #include in NeoChristmasTreeV2.ino.

#pragma once

#define MAX_FRAMES  200   // maximum number of frames per pattern
#define MAX_LEDS     66   // maximum number of LEDs (matches LED_COUNT)

// A fully loaded pattern, ready to render.
// Filled by loadPatternById() and read by patternwork().
struct ActivePattern {
  char  name[32];                       // display name, e.g. "Random"
  int   ledLen;                         // number of defined LEDs per frame
  int   wait;                           // wait time in ms between frames
  int   frameCount;                     // actual number of loaded frames
  uint8_t frames[MAX_FRAMES][MAX_LEDS]; // color index values (indices into colordef[])
};

// Global instance – defined in filesystem.ino, extern-visible everywhere
extern ActivePattern gActivePattern;

// Pattern list for IR/button cycling – defined in filesystem.ino
extern char gPatternIds[20][32];
extern char gPatternNames[20][32];
extern int  gPatternCount;
extern int  gPatternListIndex;  // current position for button/IR cycling

// Flags for task communication between webserver task and loop task
extern volatile bool gNeedPatternReload;  // true → loop should call loadPatternById()

// Currently selected pattern ID (defined in NeoChristmasTreeV2.ino)
extern char gSelectedPatternId[32];

// Filesystem functions (defined in filesystem.ino)
void initFilesystem();
void loadPatternIndex();
void loadPatternById(const char* id);
bool savePattern(const char* id, const char* jsonBody);
bool resetPatternToDefault(const char* id);
void migrateDefaultPatterns();
bool updateIndexFromFile(const char* id);
