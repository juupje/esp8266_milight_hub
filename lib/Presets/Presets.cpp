#include <Presets.h>

#ifndef PRESETS_FILE
#define PRESETS_FILE "/presets.json"

bool Presets::retrieve(ESP8266WebServer& server) {
  if (SPIFFS.exists(PRESETS_FILE)) {
    File f = SPIFFS.open(PRESETS_FILE, "r");
    server.streamFile(f, "application/json");
    f.close();
  } else {
    return false;
  }
  return true;
}

bool Presets::save(JsonObject json) {
  File f = SPIFFS.open(PRESETS_FILE, "w");
  if (!f) {
    Serial.println(F("Opening presets file failed"));
    return false;
  } else {
    serializeJson(json, f);
    f.close();
    return true;
  }
}
#endif