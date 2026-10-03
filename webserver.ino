// webserver.ino
// WiFi access point and ESPAsyncWebServer with REST API.
// Only activated when USB voltage is present (analogRead(USB5VSENSE) >= 1000).

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "PatternEngine.h"

static AsyncWebServer server(80);

// -------------------------------------------------------------------------
// setupWiFi()
// Checks USB voltage. On USB: open AP and start the webserver.
// Without USB: turn WiFi off completely (saves power).
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
// Opens the WiFi AP "ChristmasTree" without a password and registers all routes.
// -------------------------------------------------------------------------
void startAPAndServer() {
  WiFi.softAP(APHOSTNAME);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] AP gestartet. IP: %s\n", ip.toString().c_str());

  // ---- Static files ----
  // Serves index.html from LittleFS /www/
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!LittleFS.exists("/www/index.html")) {
      req->send(503, "text/plain", "index.html fehlt - LittleFS-Daten hochladen!");
      return;
    }
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

  // ---- POST /api/pattern?id=X (Body: pattern JSON) ----
  // Chunks are streamed directly into the file (no buffer in RAM).
  server.on("/api/pattern", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      // Completion handler: file has been written, update the index
      if (req->_tempObject) {
        File* fp = (File*)req->_tempObject;
        fp->close();
        delete fp;
        req->_tempObject = nullptr;
      }
      if (!req->hasParam("id")) {
        req->send(400, "application/json", "{\"error\":\"id fehlt\"}");
        return;
      }
      String id = req->getParam("id")->value();
      bool ok = updateIndexFromFile(id.c_str());
      req->send(ok ? 200 : 500, "application/json",
                ok ? "{\"ok\":true}" : "{\"error\":\"Speichern fehlgeschlagen\"}");
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      if (!req->hasParam("id")) return;
      String id = req->getParam("id")->value();
      char path[48];
      snprintf(path, sizeof(path), "/patterns/%s.json", id.c_str());

      if (index == 0) {
        // First chunk: create the file and remember the file handle
        File* fp = new File(LittleFS.open(path, "w"));
        if (*fp) {
          fp->write(data, len);
          req->_tempObject = fp;
        } else {
          delete fp;
          Serial.printf("[WiFi] savePattern: Datei nicht öffenbar: %s\n", path);
        }
      } else if (req->_tempObject) {
        // Write subsequent chunks directly into the open file
        File* fp = (File*)req->_tempObject;
        fp->write(data, len);
      }
    }
  );

  // ---- GET /api/state ----
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    DynamicJsonDocument doc(256);
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

      DynamicJsonDocument doc(256);
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

      // Find the new pattern in the list and update the index
      for (int i = 0; i < gPatternCount; i++) {
        if (strcmp(gPatternIds[i], newId) == 0) {
          gPatternListIndex = i;
          break;
        }
      }

      // Only save if not offpattern
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

      DynamicJsonDocument doc(256);
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
      // If this pattern is currently active, reload it
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
