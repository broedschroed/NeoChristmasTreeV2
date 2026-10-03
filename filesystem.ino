// filesystem.ino
// LittleFS initialization, pattern index management and JSON serialization.
// Defines all extern symbols from PatternEngine.h.

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "PatternEngine.h"

// === Global definitions (declared extern in PatternEngine.h) ===
ActivePattern gActivePattern;

char gPatternIds[20][32];
char gPatternNames[20][32];
int  gPatternCount     = 0;
int  gPatternListIndex = 0;

// -------------------------------------------------------------------------
// initFilesystem()
// Mounts LittleFS. On failure, formats and retries.
// Calls migrateDefaultPatterns() if the index is missing.
// -------------------------------------------------------------------------
void initFilesystem() {
  if (!LittleFS.begin(false)) {
    Serial.println("[FS] Mount fehlgeschlagen, formatiere...");
    LittleFS.format();
    if (!LittleFS.begin(false)) {
      Serial.println("[FS] KRITISCH: LittleFS nicht mountbar!");
      return;
    }
  }
  Serial.println("[FS] LittleFS gemountet");

  if (!LittleFS.exists("/patterns/_index.json")) {
    Serial.println("[FS] Kein Pattern-Index gefunden, starte Migration...");
    migrateDefaultPatterns();
  }
}

// -------------------------------------------------------------------------
// loadPatternIndex()
// Reads /patterns/_index.json and fills gPatternIds/gPatternNames/gPatternCount.
// -------------------------------------------------------------------------
void loadPatternIndex() {
  gPatternCount = 0;

  File f = LittleFS.open("/patterns/_index.json", "r");
  if (!f) {
    Serial.println("[FS] _index.json nicht gefunden");
    return;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[FS] JSON-Fehler im Index: %s\n", err.c_str());
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  int count = 0;
  for (JsonObject entry : arr) {
    if (count >= 20) break;
    strncpy(gPatternIds[count],   entry["id"]   | "unknown", 31);
    strncpy(gPatternNames[count], entry["name"] | "?",       31);
    gPatternIds[count][31]   = '\0';
    gPatternNames[count][31] = '\0';
    count++;
  }
  gPatternCount = count;
  Serial.printf("[FS] %d Pattern geladen\n", gPatternCount);

  // Set gPatternListIndex to the current gSelectedPatternId
  for (int i = 0; i < gPatternCount; i++) {
    if (strcmp(gPatternIds[i], gSelectedPatternId) == 0) {
      gPatternListIndex = i;
      break;
    }
  }
}

// -------------------------------------------------------------------------
// loadPatternById()
// Reads /patterns/{id}.json and fills gActivePattern.
// On failure, gActivePattern is set to "all black" (fallback).
// -------------------------------------------------------------------------
void loadPatternById(const char* id) {
  char path[48];
  snprintf(path, sizeof(path), "/patterns/%s.json", id);

  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[FS] Pattern nicht gefunden: %s\n", path);
    strncpy(gActivePattern.name, "off", 31);
    gActivePattern.ledLen     = MAX_LEDS;
    gActivePattern.wait       = 100;
    gActivePattern.frameCount = 1;
    memset(gActivePattern.frames[0], 0, MAX_LEDS);
    return;
  }

  // Parse metadata via filter (stack-only, no heap)
  StaticJsonDocument<64> filter;
  filter["name"]   = true;
  filter["ledLen"] = true;
  filter["wait"]   = true;

  StaticJsonDocument<256> meta;
  DeserializationError err = deserializeJson(meta, f, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[FS] Metadaten-Fehler in %s: %s\n", path, err.c_str());
    f.close();
    return;
  }
  strncpy(gActivePattern.name, meta["name"] | id, 31);
  gActivePattern.name[31] = '\0';
  gActivePattern.ledLen   = meta["ledLen"] | MAX_LEDS;
  gActivePattern.wait     = meta["wait"]   | 100;

  // Parse the frames array manually from the stream (no ArduinoJson, no heap)
  f.seek(0);
  const char* framesKey = "\"frames\":[";
  int ki = 0;
  int c;
  while ((c = f.read()) != -1) {
    if (c == framesKey[ki]) {
      ki++;
      if (framesKey[ki] == '\0') break;
    } else {
      ki = (c == framesKey[0]) ? 1 : 0;
    }
  }

  int fc = 0;
  while (fc < MAX_FRAMES && (c = f.read()) != -1) {
    if (c == '[') {
      int li = 0;
      c = f.read();
      while (c != ']' && c != -1) {
        if (c >= '0' && c <= '9') {
          int val = c - '0';
          while ((c = f.read()) >= '0' && c <= '9')
            val = val * 10 + (c - '0');
          if (li < MAX_LEDS) gActivePattern.frames[fc][li++] = (uint8_t)val;
        } else {
          c = f.read();
        }
      }
      for (; li < MAX_LEDS; li++) gActivePattern.frames[fc][li] = 0;
      fc++;
    } else if (c == ']') {
      break;
    }
  }
  gActivePattern.frameCount = fc;
  f.close();

  Serial.printf("[FS] Pattern geladen: %s (%d Frames, ledLen=%d, wait=%d)\n",
                gActivePattern.name, fc, gActivePattern.ledLen, gActivePattern.wait);
}

