# WiFi AP + Web-basiertes Pattern-Management — Implementierungsplan

> **Für agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Ziel:** Wenn USB-Spannung erkannt wird (`analogRead(USB5VSENSE) >= 1000`), öffnet der ESP32-S3 einen WiFi Access Point (SSID: `ChristmasTree`, kein Passwort). Ein Webserver liefert den modifizierten Pattern-Editor aus LittleFS. Muster werden als JSON in LittleFS gespeichert. Eine REST-API ermöglicht dem Browser das Laden, Speichern und Zurücksetzen von Mustern auf das Original (AllPattern.h) sowie die Steuerung des aktiven Musters und der Helligkeit.

**Architektur:** ESPAsyncWebServer auf FreeRTOS-Task 0, LED-Animation auf Task 1 (Arduino-Loop). Gemeinsame Variablen sind `volatile`. LittleFS für HTML und Pattern-JSON. Erste Inbetriebnahme migriert Muster aus `AllPattern.h`.

**Tech Stack:** ESP32-S3 Arduino, ESPAsyncWebServer + AsyncTCP (GitHub: me-no-dev), ArduinoJson 7.x (Library Manager), LittleFS (built-in), Adafruit NeoPixel, IRremote

---

## File Map

| Datei | Status | Verantwortlichkeit |
|---|---|---|
| `PatternEngine.h` | **Neu** | Struct `ActivePattern`, Konstanten `MAX_FRAMES`, `MAX_LEDS` |
| `filesystem.ino` | **Neu** | LittleFS-Init, `loadPatternIndex()`, `loadPatternById()`, `savePattern()`, `resetPatternToDefault()`, `migrateDefaultPatterns()` |
| `webserver.ino` | **Neu** | WiFi-AP-Setup, ESPAsyncWebServer, alle REST-Routen inkl. `POST /api/reset` |
| `data/www/index.html` | **Neu** | Modifizierter Pattern-Editor mit API-Modus |
| `NeoChristmasTreeV2.ino` | **Geändert** | Neue Globals, bedingtes WiFi, `gSelectedPatternId` statt `gSelectedPattern` |
| `pattering.ino` | **Geändert** | Generischer Renderer mit `ActivePattern`-Struct |
| `myPrefHandling.ino` | **Geändert** | `gSelectedPatternId` als String speichern/laden (Key `"IRp"`) |
| `IRhandling.ino` | **Geändert** | `gPatternListIndex` für Cycling, `gSelectedPatternId` für Direktwahl |
| `AllPattern.h` | **Unverändert** | Nur für Migration bei Erststart |
| `IRcodes.h` | **Unverändert** | IR-Code-Konstanten |
| `serialprintf.ino` | **Unverändert** | Debug-Hilfsfunktion |

---

## REST-API

| Methode | Endpunkt | Body | Antwort | Beschreibung |
|---|---|---|---|---|
| `GET` | `/api/patterns` | — | `[{"id":"randompattern","name":"Random"},...]` | Pattern-Index laden |
| `GET` | `/api/pattern?id=X` | — | `{"name":"Random","ledLen":16,"wait":100,"frames":[[...],...]}` | Einzelnes Pattern laden |
| `POST` | `/api/pattern?id=X` | Pattern-JSON | `{"ok":true}` | Pattern speichern / anlegen |
| `POST` | `/api/reset?id=X` | — | `{"ok":true}` | Pattern auf Original (AllPattern.h) zurücksetzen |
| `GET` | `/api/state` | — | `{"activeId":"randompattern","brightness":20}` | Aktuellen Zustand abfragen |
| `POST` | `/api/active` | `{"id":"randompattern"}` | `{"ok":true}` | Aktives Pattern wechseln |
| `POST` | `/api/brightness` | `{"value":20}` | `{"ok":true}` | Helligkeit setzen |

---

## Partition Scheme

In der Arduino IDE muss für das Board `ESP32S3 Dev Module` das Partition Scheme auf **"No OTA (2MB APP/2MB LittleFS)"** gesetzt werden, bevor das erste Mal geflasht wird. Ansonsten fehlt der LittleFS-Bereich.

---

## Task 1 — Bibliotheken installieren und Partition Scheme setzen

**Dateien:** Keine Codeänderungen — nur Arduino-IDE-Einstellungen und Bibliotheksmanager

- [ ] In der Arduino IDE unter `Tools → Manage Libraries` nach `ArduinoJson` suchen und Version **7.x** von Benoit Blanchon installieren
- [ ] ESPAsyncWebServer und AsyncTCP aus GitHub installieren:
  - `https://github.com/me-no-dev/AsyncTCP` → als ZIP herunterladen → `Sketch → Include Library → Add .ZIP Library`
  - `https://github.com/me-no-dev/ESPAsyncWebServer` → ebenso als ZIP
- [ ] In der Arduino IDE unter `Tools → Board` das Board `ESP32S3 Dev Module` (oder das tatsächlich verwendete S3-Board) auswählen
- [ ] Unter `Tools → Partition Scheme` den Eintrag **"No OTA (2MB APP/2MB LittleFS)"** auswählen
- [ ] Unter `Tools → Flash Size` sicherstellen, dass **4MB** eingestellt ist (passend zur 4-MB-Variante des ESP32-S3-Moduls)
- [ ] Sicherstellen, dass das Plugin `arduino-littlefs-upload` installiert ist, damit das `data/`-Verzeichnis per `Tools → Upload LittleFS Image to ESP32` hochgeladen werden kann (Download: `https://github.com/earlephilhower/arduino-littlefs-upload/releases`)

**Verifikation:** Arduino IDE zeigt nach der Bibliotheksinstallation keinen Fehler bei `#include <ESPAsyncWebServer.h>` und `#include <ArduinoJson.h>`. Der Menüpunkt `Tools → Upload LittleFS Image to ESP32` ist sichtbar.

---

## Task 2 — `PatternEngine.h` anlegen

**Dateien:** Neu: `PatternEngine.h`

Diese Header-Datei definiert alle gemeinsamen Datenstrukturen zwischen `filesystem.ino`, `webserver.ino` und `pattering.ino`. Sie muss **als erste** angelegt werden, da alle folgenden Tasks darauf aufbauen.

- [ ] Datei `PatternEngine.h` im Sketch-Verzeichnis anlegen:

```cpp
// PatternEngine.h
// Gemeinsame Datenstrukturen für Pattern-Rendering, Filesystem und Webserver.
// Muss als erstes #include in NeoChristmasTreeV2.ino stehen.

#pragma once

#define MAX_FRAMES  200   // maximale Anzahl Frames pro Pattern
#define MAX_LEDS     66   // maximale Anzahl LEDs (entspricht LED_COUNT)

// Ein vollständig geladenes Pattern, bereit zum Rendern.
// Wird von loadPatternById() befüllt und von patternwork() gelesen.
struct ActivePattern {
  char  name[32];                       // Anzeigename, z.B. "Random"
  int   ledLen;                         // Anzahl definierter LEDs pro Frame
  int   wait;                           // Wartezeit in ms zwischen Frames
  int   frameCount;                     // tatsächliche Anzahl geladener Frames
  uint8_t frames[MAX_FRAMES][MAX_LEDS]; // Farbindex-Werte (Indizes in colordef[])
};

// Globale Instanz – in filesystem.ino definiert, extern überall sichtbar
extern ActivePattern gActivePattern;

// Pattern-Liste für IR/Button-Cycling – in filesystem.ino definiert
extern char gPatternIds[20][32];
extern char gPatternNames[20][32];
extern int  gPatternCount;
extern int  gPatternListIndex;  // aktuelle Position für Button/IR-Cycling

// Flags für Task-Kommunikation zwischen Webserver-Task und Loop-Task
extern volatile bool gNeedPatternReload;  // true → Loop soll loadPatternById() aufrufen

// Aktuell gewähltes Pattern-ID (in NeoChristmasTreeV2.ino definiert)
extern char gSelectedPatternId[32];
```

- [ ] Datei speichern

**Verifikation:** Arduino kompiliert ohne Fehler, wenn `#include "PatternEngine.h"` in `NeoChristmasTreeV2.ino` steht (Test nach Task 3).

---

## Task 3 — `NeoChristmasTreeV2.ino` anpassen: neue Globals und bedingtes WiFi

**Dateien:** Geändert: `NeoChristmasTreeV2.ino`

- [ ] `#include "AllPattern.h"` **bleibt** bestehen (wird für `migrateDefaultPatterns()` benötigt)
- [ ] `#include <WiFi.h>` bleibt bestehen (wird nun aktiv für AP-Modus genutzt)
- [ ] `#include "PatternEngine.h"` als neues Include hinzufügen — **direkt nach den bestehenden Includes**, vor allen anderen Deklarationen

Die gesamte `// ---- GLOBAL ----`-Sektion wie folgt ersetzen:

```cpp
// ---- GLOBAL ----
Preferences preferences;                  // speichert IR-Codes, Pattern-ID und Helligkeit
int gPatStep = 0;                         // aktueller Animationsschritt des gewählten Patterns

// Pattern-ID als String statt int-Index
char gSelectedPatternId[32] = "offpattern"; // aktives Pattern; "offpattern" = Baum aus

// Shared-State-Flags (volatile für Task-sicheren Zugriff)
volatile bool gNeedPatternReload = true;  // true → Loop lädt Pattern neu aus LittleFS

Adafruit_NeoPixel strip(LED_COUNT,
                        LED_PIN,
                        NEO_GRB + NEO_KHZ800);
int gBrightness = BRIGHTNESS;             // aktuell gewählte Helligkeit
unsigned int gIRcodes[12];               // 12 IR-Codes: Pattern 0–7, Off, On, Dunkler, Heller
```

- [ ] Die alte Zeile `int gSelectedPattern = 0;` **entfernen**

Im `setup()` den WiFi-Block ersetzen. Der bisherige Block lautet:
```cpp
  WiFi.mode(WIFI_OFF);
  btStop();
```

Ersetzen durch:
```cpp
  btStop();  // Bluetooth immer ausschalten, spart Strom
  // WiFi wird NICHT hier deaktiviert — setupWiFi() in webserver.ino
  // entscheidet anhand USB5VSENSE, ob AP gestartet oder WiFi abgeschaltet wird
```

Am Ende des `setup()`, **nach** `IrReceiver.begin(...)` und vor der schließenden `}`, folgende Zeilen hinzufügen:

```cpp
  // LittleFS initialisieren und ggf. Standardmuster migrieren
  initFilesystem();

  // Pattern-Index aus LittleFS laden (befüllt gPatternIds/gPatternNames/gPatternCount)
  loadPatternIndex();

  // WiFi AP starten wenn USB-Spannung anliegt, sonst WiFi aus
  setupWiFi();

  // Erstes Pattern laden
  loadPatternById(gSelectedPatternId);
  gNeedPatternReload = false;
```

Im `loop()` den Aufruf `patternwork(gSelectedPattern)` ersetzen durch:

```cpp
  // Falls Webserver oder IR ein neues Pattern angefordert hat, neu laden
  if (gNeedPatternReload) {
    loadPatternById(gSelectedPatternId);
    gNeedPatternReload = false;
    gPatStep = 0;
  }

  // Aktuellen Animationsschritt des geladenen Patterns auf LEDs ausgeben
  patternwork();
```

Den bisherigen Button-Cycling-Block ersetzen:

```cpp
  // Wenn Learn/Mode-Schalter gedrückt: zum nächsten Pattern wechseln und in Flash speichern
  if (!digitalRead(SWLEARN)) {
    digitalWrite(STAT_LED, HIGH);
    delay(100);
    while (!digitalRead(SWLEARN)) {}
    digitalWrite(STAT_LED, LOW);

    // Vorwärts durch die Pattern-Liste (ohne offpattern, das ist immer letztes Element)
    int nextIdx = gPatternListIndex + 1;
    if (nextIdx >= gPatternCount - 1) nextIdx = 0;  // -1: offpattern überspringen
    gPatternListIndex = nextIdx;
    strncpy(gSelectedPatternId, gPatternIds[nextIdx], 31);
    gSelectedPatternId[31] = '\0';
    gNeedPatternReload = true;
    gPatStep = 0;
    writePreferences();
  }
```

