#include <ArduinoJson.h>
#include <FS.h>
#include <ESP8266WebServer.h>
class Presets {
    public:
        static bool retrieve(ESP8266WebServer& server);
        static bool save(JsonObject json);
};