// -------------------------------------------------------------------------
// savePattern()
// Writes a pattern from the JSON body to /patterns/{id}.json
// and updates /patterns/_index.json.
// Returns: true on success, false on failure.
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// updateIndexFromFile()
// Reads the name from an already saved pattern file and
// updates /patterns/_index.json. Called after a streaming upload.
// -------------------------------------------------------------------------
bool updateIndexFromFile(const char* id) {
  char path[48];
  snprintf(path, sizeof(path), "/patterns/%s.json", id);

  // Read name from file – only the name field, no heap
  StaticJsonDocument<32> filter;
  filter["name"] = true;
  StaticJsonDocument<128> meta;
  File fi = LittleFS.open(path, "r");
  if (!fi) return false;
  deserializeJson(meta, fi, DeserializationOption::Filter(filter));
  fi.close();
  const char* name = meta["name"] | id;

  // Load and update index
  DynamicJsonDocument idxDoc(2048);
  File fidx = LittleFS.open("/patterns/_index.json", "r");
  if (fidx) { deserializeJson(idxDoc, fidx); fidx.close(); }

  JsonArray arr;
  if (idxDoc.is<JsonArray>()) arr = idxDoc.as<JsonArray>();
  else                        arr = idxDoc.to<JsonArray>();

  bool found = false;
  for (JsonObject entry : arr) {
    if (strcmp(entry["id"] | "", id) == 0) {
      entry["name"] = name;
      found = true;
      break;
    }
  }
  if (!found) {
    JsonObject newEntry = arr.createNestedObject();
    newEntry["id"]   = id;
    newEntry["name"] = name;
  }

  File fo = LittleFS.open("/patterns/_index.json", "w");
  if (!fo) return false;
  serializeJson(idxDoc, fo);
  fo.close();

  loadPatternIndex();
  Serial.printf("[FS] Index aktualisiert: %s (%s)\n", id, name);
  return true;
}

bool savePattern(const char* id, const char* jsonBody) {
  // 1) Validate JSON
  DynamicJsonDocument doc(65536);
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    Serial.printf("[FS] savePattern: ungültiges JSON: %s\n", err.c_str());
    return false;
  }
  const char* name = doc["name"] | id;

  // 2) Write pattern file
  char path[48];
  snprintf(path, sizeof(path), "/patterns/%s.json", id);

  File f = LittleFS.open(path, "w");
  if (!f) {
    Serial.printf("[FS] savePattern: kann nicht schreiben: %s\n", path);
    return false;
  }
  f.print(jsonBody);
  f.close();

  // 3) Read and update index file
  DynamicJsonDocument idxDoc(2048);
  File fi = LittleFS.open("/patterns/_index.json", "r");
  if (fi) {
    deserializeJson(idxDoc, fi);
    fi.close();
  }

  JsonArray arr;
  if (idxDoc.is<JsonArray>()) {
    arr = idxDoc.as<JsonArray>();
  } else {
    arr = idxDoc.to<JsonArray>();
  }

  // Check whether the id already exists
  bool found = false;
  for (JsonObject entry : arr) {
    if (strcmp(entry["id"] | "", id) == 0) {
      entry["name"] = name;
      found = true;
      break;
    }
  }
  if (!found) {
    JsonObject newEntry = arr.createNestedObject();
    newEntry["id"]   = id;
    newEntry["name"] = name;
  }

  // 4) Write index back
  File fo = LittleFS.open("/patterns/_index.json", "w");
  if (!fo) {
    Serial.println("[FS] savePattern: Index nicht schreibbar");
    return false;
  }
  serializeJson(idxDoc, fo);
  fo.close();

  // Update local list
  loadPatternIndex();
  Serial.printf("[FS] Pattern gespeichert: %s\n", id);
  return true;
}