Den bisherigen Power-Button-Block ersetzen:

```cpp
  // Power-Button: toggelt zwischen offpattern und zuletzt gespeichertem Pattern
  if (analogRead(PWRSWSENSE) < 500) {
    if (strcmp(gSelectedPatternId, "offpattern") != 0) {
      strncpy(gSelectedPatternId, "offpattern", 31);
      gNeedPatternReload = true;
    } else {
      getPreferences();  // lädt zuletzt gespeichertes Pattern in gSelectedPatternId
      gNeedPatternReload = true;
    }
    delay(1000);  // Entprellung
  }
```

Den bisherigen Shutdown-Check ersetzen:

```cpp
  // Wenn Baum im Off-Zustand ist und USB-Strom fehlt → echtes Abschalten
  if ((analogRead(USB5VSENSE) < 1000) && (strcmp(gSelectedPatternId, "offpattern") == 0)) {
    powerOff();
  }
```

- [ ] Datei speichern

**Verifikation:** Arduino kompiliert ohne Fehler (auch wenn `filesystem.ino` und `webserver.ino` noch leer sind, solange die Funktions-Prototypen bekannt sind — daher erst nach Task 4 und Task 5 vollständig testen).

---

## Task 4 — `filesystem.ino` anlegen

**Dateien:** Neu: `filesystem.ino`

Diese Datei enthält alle LittleFS-Operationen sowie die Pattern-Liste. Sie definiert die `extern`-Symbole aus `PatternEngine.h`.

- [ ] Datei `filesystem.ino` im Sketch-Verzeichnis anlegen:

```cpp
// filesystem.ino
// LittleFS-Initialisierung, Pattern-Index-Verwaltung und JSON-Serialisierung.
// Definiert alle extern-Symbole aus PatternEngine.h.

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "PatternEngine.h"

// === Globale Definitionen (in PatternEngine.h als extern deklariert) ===
ActivePattern gActivePattern;

char gPatternIds[20][32];
char gPatternNames[20][32];
int  gPatternCount     = 0;
int  gPatternListIndex = 0;

// -------------------------------------------------------------------------
// initFilesystem()
// Mountet LittleFS. Bei Fehler wird formatiert und neu versucht.
// Ruft bei fehlendem Index migrateDefaultPatterns() auf.
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
// Liest /patterns/_index.json und befüllt gPatternIds/gPatternNames/gPatternCount.
// -------------------------------------------------------------------------
void loadPatternIndex() {
  gPatternCount = 0;

  File f = LittleFS.open("/patterns/_index.json", "r");
  if (!f) {
    Serial.println("[FS] _index.json nicht gefunden");
    return;
  }

  JsonDocument doc;
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

  // gPatternListIndex auf das aktuelle gSelectedPatternId setzen
  for (int i = 0; i < gPatternCount; i++) {
    if (strcmp(gPatternIds[i], gSelectedPatternId) == 0) {
      gPatternListIndex = i;
      break;
    }
  }
}

// -------------------------------------------------------------------------
// loadPatternById()
// Liest /patterns/{id}.json und befüllt gActivePattern.
// Bei Fehler wird gActivePattern auf "alles schwarz" gesetzt (Fallback).
// -------------------------------------------------------------------------
void loadPatternById(const char* id) {
  char path[48];
  snprintf(path, sizeof(path), "/patterns/%s.json", id);

  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[FS] Pattern nicht gefunden: %s\n", path);
    // Fallback: alles schwarz, 1 Frame
    strncpy(gActivePattern.name, "off", 31);
    gActivePattern.ledLen     = MAX_LEDS;
    gActivePattern.wait       = 100;
    gActivePattern.frameCount = 1;
    memset(gActivePattern.frames[0], 0, MAX_LEDS);
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[FS] JSON-Fehler in %s: %s\n", path, err.c_str());
    return;
  }

  strncpy(gActivePattern.name, doc["name"] | id, 31);
  gActivePattern.name[31]   = '\0';
  gActivePattern.ledLen     = doc["ledLen"]  | MAX_LEDS;
  gActivePattern.wait       = doc["wait"]    | 100;

  JsonArray framesArr = doc["frames"].as<JsonArray>();
  int fc = 0;
  for (JsonArray row : framesArr) {
    if (fc >= MAX_FRAMES) break;
    int li = 0;
    for (int val : row) {
      if (li >= MAX_LEDS) break;
      gActivePattern.frames[fc][li++] = (uint8_t)val;
    }
    // restliche LEDs in diesem Frame auf 0 setzen
    for (; li < MAX_LEDS; li++) gActivePattern.frames[fc][li] = 0;
    fc++;
  }
  gActivePattern.frameCount = fc;

  Serial.printf("[FS] Pattern geladen: %s (%d Frames, ledLen=%d, wait=%d)\n",
                gActivePattern.name, fc, gActivePattern.ledLen, gActivePattern.wait);
}

// -------------------------------------------------------------------------
// savePattern()
// Schreibt ein Pattern aus dem JSON-Body in /patterns/{id}.json
// und aktualisiert /patterns/_index.json (fügt neuen Eintrag ein oder
// überschreibt bestehenden).
// Rückgabe: true bei Erfolg, false bei Fehler.
// -------------------------------------------------------------------------
bool savePattern(const char* id, const char* jsonBody) {
  // 1) Pattern-Datei schreiben
  char path[48];
  snprintf(path, sizeof(path), "/patterns/%s.json", id);

  // JSON validieren
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err) {
    Serial.printf("[FS] savePattern: ungültiges JSON: %s\n", err.c_str());
    return false;
  }
  const char* name = doc["name"] | id;

  File f = LittleFS.open(path, "w");
  if (!f) {
    Serial.printf("[FS] savePattern: kann nicht schreiben: %s\n", path);
    return false;
  }
  f.print(jsonBody);
  f.close();

  // 2) Index-Datei lesen
  JsonDocument idxDoc;
  File fi = LittleFS.open("/patterns/_index.json", "r");
  if (fi) {
    deserializeJson(idxDoc, fi);
    fi.close();
  }
  JsonArray arr = idxDoc.is<JsonNull>() ? idxDoc.to<JsonArray>() : idxDoc.as<JsonArray>();

  // Prüfen, ob id bereits existiert
  bool found = false;
  for (JsonObject entry : arr) {
    if (strcmp(entry["id"] | "", id) == 0) {
      entry["name"] = name;
      found = true;
      break;
    }
  }
  if (!found) {
    JsonObject newEntry = arr.add<JsonObject>();
    newEntry["id"]   = id;
    newEntry["name"] = name;
  }

  // 3) Index zurückschreiben
  File fo = LittleFS.open("/patterns/_index.json", "w");
  if (!fo) {
    Serial.println("[FS] savePattern: Index nicht schreibbar");
    return false;
  }
  serializeJson(idxDoc, fo);
  fo.close();

  // Lokale Liste aktualisieren
  loadPatternIndex();
  Serial.printf("[FS] Pattern gespeichert: %s\n", id);
  return true;
}

// -------------------------------------------------------------------------
// resetPatternToDefault()
// Schreibt das Originalmuster aus AllPattern.h zurück in LittleFS.
// Nutzt dieselbe Mapping-Tabelle wie migrateDefaultPatternsAlt().
// Rückgabe: true bei Erfolg, false wenn ID nicht in den Standardmustern.
// -------------------------------------------------------------------------
bool resetPatternToDefault(const char* id) {
  struct PatInfo {
    const char* id;
    const char* name;
    int index;
    int (*data)[];
  };

  PatInfo infos[] = {
    {"randompattern",      "Random",       0, (int(*)[])randompattern},
    {"cdlpattern",         "CDL",          1, (int(*)[])cdlpattern},
    {"blinkpattern",       "Blink",        2, (int(*)[])blinkpattern},
    {"colorscrollpattern", "Color Scroll", 3, (int(*)[])colorscrollpattern},
    {"colorlinespattern",  "Color Lines",  4, (int(*)[])colorlinespattern},
    {"colorsidespattern",  "Color Sides",  5, (int(*)[])colorsidespattern},
    {"treesnowpattern",    "Tree Snow",    6, (int(*)[])treesnowpattern},
    {"wblinkpattern",      "WBlink",       7, (int(*)[])wblinkpattern},
    {"offpattern",         "Off",          8, (int(*)[])offpattern},
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
        f.print(info.data[fr][li]);
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
// Wird beim Erststart aufgerufen wenn /patterns/_index.json fehlt.
// Schreibt alle Standardmuster aus AllPattern.h als JSON-Dateien in LittleFS.
// AllPattern.h muss colordef[][] und alle Pattern-Arrays enthalten.
// -------------------------------------------------------------------------
void migrateDefaultPatterns() {
  // Verzeichnis anlegen
  LittleFS.mkdir("/patterns");
  LittleFS.mkdir("/www");

  // Hilfsmakro: schreibt ein Pattern-Array als JSON-Datei
  // Parameter: Dateiname-ID, Anzeigename, Array-Name, LEDLEN-Wert, LENS-Wert, WAIT-Wert
  // colordef ist als globale Variable aus AllPattern.h verfügbar

  auto writePattern = [](const char* id, const char* displayName,
                          int ledLen, int wait,
                          uint8_t (*patternData)[],  // Zeiger auf 2D-Array-Zeile
                          int numFrames) {
    char path[48];
    snprintf(path, sizeof(path), "/patterns/%s.json", id);
    File f = LittleFS.open(path, "w");
    if (!f) {
      Serial.printf("[MIG] Fehler beim Öffnen: %s\n", path);
      return;
    }

    f.printf("{\"name\":\"%s\",\"ledLen\":%d,\"wait\":%d,\"frames\":[", displayName, ledLen, wait);
    for (int fr = 0; fr < numFrames; fr++) {
      f.print("[");
      for (int li = 0; li < ledLen; li++) {
        f.print(patternData[fr][li]);
        if (li < ledLen - 1) f.print(",");
      }
      f.print("]");
      if (fr < numFrames - 1) f.print(",");
    }
    f.print("]}");
    f.close();
    Serial.printf("[MIG] Geschrieben: %s\n", path);
  };

  // Alle Standardmuster aus AllPattern.h migrieren
  // Die Makros LEDLEN*, LEN*, WAIT* und die Arrays sind aus AllPattern.h verfügbar
  writePattern("randompattern",     "Random",       LEDLENRANDOMPATTERN,     WAITRANDOMPATTERN,     (uint8_t(*)[])randompattern,     LENRANDOMPATTERN);
  writePattern("cdlpattern",        "CDL",          LEDLENCDLPATTERN,        WAITCDLPATTERN,        (uint8_t(*)[])cdlpattern,        LENCDLPATTERN);
  writePattern("blinkpattern",      "Blink",        LEDLENBLINKPATTERN,      WAITBLINKPATTERN,      (uint8_t(*)[])blinkpattern,      LENBLINKPATTERN);
  writePattern("colorscrollpattern","Color Scroll",  LEDLENCOLORSCROLLPATTERN,WAITCOLORSCROLLPATTERN,(uint8_t(*)[])colorscrollpattern,LENCOLORSCROLLPATTERN);
  writePattern("colorlinespattern", "Color Lines",  LEDLENCOLORLINESPATTERN, WAITCOLORLINESPATTERN, (uint8_t(*)[])colorlinespattern, LENCOLORLINESPATTERN);
  writePattern("colorsidespattern", "Color Sides",  LEDLENCOLORSIDESPATTERN, WAITCOLORSIDESPATTERN, (uint8_t(*)[])colorsidespattern, LENCOLORSIDESPATTERN);
  writePattern("treesnowpattern",   "Tree Snow",    LEDLENTREESNOWPATTERN,   WAITTREESNOWPATTERN,   (uint8_t(*)[])treesnowpattern,   LENTREESNOWPATTERN);
  writePattern("wblinkpattern",     "WBlink",       LEDLENWBLINKPATTERN,     WAITWBLINKPATTERN,     (uint8_t(*)[])wblinkpattern,     LENWBLINKPATTERN);
  writePattern("offpattern",        "Off",          LEDLENOFFPATTERN,        WAITOFFPATTERN,        (uint8_t(*)[])offpattern,        LENOFFPATTERN);

  // Index-Datei schreiben (offpattern immer als letzter Eintrag)
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
```

**Wichtiger Hinweis zur Migration:** Die Hilfsfunktion `writePattern` mit dem Lambda und dem Cast `(uint8_t(*)[])` setzt voraus, dass die Pattern-Arrays in `AllPattern.h` als `int`-Arrays mit exakt den passenden `LEDLEN*`-Makros definiert sind. Falls die Arrays in `AllPattern.h` als `int` statt `uint8_t` deklariert sind (was dem Original entspricht), muss der Cast und die Funktion angepasst werden — am einfachsten, indem für jeden Pattern-Array eine eigene Schreibfunktion angelegt wird (kein Lambda, kein Cast). Siehe Verifikationsschritt.

- [ ] Datei speichern

**Verifikation:** Nach dem ersten Flash (ohne `data/`-Upload) erscheint im Serial Monitor:
```
[FS] LittleFS gemountet
[FS] Kein Pattern-Index gefunden, starte Migration...
[MIG] Geschrieben: /patterns/randompattern.json
... (alle 9 Pattern)
[MIG] Index geschrieben
[MIG] Migration abgeschlossen
[FS] 9 Pattern geladen
```

---

## Task 5 — `pattering.ino` auf generischen Renderer umstellen

**Dateien:** Geändert: `pattering.ino`

Der bisherige `switch/case`-Renderer mit hartcodierten Pattern-Arrays wird durch einen generischen Renderer ersetzt, der `gActivePattern` liest.

- [ ] Gesamten Inhalt von `pattering.ino` ersetzen:

```cpp
// pattering.ino
// Generischer Pattern-Renderer. Liest ausschließlich aus gActivePattern (PatternEngine.h).
// Keine direkten Referenzen mehr auf AllPattern.h-Arrays.

#include "PatternEngine.h"

/*
 * patternwork()
 * Gibt den aktuellen Animationsschritt (gPatStep) des geladenen Patterns
 * auf den LED-Strip aus. Wenn alle Frames durchlaufen sind, springt gPatStep zurück.
 * Wiederholt das Pattern, wenn LED_COUNT > gActivePattern.ledLen.
 */
void patternwork() {
  // Wenn alle Frames durchlaufen: von vorne beginnen
  if (gPatStep >= gActivePattern.frameCount) {
    gPatStep = 0;
  }

  // Aktuellen Frame referenzieren
  const uint8_t* frame = gActivePattern.frames[gPatStep];
  const int ledLen = gActivePattern.ledLen;

  // LEDs setzen: Pattern wird wiederholt, bis alle LEDs versorgt sind
  for (int ledPos = 0; ledPos < strip.numPixels(); ledPos++) {
    int patIdx = ledPos % ledLen;          // Wiederholung des Patterns
    uint8_t colorIdx = frame[patIdx];      // Farbindex in colordef[]
    strip.setPixelColor(ledPos, strip.Color(
      colordef[colorIdx][0],
      colordef[colorIdx][1],
      colordef[colorIdx][2]
    ));
  }

  gPatStep++;

  // Ausgabe nur wenn IR-Empfänger inaktiv (verhindert Interferenzen)
  if (IrReceiver.isIdle()) {
    strip.show();
  }

  // Geschwindigkeit des Pattern-Schrittwechsels
  delay(gActivePattern.wait);
}
```

- [ ] Datei speichern

**Verifikation:** Nach vollständigem Kompilieren (Tasks 1–5 abgeschlossen): Serial Monitor zeigt `[FS] Pattern geladen: Random (X Frames, ...)` und der Baum zeigt das geladene Muster korrekt an.

---

## Task 6 — `myPrefHandling.ino` auf String-Pattern-ID umstellen

**Dateien:** Geändert: `myPrefHandling.ino`

Statt `gSelectedPattern` (int) wird nun `gSelectedPatternId` (String) gespeichert und geladen.

- [ ] Gesamten Inhalt von `myPrefHandling.ino` ersetzen:

```cpp
// myPrefHandling.ino
// Laden und Speichern von IR-Codes, Pattern-ID und Helligkeit in den NVS-Preferences.
// Anstelle des Integer-Pattern-Index wird jetzt der String-ID-Key "IRp" verwendet.

/*
 * getPreferences()
 * Lädt IR-Codes, zuletzt gespeicherte Pattern-ID und Helligkeit aus dem Flash.
 * Nach dem Laden wird gNeedPatternReload auf true gesetzt, damit der Loop
 * das Pattern aus LittleFS nachlädt.
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

  // Pattern-ID als String laden (Key "IRp"), Fallback "randompattern"
  String savedId = preferences.getString("IRp", "randompattern");
  strncpy(gSelectedPatternId, savedId.c_str(), 31);
  gSelectedPatternId[31] = '\0';

  gBrightness = preferences.getUInt("gBr", BRIGHTNESS);
  gPatStep    = 0;
  gNeedPatternReload = true;  // Pattern aus LittleFS neu laden lassen
}

/*
 * writePreferences()
 * Speichert IR-Codes, aktuelle Pattern-ID und Helligkeit.
 * Wird nach Pattern-Wechsel oder Helligkeitsänderung aufgerufen.
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

  // Pattern-ID als String speichern
  preferences.putString("IRp", gSelectedPatternId);
  preferences.putUInt("gBr", gBrightness);
}
```

- [ ] Datei speichern

**Hinweis:** Der alte Key `"gSp"` (int) wird nicht mehr gelesen. Beim ersten Start nach dem Update greift der Fallback `"randompattern"`, da der alte Key einen anderen Typ hat. Das ist gewollt.

**Verifikation:** Nach Power-Cycle erscheint im Serial Monitor `[FS] Pattern geladen: Random ...` (oder das zuletzt gespeicherte Pattern), und der Baum startet mit dem erwarteten Muster.

---

## Task 7 — `IRhandling.ino` auf neue Pattern-IDs umstellen

**Dateien:** Geändert: `IRhandling.ino`

Die direkten `gSelectedPattern = N`-Zuweisungen werden durch ID-basierte Zuweisungen aus der geladenen Pattern-Liste ersetzt.

- [ ] `checkIR()`-Funktion in `IRhandling.ino` komplett ersetzen (nur `checkIR`, nicht `learnIR`):

```cpp
/*
 * checkIR()
 * Wertet empfangene IR-Befehle aus.
 * IR-Codes 0–7 wählen direkt Pattern nach Position in gPatternIds.
 * IR-Code 8: Off (Battery: Abschalten, USB: offpattern).
 * IR-Code 9: On (lädt letztes gespeichertes Pattern via getPreferences).
 * IR-Codes 10/11: Helligkeit dunkler/heller.
 */
void checkIR() {
  if (IrReceiver.decode()) {
    digitalWrite(STAT_LED, HIGH);
    IrReceiver.printIRResultShort(&Serial);

    // Direkte Pattern-Auswahl über IR-Codes 0–7
    // Wählt Pattern nach Index in gPatternIds (ohne offpattern am Ende)
    for (int slot = 0; slot <= 7; slot++) {
      if (IrReceiver.decodedIRData.command == gIRcodes[slot]) {
        // Index in der Liste prüfen (offpattern ist immer letzter Eintrag)
        int targetIdx = slot;
        if (targetIdx < gPatternCount - 1) {  // -1: offpattern überspringen
          strncpy(gSelectedPatternId, gPatternIds[targetIdx], 31);
          gSelectedPatternId[31] = '\0';
          gPatternListIndex = targetIdx;
          gNeedPatternReload = true;
          gPatStep = 0;
        }
      }
    }

    // IR-Code 8: Ausschalten
    if (IrReceiver.decodedIRData.command == gIRcodes[8]) {
      if (analogRead(USB5VSENSE) < 1000) {
        // Batteriebetrieb: echtes Abschalten
        powerOff();
      } else {
        // USB-Betrieb: offpattern aktivieren
        strncpy(gSelectedPatternId, "offpattern", 31);
        gNeedPatternReload = true;
      }
    }

    // IR-Code 9: Einschalten (letztes gespeichertes Pattern laden)
    if (IrReceiver.decodedIRData.command == gIRcodes[9]) {
      getPreferences();  // setzt gSelectedPatternId und gNeedPatternReload = true
    }

    // IR-Code 10: Dunkler
    if (IrReceiver.decodedIRData.command == gIRcodes[10]) {
      gBrightness = (gBrightness > 1) ? gBrightness - 2 : 0;
      strip.setBrightness(gBrightness);
      serialPrintf("gBrightness = %d\n", gBrightness);
    }

    // IR-Code 11: Heller
    if (IrReceiver.decodedIRData.command == gIRcodes[11]) {
      gBrightness = (gBrightness < BRIGHTNESS - 1) ? gBrightness + 2 : BRIGHTNESS;
      strip.setBrightness(gBrightness);
      serialPrintf("gBrightness = %d\n", gBrightness);
    }

    IrReceiver.resume();

    // Präferenz nur speichern, wenn nicht im Off-Zustand
    if (strcmp(gSelectedPatternId, "offpattern") != 0) {
      writePreferences();
    }

    digitalWrite(STAT_LED, LOW);
  }
}
```

- [ ] Datei speichern (nur `checkIR` ersetzen, `learnIR` bleibt unverändert)

**Verifikation:** IR-Fernbedienung Taste 1 lädt `randompattern` (Serial Monitor: `[FS] Pattern geladen: Random ...`). Taste für Off wechselt zu `offpattern` (Baum dunkel). Taste für On kehrt zum letzten Pattern zurück.

---

## Task 8 — `webserver.ino` anlegen

**Dateien:** Neu: `webserver.ino`

Dieser Task implementiert den WiFi-AP und alle REST-API-Endpunkte.

- [ ] Datei `webserver.ino` im Sketch-Verzeichnis anlegen:

```cpp
// webserver.ino
// WiFi Access Point und ESPAsyncWebServer mit REST-API.
// Wird nur aktiviert wenn USB-Spannung anliegt (analogRead(USB5VSENSE) >= 1000).

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "PatternEngine.h"

static AsyncWebServer server(80);

// -------------------------------------------------------------------------
// setupWiFi()
// Prüft USB-Spannung. Bei USB: AP öffnen und Webserver starten.
// Ohne USB: WiFi komplett abschalten (Stromsparen).
// -------------------------------------------------------------------------
void setupWiFi() {
  if (analogRead(USB5VSENSE) >= 1000) {
    Serial.println("[WiFi] USB-Strom erkannt, starte Access Point...");
    startAPAndServer();
  } else {
    Serial.println("[WiFi] Kein USB, WiFi bleibt aus");
    WiFi.mode(WIFI_OFF);
  }
}

// -------------------------------------------------------------------------
// startAPAndServer()
// Öffnet WiFi-AP "ChristmasTree" ohne Passwort und registriert alle Routen.
// -------------------------------------------------------------------------
void startAPAndServer() {
  WiFi.softAP("ChristmasTree");
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] AP gestartet. IP: %s\n", ip.toString().c_str());

  // ---- Statische Dateien ----
  // Liefert index.html aus LittleFS /www/
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(LittleFS, "/www/index.html", "text/html");
  });

  // ---- GET /api/patterns ----
  server.on("/api/patterns", HTTP_GET, [](AsyncWebServerRequest* req) {
    File f = LittleFS.open("/patterns/_index.json", "r");
    if (!f) {
      req->send(500, "application/json", "{\"error\":\"index nicht gefunden\"}");
      return;
    }
    String body = f.readString();
    f.close();
    req->send(200, "application/json", body);
  });

  // ---- GET /api/pattern?id=X ----
  server.on("/api/pattern", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("id")) {
      req->send(400, "application/json", "{\"error\":\"id fehlt\"}");
      return;
    }
    String id = req->getParam("id")->value();
    char path[48];
    snprintf(path, sizeof(path), "/patterns/%s.json", id.c_str());
    if (!LittleFS.exists(path)) {
      req->send(404, "application/json", "{\"error\":\"nicht gefunden\"}");
      return;
    }
    req->send(LittleFS, path, "application/json");
  });

  // ---- POST /api/pattern?id=X (Body: Pattern-JSON) ----
  // ESPAsyncWebServer benötigt für POST-Bodies einen Body-Handler
  server.on("/api/pattern", HTTP_POST,
    [](AsyncWebServerRequest* req) {},  // leerer Completion-Handler
    nullptr,                            // kein Upload
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      // Nur vollständige Bodies verarbeiten (für kleine Pattern-JSONs ausreichend)
      if (index + len < total) return;  // auf nächsten Chunk warten

      if (!req->hasParam("id")) {
        req->send(400, "application/json", "{\"error\":\"id fehlt\"}");
        return;
      }
      String id = req->getParam("id")->value();

      // Null-terminierte Kopie des Body-Puffers erzeugen
      char* body = new char[len + 1];
      memcpy(body, data, len);
      body[len] = '\0';

      bool ok = savePattern(id.c_str(), body);
      delete[] body;

      req->send(ok ? 200 : 500, "application/json",
                ok ? "{\"ok\":true}" : "{\"error\":\"Speichern fehlgeschlagen\"}");
    }
  );

  // ---- GET /api/state ----
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["activeId"]   = gSelectedPatternId;
    doc["brightness"] = gBrightness;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // ---- POST /api/active (Body: {"id":"patternid"}) ----
  server.on("/api/active", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index + len < total) return;

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err || !doc["id"].is<const char*>()) {
        req->send(400, "application/json", "{\"error\":\"ungültiger Body\"}");
        return;
      }
      const char* newId = doc["id"];
      strncpy(gSelectedPatternId, newId, 31);
      gSelectedPatternId[31] = '\0';
      gNeedPatternReload = true;
      gPatStep = 0;

      // Neues Pattern in Liste suchen und Index aktualisieren
      for (int i = 0; i < gPatternCount; i++) {
        if (strcmp(gPatternIds[i], newId) == 0) {
          gPatternListIndex = i;
          break;
        }
      }

      // Nur speichern wenn nicht offpattern
      if (strcmp(newId, "offpattern") != 0) {
        writePreferences();
      }

      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // ---- POST /api/brightness (Body: {"value":20}) ----
  server.on("/api/brightness", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index + len < total) return;

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err || !doc["value"].is<int>()) {
        req->send(400, "application/json", "{\"error\":\"ungültiger Body\"}");
        return;
      }
      int val = doc["value"];
      if (val < 0)   val = 0;
      if (val > 255) val = 255;
      gBrightness = val;
      strip.setBrightness(gBrightness);
      writePreferences();

      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // ---- POST /api/reset?id=X ----
  server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("id")) {
      req->send(400, "application/json", "{\"error\":\"id fehlt\"}");
      return;
    }
    String id = req->getParam("id")->value();
    bool ok = resetPatternToDefault(id.c_str());
    if (ok) {
      // Falls dieses Pattern gerade aktiv ist, neu laden
      if (strcmp(gSelectedPatternId, id.c_str()) == 0) {
        gNeedPatternReload = true;
      }
    }
    req->send(ok ? 200 : 500, "application/json",
              ok ? "{\"ok\":true}" : "{\"error\":\"Original nicht gefunden\"}");
  });

  // ---- 404-Handler ----
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "application/json", "{\"error\":\"nicht gefunden\"}");
  });

  server.begin();
  Serial.println("[WiFi] Webserver gestartet auf Port 80");
}
```

- [ ] Datei speichern

**Verifikation:** Nach dem Flash (noch ohne `data/`-Upload) erscheint im Serial Monitor:
```
[WiFi] USB-Strom erkannt, starte Access Point...
[WiFi] AP gestartet. IP: 192.168.4.1
[WiFi] Webserver gestartet auf Port 80
```
Im WiFi-Scan des Smartphones erscheint das Netzwerk `ChristmasTree`. `curl http://192.168.4.1/api/patterns` gibt den Pattern-Index zurück.

---

## Task 9 — REST-API mit curl testen (ohne Browser)

**Dateien:** Keine Codeänderungen — nur Verifikation

Dieser Task stellt sicher, dass alle API-Endpunkte korrekt funktionieren, bevor die Web-UI gebaut wird.

- [ ] ESP32-S3 flashen (noch ohne `data/`-Upload — `initFilesystem()` führt Migration durch)
- [ ] Mit dem `ChristmasTree`-WLAN verbinden
- [ ] Pattern-Liste abfragen:
  ```
  curl http://192.168.4.1/api/patterns
  ```
  Erwartete Ausgabe: `[{"id":"randompattern","name":"Random"},{"id":"cdlpattern","name":"CDL"},...]`

- [ ] Einzelnes Pattern laden:
  ```
  curl "http://192.168.4.1/api/pattern?id=blinkpattern"
  ```
  Erwartete Ausgabe: JSON mit `name`, `ledLen`, `wait`, `frames`

- [ ] Aktuellen Zustand abfragen:
  ```
  curl http://192.168.4.1/api/state
  ```
  Erwartete Ausgabe: `{"activeId":"randompattern","brightness":20}`

- [ ] Pattern wechseln:
  ```
  curl -X POST -H "Content-Type: application/json" -d '{"id":"blinkpattern"}' http://192.168.4.1/api/active
  ```
  Erwartete Ausgabe: `{"ok":true}` — Baum wechselt zu blinkpattern

- [ ] Helligkeit setzen:
  ```
  curl -X POST -H "Content-Type: application/json" -d '{"value":50}' http://192.168.4.1/api/brightness
  ```
  Erwartete Ausgabe: `{"ok":true}` — Baum leuchtet heller

- [ ] Neues Pattern speichern (minimales Testmuster, 1 Frame, 2 LEDs):
  ```
  curl -X POST "http://192.168.4.1/api/pattern?id=testpattern" \
    -H "Content-Type: application/json" \
    -d '{"name":"Test","ledLen":2,"wait":200,"frames":[[5,10]]}'
  ```
  Erwartete Ausgabe: `{"ok":true}`

- [ ] Test-Pattern auf Original zurücksetzen (sofern es eines der Standardmuster wäre — hier entfällt der Schritt, da `testpattern` kein Standardmuster ist):
  ```
  curl -X POST "http://192.168.4.1/api/reset?id=randompattern"
  ```
  Erwartete Ausgabe: `{"ok":true}`

- [ ] Alle Tests bestanden → Pattern-Index enthält 9 Einträge

---

## Task 10 — `data/www/index.html` anlegen: Pattern-Editor mit API-Modus

**Dateien:** Neu: `data/www/index.html`

Der bestehende `pattern-editor.html` wird erweitert. Der Dual-Modus wird über `window.location.protocol !== 'file:'` erkannt. Im API-Modus erscheinen zusätzliche Steuerelemente; die Import/Export-Logik bleibt im Datei-Modus erhalten.

- [ ] Verzeichnis `data/www/` im Sketch-Verzeichnis anlegen
- [ ] `data/www/index.html` mit folgendem Inhalt anlegen (vollständige Datei):