// -------------------------------------------------------------------------
// resetPatternToDefault()
// Writes the original pattern from AllPattern.h back to LittleFS.
// Uses the same mapping table as migrateDefaultPatternsAlt().
// Returns: true on success, false if the ID is not among the default patterns.
// -------------------------------------------------------------------------
bool resetPatternToDefault(const char* id) {
  struct PatInfo {
    const char* id;
    const char* name;
    int index;
    int* data;
  };

  PatInfo infos[] = {
    {"randompattern",      "Random",       0, (int*)randompattern},
    {"cdlpattern",         "CDL",          1, (int*)cdlpattern},
    {"blinkpattern",       "Blink",        2, (int*)blinkpattern},
    {"colorscrollpattern", "Color Scroll", 3, (int*)colorscrollpattern},
    {"colorlinespattern",  "Color Lines",  4, (int*)colorlinespattern},
    {"colorsidespattern",  "Color Sides",  5, (int*)colorsidespattern},
    {"treesnowpattern",    "Tree Snow",    6, (int*)treesnowpattern},
    {"wblinkpattern",      "WBlink",       7, (int*)wblinkpattern},
    {"offpattern",         "Off",          8, (int*)offpattern},
  };

  for (auto& info : infos) {
    if (strcmp(info.id, id) != 0) continue;

    int ledLen    = LEDLENS[info.index];
    int numFrames = LENS[info.index];
    int wait      = WAITS[info.index];

    char path[48];
    snprintf(path, sizeof(path), "/patterns/%s.json", id);
    File f = LittleFS.open(path, "w");
    if (!f) {
      Serial.printf("[FS] resetPatternToDefault: Schreibfehler: %s\n", path);
      return false;
    }

    f.printf("{\"name\":\"%s\",\"ledLen\":%d,\"wait\":%d,\"frames\":[", info.name, ledLen, wait);
    for (int fr = 0; fr < numFrames; fr++) {
      f.print("[");
      for (int li = 0; li < ledLen; li++) {
        f.print(info.data[fr * ledLen + li]);
        if (li < ledLen - 1) f.print(",");
      }
      f.print("]");
      if (fr < numFrames - 1) f.print(",");
    }
    f.print("]}");
    f.close();

    Serial.printf("[FS] Original wiederhergestellt: %s\n", id);
    return true;
  }

  Serial.printf("[FS] resetPatternToDefault: ID unbekannt: %s\n", id);
  return false;
}

// -------------------------------------------------------------------------
// migrateDefaultPatterns()
// Called on first boot when /patterns/_index.json is missing.
// Writes all default patterns from AllPattern.h as JSON files to LittleFS.
// Uses the LEDLENS[], LENS[], WAITS[] arrays from AllPattern.h.
// -------------------------------------------------------------------------
void migrateDefaultPatterns() {
  LittleFS.mkdir("/patterns");
  LittleFS.mkdir("/www");

  struct PatInfo {
    const char* id;
    const char* name;
    int index;
    int* data;
  };

  PatInfo infos[] = {
    {"randompattern",      "Random",       0, (int*)randompattern},
    {"cdlpattern",         "CDL",          1, (int*)cdlpattern},
    {"blinkpattern",       "Blink",        2, (int*)blinkpattern},
    {"colorscrollpattern", "Color Scroll", 3, (int*)colorscrollpattern},
    {"colorlinespattern",  "Color Lines",  4, (int*)colorlinespattern},
    {"colorsidespattern",  "Color Sides",  5, (int*)colorsidespattern},
    {"treesnowpattern",    "Tree Snow",    6, (int*)treesnowpattern},
    {"wblinkpattern",      "WBlink",       7, (int*)wblinkpattern},
    {"offpattern",         "Off",          8, (int*)offpattern},
  };

  for (auto& info : infos) {
    int ledLen    = LEDLENS[info.index];
    int numFrames = LENS[info.index];
    int wait      = WAITS[info.index];

    char path[48];
    snprintf(path, sizeof(path), "/patterns/%s.json", info.id);
    File f = LittleFS.open(path, "w");
    if (!f) { Serial.printf("[MIG] Fehler: %s\n", path); continue; }

    f.printf("{\"name\":\"%s\",\"ledLen\":%d,\"wait\":%d,\"frames\":[", info.name, ledLen, wait);
    for (int fr = 0; fr < numFrames; fr++) {
      f.print("[");
      for (int li = 0; li < ledLen; li++) {
        f.print(info.data[fr * ledLen + li]);
        if (li < ledLen - 1) f.print(",");
      }
      f.print("]");
      if (fr < numFrames - 1) f.print(",");
    }
    f.print("]}");
    f.close();
    Serial.printf("[MIG] %s geschrieben\n", info.id);
  }

  // Write index file
  File fi = LittleFS.open("/patterns/_index.json", "w");
  if (fi) {
    fi.print("["
      "{\"id\":\"randompattern\",\"name\":\"Random\"},"
      "{\"id\":\"cdlpattern\",\"name\":\"CDL\"},"
      "{\"id\":\"blinkpattern\",\"name\":\"Blink\"},"
      "{\"id\":\"colorscrollpattern\",\"name\":\"Color Scroll\"},"
      "{\"id\":\"colorlinespattern\",\"name\":\"Color Lines\"},"
      "{\"id\":\"colorsidespattern\",\"name\":\"Color Sides\"},"
      "{\"id\":\"treesnowpattern\",\"name\":\"Tree Snow\"},"
      "{\"id\":\"wblinkpattern\",\"name\":\"WBlink\"},"
      "{\"id\":\"offpattern\",\"name\":\"Off\"}"
    "]");
    fi.close();
    Serial.println("[MIG] Index geschrieben");
  }

  Serial.println("[MIG] Migration abgeschlossen");
}