```html
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <title>NeoChristmasTree Pattern Editor</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: monospace; background: #1a1a1a; color: #ddd; height: 100vh; display: flex; flex-direction: column; }
    #topbar { display: flex; align-items: center; gap: 12px; background: #2a2a2a; padding: 8px 16px; border-bottom: 1px solid #444; flex-shrink: 0; flex-wrap: wrap; }
    #topbar h1 { font-size: 14px; color: #aaa; flex: 1; }
    .btn { background: #3a3a3a; color: #ddd; border: 1px solid #555; padding: 5px 12px; cursor: pointer; font-family: monospace; font-size: 13px; border-radius: 3px; }
    .btn:hover { background: #4a4a4a; }
    .btn-primary { background: #1a5a1a; border-color: #2a8a2a; color: #aeffae; }
    .btn-primary:hover { background: #2a6a2a; }
    .btn-active { background: #1a3a5a; border-color: #2a6a8a; color: #aed6ff; }
    .btn-active:hover { background: #2a4a6a; }
    /* API-Mode-Steuerelemente im topbar */
    #api-controls { display: none; align-items: center; gap: 8px; flex-wrap: wrap; }
    #api-controls.visible { display: flex; }
    #pattern-select { background: #1a1a1a; border: 1px solid #555; color: #ddd; padding: 4px 6px; font-family: monospace; font-size: 13px; border-radius: 3px; max-width: 180px; }
    /* Helligkeit im topbar (API-Modus) */
    #brightness-group { display: none; align-items: center; gap: 6px; font-size: 12px; }
    #brightness-group.visible { display: flex; }
    #brightness-group label { color: #888; }
    #brightness-slider { width: 80px; }
    #brightness-value { width: 32px; color: #ddd; font-size: 12px; }
    #main { display: grid; grid-template-columns: 200px 1fr 180px; flex: 1; overflow: hidden; }
    #frame-panel { background: #222; border-right: 1px solid #444; display: flex; flex-direction: column; overflow: hidden; }
    #frame-panel-header { padding: 8px; background: #2a2a2a; border-bottom: 1px solid #444; font-size: 12px; color: #888; }
    #frame-list { flex: 1; overflow-y: auto; }
    .frame-item { display: flex; align-items: center; gap: 6px; padding: 4px 8px; cursor: pointer; border-bottom: 1px solid #333; user-select: none; }
    .frame-item:hover { background: #2a2a2a; }
    .frame-item.active { background: #1a3a1a; border-left: 3px solid #4a4; }
    .frame-item .frame-num { font-size: 11px; color: #666; width: 24px; text-align: right; }
    .frame-item canvas { border: 1px solid #444; }
    #frame-buttons { display: flex; gap: 4px; padding: 8px; background: #2a2a2a; border-top: 1px solid #444; }
    #frame-buttons .btn { flex: 1; font-size: 16px; padding: 4px; }
    #cross-panel { display: flex; align-items: center; justify-content: center; background: #1a1a1a; overflow: auto; padding: 20px; }
    #cross-grid { display: grid; grid-template-columns: repeat(18, 22px); grid-template-rows: repeat(18, 22px); gap: 1px; }
    .led-cell { width: 22px; height: 22px; background: #000; border: 1px solid #333; cursor: pointer; border-radius: 2px; transition: border-color 0.1s; }
    .led-cell:hover { border-color: #fff; }
    .led-cell.led-inactive { opacity: 0.2; cursor: default; pointer-events: none; }
    #palette-panel { background: #222; border-left: 1px solid #444; display: flex; flex-direction: column; overflow-y: auto; padding: 8px; }
    #palette-panel h3 { font-size: 11px; color: #888; margin-bottom: 6px; }
    .color-group { margin-bottom: 6px; }
    .color-group-label { font-size: 10px; color: #666; margin-bottom: 2px; }
    .color-swatches { display: flex; flex-wrap: wrap; gap: 2px; }
    .color-swatch { width: 24px; height: 24px; cursor: pointer; border: 2px solid transparent; border-radius: 2px; }
    .color-swatch:hover { border-color: #aaa; }
    .color-swatch.active { border-color: #fff; outline: 1px solid #fff; }
    #active-color-label { margin-top: 8px; font-size: 11px; color: #aaa; padding: 4px; background: #2a2a2a; border: 1px solid #444; border-radius: 2px; }
    #sym-btn { margin-top: 12px; width: 100%; font-size: 11px; }
    #bottombar { background: #2a2a2a; border-top: 1px solid #444; padding: 8px 16px; display: flex; align-items: center; gap: 16px; flex-shrink: 0; flex-wrap: wrap; }
    .setting-group { display: flex; align-items: center; gap: 6px; font-size: 12px; }
    .setting-group label { color: #888; }
    .setting-group input[type=text], .setting-group input[type=number] { background: #1a1a1a; border: 1px solid #555; color: #ddd; padding: 3px 6px; font-family: monospace; font-size: 12px; border-radius: 2px; }
    .setting-group input[type=text] { width: 120px; }
    .setting-group input[type=number] { width: 60px; }
    #anim-controls { display: flex; align-items: center; gap: 8px; margin-left: auto; }
    #speed-slider { width: 100px; }
    .modal-overlay { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.7); z-index: 100; align-items: center; justify-content: center; }
    .modal-overlay.open { display: flex; }
    .modal { background: #2a2a2a; border: 1px solid #555; border-radius: 4px; padding: 20px; width: 600px; max-width: 90vw; max-height: 80vh; display: flex; flex-direction: column; gap: 12px; }
    .modal h2 { font-size: 14px; color: #aaa; }
    .modal textarea { flex: 1; min-height: 300px; background: #1a1a1a; color: #ddd; border: 1px solid #555; font-family: monospace; font-size: 11px; padding: 8px; border-radius: 2px; resize: vertical; }
    .modal-buttons { display: flex; gap: 8px; justify-content: flex-end; }
    #import-warning { color: #f90; font-size: 11px; display: none; }
    #status-bar { font-size: 11px; color: #888; padding: 2px 16px; background: #222; border-top: 1px solid #333; flex-shrink: 0; min-height: 20px; }
  </style>
</head>
<body>
  <div id="topbar">
    <h1>NeoChristmasTree Pattern Editor</h1>

    <!-- API-Modus: Pattern-Auswahl und Aktionen -->
    <div id="api-controls">
      <select id="pattern-select" title="Pattern auswählen"></select>
      <button class="btn btn-active" id="api-activate-btn" title="Pattern auf Baum aktivieren">&#9654; Aktivieren</button>
      <button class="btn" id="api-reset-btn" title="Original aus AllPattern.h wiederherstellen">&#8635; Original</button>
    </div>

    <!-- Helligkeit (API-Modus) -->
    <div id="brightness-group">
      <label for="brightness-slider">Helligkeit:</label>
      <input type="range" id="brightness-slider" min="0" max="255" value="20">
      <span id="brightness-value">20</span>
    </div>

    <!-- Datei-Modus: Import/Export -->
    <button class="btn" id="import-btn">&#8595; Import</button>
    <button class="btn" id="preview-btn">&#9654; Vorschau</button>
    <!-- Export (Datei-Modus) / Speichern (API-Modus) — wird per JS umgeschaltet -->
    <button class="btn btn-primary" id="save-btn">&#8593; Export</button>
  </div>

  <div id="main">
    <div id="frame-panel">
      <div id="frame-panel-header">Frames</div>
      <div id="frame-list"></div>
      <div id="frame-buttons">
        <button class="btn" id="frame-add-btn" title="Frame hinzufügen (Kopie)">+</button>
        <button class="btn" id="frame-del-btn" title="Frame löschen">&#8722;</button>
        <button class="btn" id="frame-dup-btn" title="Frame duplizieren">&#10697;</button>
      </div>
    </div>
    <div id="cross-panel">
      <div id="cross-grid"></div>
    </div>
    <div id="palette-panel">
      <h3>Farbe wählen</h3>
      <div id="color-groups"></div>
      <div id="active-color-label">Aktiv: BLK</div>
      <button class="btn" id="sym-btn">PCB1 &#8594; PCB2</button>
    </div>
  </div>

  <div id="bottombar">
    <div class="setting-group">
      <label for="pattern-name">Name:</label>
      <input type="text" id="pattern-name" value="mypattern">
    </div>
    <div class="setting-group">
      <label for="ledlen-input">LEDLEN:</label>
      <input type="number" id="ledlen-input" value="66" min="1" max="66">
    </div>
    <div class="setting-group">
      <label for="wait-input">WAIT (ms):</label>
      <input type="number" id="wait-input" value="100" min="1">
    </div>
    <div id="anim-controls">
      <button class="btn" id="prev-frame-btn">&#9664;&#9664;</button>
      <button class="btn" id="play-btn">&#9654;</button>
      <button class="btn" id="next-frame-btn">&#9654;&#9654;</button>
      <label for="speed-slider" style="font-size:12px;color:#888;">Speed:</label>
      <input type="range" id="speed-slider" min="10" max="1000" value="200">
    </div>
  </div>

  <div id="status-bar">Bereit</div>

  <!-- Export-Modal (Datei-Modus) -->
  <div class="modal-overlay" id="export-modal">
    <div class="modal">
      <h2>Export &#8212; C-Code</h2>
      <textarea id="export-textarea" readonly></textarea>
      <div class="modal-buttons">
        <button class="btn btn-primary" id="copy-btn">Kopieren</button>
        <button class="btn" id="export-close-btn">Schlie&#223;en</button>
      </div>
    </div>
  </div>

  <!-- Import-Modal -->
  <div class="modal-overlay" id="import-modal">
    <div class="modal">
      <h2>Import &#8212; C-Code einfügen</h2>
      <textarea id="import-textarea" placeholder="Hier den C-Array-Block aus AllPattern.h einfügen..."></textarea>
      <div id="import-warning"></div>
      <div class="modal-buttons">
        <button class="btn btn-primary" id="do-import-btn">Importieren</button>
        <button class="btn" id="import-close-btn">Abbrechen</button>
      </div>
    </div>
  </div>

  <script>
    // ============================================================
    // === MODUS-ERKENNUNG ===
    // ============================================================
    const IS_SERVED = window.location.protocol !== 'file:';

    // ============================================================
    // === CONSTANTS ===
    // ============================================================
    const COLORS = [
      { name: 'BLK', r:   0, g:   0, b:   0 },
      { name: 'RD1', r:  50, g:   0, b:   0 },
      { name: 'RD2', r: 100, g:   0, b:   0 },
      { name: 'RD3', r: 150, g:   0, b:   0 },
      { name: 'RD4', r: 200, g:   0, b:   0 },
      { name: 'RD5', r: 255, g:   0, b:   0 },
      { name: 'GN1', r:   0, g:  50, b:   0 },
      { name: 'GN2', r:   0, g: 100, b:   0 },
      { name: 'GN3', r:   0, g: 150, b:   0 },
      { name: 'GN4', r:   0, g: 200, b:   0 },
      { name: 'GN5', r:   0, g: 255, b:   0 },
      { name: 'BL1', r:   0, g:   0, b:  50 },
      { name: 'BL2', r:   0, g:   0, b: 100 },
      { name: 'BL3', r:   0, g:   0, b: 150 },
      { name: 'BL4', r:   0, g:   0, b: 200 },
      { name: 'BL5', r:   0, g:   0, b: 255 },
      { name: 'YL1', r:  50, g:  50, b:   0 },
      { name: 'YL2', r: 100, g: 100, b:   0 },
      { name: 'YL3', r: 150, g: 150, b:   0 },
      { name: 'YL4', r: 200, g: 200, b:   0 },
      { name: 'YL5', r: 255, g: 255, b:   0 },
      { name: 'CY1', r:   0, g:  50, b:  50 },
      { name: 'CY2', r:   0, g: 100, b: 100 },
      { name: 'CY3', r:   0, g: 150, b: 150 },
      { name: 'CY4', r:   0, g: 200, b: 200 },
      { name: 'CY5', r:   0, g: 255, b: 255 },
      { name: 'MG1', r:  50, g:   0, b:  50 },
      { name: 'MG2', r: 100, g:   0, b: 100 },
      { name: 'MG3', r: 150, g:   0, b: 150 },
      { name: 'MG4', r: 200, g:   0, b: 200 },
      { name: 'MG5', r: 255, g:   0, b: 255 },
      { name: 'WH1', r:  50, g:  50, b:  50 },
      { name: 'WH2', r: 100, g: 100, b: 100 },
      { name: 'WH3', r: 150, g: 150, b: 150 },
      { name: 'WH4', r: 200, g: 200, b: 200 },
      { name: 'WH5', r: 255, g: 255, b: 255 },
    ];

    const COLOR_INDEX = {};
    COLORS.forEach((c, i) => { COLOR_INDEX[c.name] = i; });

    function colorCSS(idx) {
      const c = COLORS[idx] || COLORS[0];
      return `rgb(${c.r},${c.g},${c.b})`;
    }

    const CROSS_LAYOUT = (function buildLayout() {
      const layout = new Array(66).fill(null);
      function place(leds, rowFn, colFn) {
        leds.forEach((led, i) => { layout[led - 1] = { row: rowFn(i), col: colFn(i) }; });
      }
      place([8,7,6,5,4,3,2,1],           () => 9,  i => 11 + i);
      place([9,10,11,12,13,14,15,16],    () => 10, i => 11 + i);
      place([17,18,19,20,21,22,23,24],   () => 9,  i => 1 + i);
      place([32,31,30,29,28,27,26,25],   () => 10, i => 1 + i);
      place([33,34,35,36,37,38,39,40],   i => 1 + i, () => 9);
      place([48,47,46,45,44,43,42,41],   i => 1 + i, () => 10);
      place([56,55,54,53,52,51,50,49],   i => 11 + i, () => 9);
      place([57,58,59,60,61,62,63,64],   i => 11 + i, () => 10);
      layout[64] = { row: 9,  col: 9 };
      layout[65] = { row: 10, col: 9 };
      return layout;
    })();

    // ============================================================
    // === STATE ===
    // ============================================================
    const state = {
      name: 'mypattern',
      ledLen: 66,
      wait: 100,
      frames: [ new Array(66).fill(0) ],
      currentFrame: 0,
      activeColorIdx: 0,
      isPlaying: false,
      previewInterval: null,
      // API-Modus: aktuelle Pattern-ID auf dem Server
      currentApiId: null,
    };

    // ============================================================
    // === STATUS-BAR ===
    // ============================================================
    function setStatus(msg) {
      document.getElementById('status-bar').textContent = msg;
    }

    // ============================================================
    // === API-HILFSFUNKTIONEN ===
    // ============================================================
    async function apiGet(url) {
      const r = await fetch(url);
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      return r.json();
    }

    async function apiPost(url, body) {
      const r = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      return r.json();
    }

    async function apiDelete(url) {
      const r = await fetch(url, { method: 'DELETE' });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      return r.json();
    }

    async function apiPostRaw(url, jsonString) {
      const r = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: jsonString,
      });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      return r.json();
    }

    // ============================================================
    // === API-MODUS-INITIALISIERUNG ===
    // ============================================================
    async function initApiMode() {
      // UI-Elemente für API-Modus einblenden
      document.getElementById('api-controls').classList.add('visible');
      document.getElementById('brightness-group').classList.add('visible');
      // Import-Button ausblenden (Import ist im AP-Modus weniger sinnvoll, aber optional belassen)
      // Save-Button umbenennen
      document.getElementById('save-btn').textContent = '↑ Speichern';

      // Helligkeit vom Server laden
      try {
        const st = await apiGet('/api/state');
        const bSlider = document.getElementById('brightness-slider');
        bSlider.value = st.brightness;
        document.getElementById('brightness-value').textContent = st.brightness;
        state.currentApiId = st.activeId;
      } catch (e) {
        setStatus('Fehler beim Laden des Zustands: ' + e.message);
      }

      // Pattern-Liste laden
      await refreshPatternSelect();
    }

    async function refreshPatternSelect() {
      try {
        const patterns = await apiGet('/api/patterns');
        const sel = document.getElementById('pattern-select');
        sel.innerHTML = '';
        patterns.forEach(p => {
          const opt = document.createElement('option');
          opt.value = p.id;
          opt.textContent = p.name;
          if (p.id === state.currentApiId) opt.selected = true;
          sel.appendChild(opt);
        });
        setStatus(`${patterns.length} Pattern geladen`);
      } catch (e) {
        setStatus('Fehler beim Laden der Pattern-Liste: ' + e.message);
      }
    }

    // Pattern aus API in Editor laden
    async function loadPatternFromApi(id) {
      try {
        setStatus('Lade Pattern...');
        const p = await apiGet(`/api/pattern?id=${encodeURIComponent(id)}`);
        state.name          = p.name;
        state.ledLen        = p.ledLen;
        state.wait          = p.wait;
        state.currentApiId  = id;
        // Frames konvertieren: JSON-Zahlen direkt verwenden, Frame-Array auf 66 auffüllen
        state.frames = p.frames.map(fr => {
          const full = new Array(66).fill(0);
          fr.forEach((v, i) => { if (i < 66) full[i] = v; });
          return full;
        });
        state.currentFrame = 0;
        renderAll();
        setStatus(`Pattern geladen: ${p.name}`);
      } catch (e) {
        setStatus('Fehler beim Laden: ' + e.message);
      }
    }

    // Aktuellen Editor-Inhalt als JSON-String für API serialisieren
    function buildPatternJson() {
      const frames = state.frames.map(fr => fr.slice(0, state.ledLen));
      return JSON.stringify({
        name:   state.name,
        ledLen: state.ledLen,
        wait:   state.wait,
        frames: frames,
      });
    }

    // ============================================================
    // === RENDERING ===
    // ============================================================
    function getDisplayColor(ledIdx) {
      const frame = state.frames[state.currentFrame];
      if (state.isPlaying) return frame[ledIdx % state.ledLen];
      return frame[ledIdx];
    }

    function renderCross() {
      const grid = document.getElementById('cross-grid');
      grid.innerHTML = '';
      CROSS_LAYOUT.forEach((pos, ledIdx) => {
        if (!pos) return;
        const colorIdx = getDisplayColor(ledIdx);
        const active = ledIdx < state.ledLen || state.isPlaying;
        const cell = document.createElement('div');
        cell.className = 'led-cell' + (active ? '' : ' led-inactive');
        cell.style.gridColumn = pos.col;
        cell.style.gridRow = pos.row;
        cell.style.backgroundColor = colorCSS(colorIdx);
        cell.title = `LED ${ledIdx + 1}: ${COLORS[colorIdx].name}`;
        cell.dataset.ledIdx = ledIdx;
        grid.appendChild(cell);
      });
    }

    function drawMiniPreview(canvas, frameIdx) {
      const ctx = canvas.getContext('2d');
      const cellSize = 2;
      ctx.fillStyle = '#111';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      CROSS_LAYOUT.forEach((pos, ledIdx) => {
        if (!pos) return;
        const colorIdx = state.frames[frameIdx][ledIdx % state.ledLen];
        const c = COLORS[colorIdx];
        ctx.fillStyle = `rgb(${c.r},${c.g},${c.b})`;
        ctx.fillRect((pos.col - 1) * cellSize, (pos.row - 1) * cellSize, cellSize, cellSize);
      });
    }

    function renderFrameList() {
      const list = document.getElementById('frame-list');
      list.innerHTML = '';
      state.frames.forEach((frame, idx) => {
        const item = document.createElement('div');
        item.className = 'frame-item' + (idx === state.currentFrame ? ' active' : '');
        item.dataset.frameIdx = idx;
        item.draggable = true;
        const numSpan = document.createElement('span');
        numSpan.className = 'frame-num';
        numSpan.textContent = idx + 1;
        const canvas = document.createElement('canvas');
        canvas.width = 36;
        canvas.height = 36;
        item.appendChild(numSpan);
        item.appendChild(canvas);
        list.appendChild(item);
        drawMiniPreview(canvas, idx);
      });
      const activeItem = list.querySelector('.frame-item.active');
      if (activeItem) activeItem.scrollIntoView({ block: 'nearest' });
    }

    function renderPalette() {
      const container = document.getElementById('color-groups');
      container.innerHTML = '';
      const groups = [
        { label: 'Schwarz',  indices: [0] },
        { label: 'Rot',      indices: [1,2,3,4,5] },
        { label: 'Grün',     indices: [6,7,8,9,10] },
        { label: 'Blau',     indices: [11,12,13,14,15] },
        { label: 'Gelb',     indices: [16,17,18,19,20] },
        { label: 'Cyan',     indices: [21,22,23,24,25] },
        { label: 'Magenta',  indices: [26,27,28,29,30] },
        { label: 'Weiß',     indices: [31,32,33,34,35] },
      ];
      groups.forEach(group => {
        const div = document.createElement('div');
        div.className = 'color-group';
        const label = document.createElement('div');
        label.className = 'color-group-label';
        label.textContent = group.label;
        const swatches = document.createElement('div');
        swatches.className = 'color-swatches';
        group.indices.forEach(idx => {
          const swatch = document.createElement('div');
          swatch.className = 'color-swatch' + (idx === state.activeColorIdx ? ' active' : '');
          swatch.style.backgroundColor = colorCSS(idx);
          swatch.title = COLORS[idx].name;
          swatch.dataset.colorIdx = idx;
          if (idx === 0) swatch.style.border = '2px solid #555';
          swatches.appendChild(swatch);
        });
        div.appendChild(label);
        div.appendChild(swatches);
        container.appendChild(div);
      });
      document.getElementById('active-color-label').textContent =
        'Aktiv: ' + COLORS[state.activeColorIdx].name;
    }

    function syncSettings() {
      document.getElementById('pattern-name').value = state.name;
      document.getElementById('ledlen-input').value = state.ledLen;
      document.getElementById('wait-input').value   = state.wait;
    }

    function renderAll() {
      renderCross();
      renderFrameList();
      renderPalette();
      syncSettings();
    }

    // ============================================================
    // === EVENT HANDLERS — LED-GRID ===
    // ============================================================
    document.getElementById('cross-grid').addEventListener('click', e => {
      const cell = e.target.closest('.led-cell');
      if (!cell || state.isPlaying) return;
      const ledIdx = parseInt(cell.dataset.ledIdx);
      if (ledIdx >= state.ledLen) return;
      state.frames[state.currentFrame][ledIdx] = state.activeColorIdx;
      renderCross();
      renderFrameList();
    });

    document.getElementById('cross-grid').addEventListener('contextmenu', e => {
      e.preventDefault();
      const cell = e.target.closest('.led-cell');
      if (!cell || state.isPlaying) return;
      const ledIdx = parseInt(cell.dataset.ledIdx);
      if (ledIdx >= state.ledLen) return;
      state.frames[state.currentFrame][ledIdx] = 0;
      renderCross();
      renderFrameList();
    });

    // ============================================================
    // === EVENT HANDLERS — PALETTE ===
    // ============================================================
    document.getElementById('palette-panel').addEventListener('click', e => {
      const swatch = e.target.closest('.color-swatch');
      if (!swatch) return;
      state.activeColorIdx = parseInt(swatch.dataset.colorIdx);
      renderPalette();
    });

    // ============================================================
    // === EVENT HANDLERS — FRAME-LISTE ===
    // ============================================================
    document.getElementById('frame-list').addEventListener('click', e => {
      const item = e.target.closest('.frame-item');
      if (!item) return;
      state.currentFrame = parseInt(item.dataset.frameIdx);
      renderCross();
      renderFrameList();
    });

    document.getElementById('frame-add-btn').addEventListener('click', () => {
      const copy = [...state.frames[state.currentFrame]];
      state.frames.splice(state.currentFrame + 1, 0, copy);
      state.currentFrame += 1;
      renderAll();
    });

    document.getElementById('frame-del-btn').addEventListener('click', () => {
      if (state.frames.length === 1) return;
      state.frames.splice(state.currentFrame, 1);
      state.currentFrame = Math.min(state.currentFrame, state.frames.length - 1);
      renderAll();
    });

    document.getElementById('frame-dup-btn').addEventListener('click', () => {
      const copy = [...state.frames[state.currentFrame]];
      state.frames.push(copy);
      state.currentFrame = state.frames.length - 1;
      renderAll();
    });

    // Drag & Drop
    let dragSrcIdx = null;
    document.getElementById('frame-list').addEventListener('dragstart', e => {
      const item = e.target.closest('.frame-item');
      if (!item) return;
      dragSrcIdx = parseInt(item.dataset.frameIdx);
      e.dataTransfer.effectAllowed = 'move';
    });
    document.getElementById('frame-list').addEventListener('dragover', e => {
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
    });
    document.getElementById('frame-list').addEventListener('drop', e => {
      e.preventDefault();
      const item = e.target.closest('.frame-item');
      if (!item || dragSrcIdx === null) return;
      const targetIdx = parseInt(item.dataset.frameIdx);
      if (dragSrcIdx === targetIdx) return;
      const moved = state.frames.splice(dragSrcIdx, 1)[0];
      const insertIdx = dragSrcIdx < targetIdx ? targetIdx - 1 : targetIdx;
      state.frames.splice(insertIdx, 0, moved);
      state.currentFrame = insertIdx;
      dragSrcIdx = null;
      renderAll();
    });

    // ============================================================
    // === EVENT HANDLERS — EINSTELLUNGEN ===
    // ============================================================
    document.getElementById('pattern-name').addEventListener('input', e => {
      state.name = e.target.value;
    });

    document.getElementById('ledlen-input').addEventListener('change', e => {
      const val = Math.max(1, Math.min(66, parseInt(e.target.value) || 66));
      state.ledLen = val;
      e.target.value = val;
      renderCross();
    });

    document.getElementById('wait-input').addEventListener('change', e => {
      state.wait = Math.max(1, parseInt(e.target.value) || 100);
      e.target.value = state.wait;
    });

    document.getElementById('sym-btn').addEventListener('click', () => {
      if (state.ledLen <= 32) {
        alert('PCB2-LEDs (33–64) sind bei LEDLEN ≤ 32 inaktiv. Bitte LEDLEN erhöhen.');
        return;
      }
      const frame = state.frames[state.currentFrame];
      for (let i = 0; i < Math.min(32, state.ledLen); i++) {
        frame[32 + i] = frame[i];
      }
      renderCross();
      renderFrameList();
    });

    // ============================================================
    // === EVENT HANDLERS — API-STEUERUNG ===
    // ============================================================

    // Pattern-Dropdown: Auswahl lädt Pattern in Editor
    document.getElementById('pattern-select').addEventListener('change', async e => {
      const id = e.target.value;
      if (id) await loadPatternFromApi(id);
    });

    // Aktivieren: Pattern auf Baum aktivieren
    document.getElementById('api-activate-btn').addEventListener('click', async () => {
      const id = document.getElementById('pattern-select').value;
      if (!id) return;
      try {
        await apiPost('/api/active', { id: id });
        state.currentApiId = id;
        setStatus(`Aktiviert: ${id}`);
      } catch (e) {
        setStatus('Fehler beim Aktivieren: ' + e.message);
      }
    });

    // Original laden: Pattern aus AllPattern.h wiederherstellen
    document.getElementById('api-reset-btn').addEventListener('click', async () => {
      const id = document.getElementById('pattern-select').value;
      if (!id) return;
      if (!confirm(`Pattern "${id}" auf Original zurücksetzen? Alle Änderungen gehen verloren.`)) return;
      try {
        await apiPost('/api/reset?id=' + encodeURIComponent(id), {});
        await loadPatternFromApi(id);
        setStatus(`Original wiederhergestellt: ${id}`);
      } catch (e) {
        setStatus('Fehler beim Zurücksetzen: ' + e.message);
      }
    });

    // Helligkeit
    document.getElementById('brightness-slider').addEventListener('input', e => {
      document.getElementById('brightness-value').textContent = e.target.value;
    });
    document.getElementById('brightness-slider').addEventListener('change', async e => {
      const val = parseInt(e.target.value);
      try {
        await apiPost('/api/brightness', { value: val });
        setStatus(`Helligkeit: ${val}`);
      } catch (err) {
        setStatus('Fehler Helligkeit: ' + err.message);
      }
    });

    // ============================================================
    // === SPEICHERN / EXPORT ===
    // ============================================================
    document.getElementById('save-btn').addEventListener('click', async () => {
      if (IS_SERVED) {
        // API-Modus: Pattern auf Server speichern
        const id = document.getElementById('pattern-select').value || state.currentApiId;
        if (!id) { setStatus('Kein Pattern ausgewählt'); return; }
        try {
          setStatus('Speichere...');
          await apiPostRaw(`/api/pattern?id=${encodeURIComponent(id)}`, buildPatternJson());
          await refreshPatternSelect();
          setStatus(`Gespeichert: ${id}`);
        } catch (e) {
          setStatus('Fehler beim Speichern: ' + e.message);
        }
      } else {
        // Datei-Modus: C-Code-Export-Modal öffnen
        document.getElementById('export-textarea').value = generateExportCode();
        document.getElementById('export-modal').classList.add('open');
      }
    });

    document.getElementById('export-close-btn').addEventListener('click', () => {
      document.getElementById('export-modal').classList.remove('open');
    });

    document.getElementById('copy-btn').addEventListener('click', () => {
      const ta = document.getElementById('export-textarea');
      navigator.clipboard.writeText(ta.value).then(() => {
        const btn = document.getElementById('copy-btn');
        btn.textContent = 'Kopiert!';
        setTimeout(() => { btn.textContent = 'Kopieren'; }, 2000);
      }).catch(() => {
        alert('Kopieren fehlgeschlagen. Bitte manuell markieren und kopieren.');
      });
    });

    // ============================================================
    // === ANIMATION ===
    // ============================================================
    function startPreview() {
      state.isPlaying = true;
      document.getElementById('play-btn').textContent = '❚❚';
      document.getElementById('preview-btn').textContent = '❚❚ Stop';
      scheduleNextFrame();
    }

    function stopPreview() {
      state.isPlaying = false;
      if (state.previewInterval) { clearTimeout(state.previewInterval); state.previewInterval = null; }
      document.getElementById('play-btn').textContent = '▶';
      document.getElementById('preview-btn').textContent = '▶ Vorschau';
      renderCross();
    }

    function updateFrameListActive() {
      const items = document.getElementById('frame-list').querySelectorAll('.frame-item');
      items.forEach((item, idx) => { item.classList.toggle('active', idx === state.currentFrame); });
      const activeItem = items[state.currentFrame];
      if (activeItem) activeItem.scrollIntoView({ block: 'nearest' });
    }

    function scheduleNextFrame() {
      const speed = parseInt(document.getElementById('speed-slider').value);
      state.previewInterval = setTimeout(() => {
        if (!state.isPlaying) return;
        state.currentFrame = (state.currentFrame + 1) % state.frames.length;
        renderCross();
        updateFrameListActive();
        scheduleNextFrame();
      }, speed);
    }

    document.getElementById('preview-btn').addEventListener('click', () => {
      state.isPlaying ? stopPreview() : startPreview();
    });
    document.getElementById('play-btn').addEventListener('click', () => {
      state.isPlaying ? stopPreview() : startPreview();
    });
    document.getElementById('prev-frame-btn').addEventListener('click', () => {
      if (state.isPlaying) return;
      state.currentFrame = (state.currentFrame - 1 + state.frames.length) % state.frames.length;
      renderCross();
      renderFrameList();
    });
    document.getElementById('next-frame-btn').addEventListener('click', () => {
      if (state.isPlaying) return;
      state.currentFrame = (state.currentFrame + 1) % state.frames.length;
      renderCross();
      renderFrameList();
    });

    // ============================================================
    // === EXPORT (C-Code, Datei-Modus) ===
    // ============================================================
    function generateExportCode() {
      const rawName = state.name.trim().replace(/\s+/g, '_') || 'mypattern';
      const upperName = rawName.toUpperCase();
      const lowerName = rawName.toLowerCase();
      const ledLen = state.ledLen;
      const numFrames = state.frames.length;

      let code = `// ============ ${upperName} ===================\n\n`;
      code += `#define LEDLEN${upperName} ${ledLen}\n`;
      code += `#define LEN${upperName}    ${numFrames}\n`;
      code += `#define WAIT${upperName}   ${state.wait}\n\n`;
      code += `int ${lowerName} [LEN${upperName}] [LEDLEN${upperName}] = {\n\n`;

      const headerNums = Array.from({ length: ledLen }, (_, i) =>
        String(i + 1).padStart(4)
      ).join('');
      code += `    //${headerNums}\n`;

      state.frames.forEach((frame, fi) => {
        const values = frame.slice(0, ledLen)
          .map(ci => COLORS[ci].name.padStart(3))
          .join(', ');
        const comma = fi < numFrames - 1 ? ',' : '';
        code += `    { ${values} }${comma}\n`;
      });

      code += `\n};\n`;
      return code;
    }

    // ============================================================
    // === IMPORT (C-Code) ===
    // ============================================================
    function parseImportCode(code) {
      const warnings = [];
      const ledLenMatch = code.match(/#define\s+LEDLEN\w+\s+(\d+)/);
      const ledLen = ledLenMatch ? parseInt(ledLenMatch[1]) : 66;
      const waitMatch = code.match(/#define\s+WAIT\w+\s+(\d+)/);
      const wait = waitMatch ? parseInt(waitMatch[1]) : 100;
      const nameMatch = code.match(/int\s+(\w+)\s*\[/);
      const name = nameMatch ? nameMatch[1] : 'imported';
      const noComments = code.replace(/\/\/[^\n]*/g, '');
      const frames = [];
      const frameRegex = /\{([^}]+)\}/g;
      let match;
      while ((match = frameRegex.exec(noComments)) !== null) {
        const tokens = match[1].split(',').map(t => t.trim()).filter(t => t.length > 0);
        const frameData = new Array(66).fill(0);
        tokens.forEach((token, i) => {
          if (i >= ledLen) return;
          const idx = COLOR_INDEX[token];
          if (idx !== undefined) {
            frameData[i] = idx;
          } else {
            if (!warnings.includes(token)) warnings.push(token);
            frameData[i] = 0;
          }
        });
        frames.push(frameData);
      }
      if (frames.length === 0) {
        return { error: 'Keine Frame-Daten gefunden. Bitte vollständigen C-Array-Block einfügen.' };
      }
      return { name, ledLen, wait, frames, warnings };
    }

    document.getElementById('import-btn').addEventListener('click', () => {
      document.getElementById('import-textarea').value = '';
      document.getElementById('import-warning').style.display = 'none';
      document.getElementById('import-modal').classList.add('open');
    });

    document.getElementById('import-close-btn').addEventListener('click', () => {
      document.getElementById('import-modal').classList.remove('open');
    });

    document.getElementById('do-import-btn').addEventListener('click', () => {
      const code = document.getElementById('import-textarea').value;
      const result = parseImportCode(code);
      const warningEl = document.getElementById('import-warning');
      if (result.error) {
        warningEl.textContent = '⚠ ' + result.error;
        warningEl.style.display = 'block';
        return;
      }
      const hasData = state.frames.length > 1 || state.frames[0].some(v => v !== 0);
      if (hasData && !confirm('Aktuelles Muster wird überschrieben. Fortfahren?')) return;
      state.name   = result.name;
      state.ledLen = result.ledLen;
      state.wait   = result.wait;
      state.frames = result.frames;
      state.currentFrame = 0;
      document.getElementById('import-modal').classList.remove('open');
      if (result.warnings.length > 0) {
        alert('Import erfolgreich, aber mit Warnungen:\nUnbekannte Farben (→ BLK): ' +
          result.warnings.slice(0, 10).join(', '));
      }
      renderAll();
    });

    // ============================================================
    // === INIT ===
    // ============================================================
    renderAll();
    if (IS_SERVED) {
      initApiMode().then(() => {
        // Aktives Pattern aus dem Drop-Down laden
        const sel = document.getElementById('pattern-select');
        if (sel.value) loadPatternFromApi(sel.value);
      });
    } else {
      setStatus('Datei-Modus (file://) — Import/Export aktiv, API nicht verfügbar');
    }
  </script>
</body>
</html>
```

- [ ] Datei speichern

**Verifikation:** Im Browser via `file://...` öffnen — Editor verhält sich wie gehabt (Import/Export sichtbar, API-Steuerelemente ausgeblendet). Statusbar zeigt `Datei-Modus (file://) — Import/Export aktiv, API nicht verfügbar`.

---

## Task 11 — LittleFS-Image hochladen und Browser-Test

**Dateien:** `data/www/index.html` muss im `data/`-Verzeichnis liegen

- [ ] Sicherstellen, dass die Verzeichnisstruktur korrekt ist:
  ```
  NeoChristmasTreeV2-KI/
  ├── data/
  │   └── www/
  │       └── index.html
  ├── NeoChristmasTreeV2.ino
  ├── PatternEngine.h
  ├── filesystem.ino
  ├── webserver.ino
  ├── pattering.ino
  ├── myPrefHandling.ino
  ├── IRhandling.ino
  ├── serialprintf.ino
  ├── IRcodes.h
  └── AllPattern.h
  ```
- [ ] In der Arduino IDE: `Tools → Upload LittleFS Image to ESP32` ausführen (nicht `Upload` / Sketch-Upload!)
  - Dieser Vorgang überschreibt den LittleFS-Bereich des Flash-Speichers mit den Dateien aus `data/`
  - Nach dem LittleFS-Upload muss anschließend auch der Sketch normal geflasht werden (da LittleFS-Upload nur den FS-Bereich beschreibt)
- [ ] Sketch kompilieren und hochladen (`Ctrl+U`)
- [ ] Smartphone oder Laptop mit `ChristmasTree` WLAN verbinden (kein Passwort)
- [ ] Im Browser `http://192.168.4.1` aufrufen
- [ ] Erwartetes Verhalten:
  - Pattern-Editor lädt
  - Statusbar zeigt `9 Pattern geladen` (oder die Anzahl der migrierten Pattern)
  - Pattern-Dropdown zeigt alle Pattern-Namen
  - API-Steuerelemente (Aktivieren, Original, Helligkeit) sind sichtbar

**Verifikation:** `http://192.168.4.1/api/patterns` im Browser gibt valides JSON zurück. `http://192.168.4.1/` zeigt den Editor. Der Baum ändert das Muster, wenn `Aktivieren` geklickt wird.

---

## Task 12 — End-to-End-Test: Pattern im Browser erstellen und auf Baum laden

**Dateien:** Keine Codeänderungen — vollständiger Funktionstest

- [ ] Browser mit `http://192.168.4.1` öffnen
- [ ] Dropdown auf `randompattern` setzen → Pattern lädt in Editor
- [ ] Einen Frame bearbeiten: LED 1 auf `RD5` (Rot) setzen
- [ ] `Speichern` klicken → Statusbar: `Gespeichert: randompattern`
- [ ] `Aktivieren` klicken → Baum zeigt geändertes Pattern, LED 1 leuchtet rot
- [ ] `↺ Original` klicken → Bestätigungsdialog → Pattern wird auf Original zurückgesetzt
- [ ] Statusbar zeigt `Original wiederhergestellt: randompattern`
- [ ] Baum zeigt das Original-Muster (LED 1 nicht mehr rot)
- [ ] Serial Monitor: `[FS] Original wiederhergestellt: randompattern`
- [ ] Helligkeit-Slider auf 80 schieben → Baum leuchtet heller
- [ ] Anderes Pattern aus Dropdown wählen → `Aktivieren` → Baum wechselt
- [ ] IR-Fernbedienung Taste 1 → Baum wechselt zu erstem Pattern in Liste
- [ ] Power-Button → Baum dunkel, Power-Button nochmal → Baum kehrt zurück

---

## Task 13 — Stabilitätstest: Stromversorgungswechsel

**Dateien:** Keine Codeänderungen — Robustheitstest

Dieser Task testet den Übergang zwischen USB-Betrieb (AP aktiv) und Batteriebetrieb (AP aus).

- [ ] Gerät mit USB-Kabel starten → AP `ChristmasTree` erscheint, Webserver aktiv
- [ ] Browser verbinden, Pattern auswählen und aktivieren
- [ ] USB-Kabel abziehen (Gerät läuft auf Batterie weiter) → AP verschwindet aus WiFi-Liste
- [ ] Baum läuft weiter mit letztem Pattern (gespeichert in NVS)
- [ ] IR-Fernbedienung Off-Taste → Gerät schaltet ab (Batteriebetrieb, `powerOff()`)
- [ ] Power-Button drücken → Gerät startet, USB nicht angeschlossen → kein AP, kein Webserver
- [ ] Serial Monitor zeigt: `[WiFi] Kein USB, WiFi bleibt aus`
- [ ] USB-Kabel wieder anstecken und Gerät neu starten → AP erscheint wieder
- [ ] Letztes Pattern wird aus NVS geladen und aus LittleFS gerendert

**Verifikation:** Kein Absturz (kein Watchdog-Reset) beim Wechsel USB/Batterie. Serial Monitor zeigt keine Panik-Traces.

---

## Task 14 — AllPattern.h-Abhängigkeit sichern: Hinweis auf Makro-Namen

**Dateien:** Dokumentation und ggf. Korrekturen in `filesystem.ino`

Die `migrateDefaultPatterns()`-Funktion in `filesystem.ino` setzt voraus, dass `AllPattern.h` die Pattern-Arrays und ihre Dimensionsmakros mit exakten Namen definiert. Dieser Task verifiziert die Makro-Namen und korrigiert ggf. `migrateDefaultPatterns()`.

- [ ] `AllPattern.h` öffnen und prüfen, ob folgende Makros existieren:
  - `LEDLENRANDOMPATTERN`, `LENRANDOMPATTERN`, `WAITRANDOMPATTERN`
  - `LEDLENCDLPATTERN`, `LENCDLPATTERN`, `WAITCDLPATTERN`
  - `LEDLENBLINKPATTERN`, `LENBLINKPATTERN`, `WAITBLINKPATTERN`
  - `LEDLENCOLORSCROLLPATTERN`, `LENCOLORSCROLLPATTERN`, `WAITCOLORSCROLLPATTERN`
  - `LEDLENCOLORLINESPATTERN`, `LENCOLORLINESPATTERN`, `WAITCOLORLINESPATTERN`
  - `LEDLENCOLORSIDESPATTERN`, `LENCOLORSIDESPATTERN`, `WAITCOLORSIDESPATTERN`
  - `LEDLENTREESNOWPATTERN`, `LENTREESNOWPATTERN`, `WAITTREESNOWPATTERN`
  - `LEDLENWBLINKPATTERN`, `LENWBLINKPATTERN`, `WAITWBLINKPATTERN`
  - `LEDLENOFFPATTERN`, `LENOFFPATTERN`, `WAITOFFPATTERN`
- [ ] Falls Makros anders heißen (z.B. `LEDLENRANDOM` statt `LEDLENRANDOMPATTERN`): `migrateDefaultPatterns()` in `filesystem.ino` entsprechend anpassen
- [ ] Falls `AllPattern.h` statt einzelner `LEDLEN*`-Makros ein Array `LEDLENS[]` verwendet (wie im bisherigen Code angedeutet: `LEDLENS[selectedpattern]`), muss `migrateDefaultPatterns()` eine alternative Strategie verwenden:

```cpp
// Alternative für den Fall, dass AllPattern.h LEDLENS[], LENS[], WAITS[] Arrays verwendet
// und keine Einzel-Makros pro Pattern hat.
// In diesem Fall werden die Array-Indizes 0–8 direkt genutzt:
void migrateDefaultPatternsAlt() {
  LittleFS.mkdir("/patterns");
  LittleFS.mkdir("/www");

  // Struktur: {id, displayName, C-Array-Zeiger, index in LEDLENS/LENS/WAITS}
  // Die C-Array-Zeiger verweisen auf die globalen Arrays aus AllPattern.h
  // Typ: int[LEN][LEDLEN], daher Zugriff über int(*)[]-Cast nötig
  struct PatInfo {
    const char* id;
    const char* name;
    int index;      // Index in LEDLENS[], LENS[], WAITS[]
    int (*data)[]; // Zeiger auf erstes Element des 2D-Arrays
  };

  PatInfo infos[] = {
    {"randompattern",      "Random",       0, (int(*)[])randompattern},
    {"cdlpattern",         "CDL",          1, (int(*)[])cdlpattern},
    {"blinkpattern",       "Blink",        2, (int(*)[])blinkpattern},
    {"colorscrollpattern", "Color Scroll", 3, (int(*)[])colorscrollpattern},
    {"colorlinespattern",  "Color Lines",  4, (int(*)[])colorlinespattern},
    {"colorsidespattern",  "Color Sides",  5, (int(*)[])colorsidespattern},
    {"treesnowpattern",    "Tree Snow",    6, (int(*)[])treesnowpattern},
    {"wblinkpattern",      "WBlink",       7, (int(*)[])wblinkpattern},
    {"offpattern",         "Off",          8, (int(*)[])offpattern},
  };

  for (auto& info : infos) {
    int ledLen   = LEDLENS[info.index];
    int numFrames = LENS[info.index];
    int wait     = WAITS[info.index];

    char path[48];
    snprintf(path, sizeof(path), "/patterns/%s.json", info.id);
    File f = LittleFS.open(path, "w");
    if (!f) { Serial.printf("[MIG] Fehler: %s\n", path); continue; }

    f.printf("{\"name\":\"%s\",\"ledLen\":%d,\"wait\":%d,\"frames\":[", info.name, ledLen, wait);
    for (int fr = 0; fr < numFrames; fr++) {
      f.print("[");
      for (int li = 0; li < ledLen; li++) {
        f.print(info.data[fr][li]);
        if (li < ledLen - 1) f.print(",");
      }
      f.print("]");
      if (fr < numFrames - 1) f.print(",");
    }
    f.print("]}");
    f.close();
    Serial.printf("[MIG] %s geschrieben\n", info.id);
  }

  // Index schreiben (gleich wie in migrateDefaultPatterns)
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
  }
  Serial.println("[MIG] Migration abgeschlossen (Alt-Methode)");
}
```

- [ ] Je nach Ergebnis der Prüfung: entweder `migrateDefaultPatterns()` belassen oder durch `migrateDefaultPatternsAlt()` ersetzen und in `initFilesystem()` den Aufruf anpassen
- [ ] Kompilieren und testen — keine Compiler-Fehler wegen unbekannter Symbole

**Verifikation:** Erststart (LittleFS leer): alle 9 Pattern werden migriert, kein Compile-Error, Serial Monitor zeigt 9 × `[MIG] ... geschrieben`.

---

## Task 15 — Git-Commit-Beschreibung (kein Git-Repo vorhanden)

Da das Projekt noch kein Git-Repository hat, wird hier der Commit-Inhalt beschrieben, den man nach Abschluss des Projekts anlegen würde:

**Initialer Commit nach Abschluss aller Tasks:**

Neue Dateien:
- `PatternEngine.h` — gemeinsame Datenstrukturen
- `filesystem.ino` — LittleFS-Verwaltung, Pattern-I/O, Migration
- `webserver.ino` — WiFi-AP, ESPAsyncWebServer, REST-API
- `data/www/index.html` — modifizierter Pattern-Editor mit API-Modus
- `docs/superpowers/plans/2026-05-17-wifi-pattern-manager.md` — dieser Plan

Geänderte Dateien:
- `NeoChristmasTreeV2.ino` — neue Globals (`gSelectedPatternId`, `gNeedPatternReload`), bedingtes WiFi, String-ID-basiertes Cycling
- `pattering.ino` — generischer `ActivePattern`-Renderer, kein `switch/case` mehr
- `myPrefHandling.ino` — `gSelectedPatternId` als String in NVS (Key `"IRp"`)
- `IRhandling.ino` — Pattern-Wechsel über `gPatternIds[]`-Liste

**Commit-Nachricht:**
```
feat: WiFi AP + web-based pattern management via LittleFS

When USB power is detected, the ESP32-S3 opens a WiFi AP (ChristmasTree)
and serves a REST API + pattern editor UI from LittleFS. Patterns are
stored as JSON files and can be created, edited, and deleted via browser.
IR remote and button cycling continue to work via the pattern ID list.
First boot migrates all default patterns from AllPattern.h to LittleFS.
```

---

## Bekannte Einschränkungen und Hinweise

1. **Speicher:** `MAX_FRAMES=200`, `MAX_LEDS=66` → `gActivePattern` belegt `200 × 66 = 13.200 Bytes` im RAM. Der ESP32-S3 hat 512 KB SRAM, das ist unkritisch.

2. **Body-Puffer im Webserver:** Die `POST /api/pattern`-Route puffert den gesamten Body im RAM. Für Pattern mit 200 Frames à 66 LEDs ist das JSON ca. 25–50 KB. Das ist innerhalb der ESP32-S3-RAM-Grenzen, aber bei sehr großen Patterns kann es zum Problem werden. Abhilfe: `MAX_FRAMES` reduzieren oder Chunked-Upload implementieren.

3. **Thread-Safety:** `gSelectedPatternId`, `gNeedPatternReload` und `gBrightness` werden vom Webserver-Task (AsyncWebServer-Callback) geschrieben und vom Arduino-Loop gelesen. Da es sich um einfache Typen und `char`-Arrays handelt und die Callbacks auf dem gleichen Core laufen (ESP32 Core 0 für beide, da keine `xTaskCreatePinnedToCore`-Umlenkung), ist das in der Praxis stabil. Für kritische Anwendungen wäre ein Mutex sauberer.

4. **LittleFS-Upload nach jeder HTML-Änderung:** Wenn `data/www/index.html` geändert wird, muss erneut `Tools → Upload LittleFS Image to ESP32` ausgeführt werden, danach der Sketch neu geflasht werden.

5. **AllPattern.h bleibt im Projekt:** Sie wird nur für den Erststart benötigt. Nach der Migration kann sie entfernt werden — allerdings würde dann ein LittleFS-Format (z.B. durch `LittleFS.format()`) zu einem leeren Pattern-Speicher führen ohne Möglichkeit zur Migration. Empfehlung: Datei im Projekt belassen.

6. **`colordef[][]`-Abhängigkeit in `pattering.ino`:** Der generische Renderer greift weiterhin auf `colordef[][]` aus `AllPattern.h` zu. Das ist korrekt — `AllPattern.h` definiert die 36 Farben, die der Pattern-Editor kennt. Wenn später neue Farben hinzukommen, müssen beide (HTML und `colordef`) synchron aktualisiert werden.
