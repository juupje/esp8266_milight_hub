#include <AlarmPersistence.h>
#include <AlarmController.h>
#include <FS.h>

static const char FILE_PREFIX[] = "alarms/";
static const char SUMMARY_FILE[]= "alarms/summary";

void AlarmPersistence::set(const Alarmptr alarm) {
    char path[30];
    memset(path, 0, 30);
    buildFilename(alarm->getID(), path);
    Serial.print(path);

    File f = SPIFFS.open(path, "w");
    DynamicJsonDocument doc(500);
    JsonObject obj = doc.to<JsonObject>();
    alarm->serialize(obj, false);
    serializeJson(doc, f);
    f.close();
    
    #ifdef ALARM_PERSISTENCE_DEBUG
    Serial.print("Added alarm with id ");
    Serial.println(alarm->getID());
    #endif

    std::vector<uint32_t> ids;
    getSummary(ids);
    for(uint32_t i = 0; i < ids.size(); i++) {
        if(ids[i]==alarm->getID())
            return; //nothing todo, this alarm is already present
            #ifdef ALARM_PERSISTENCE_DEBUG
            Serial.print("Added alarm with id ");
            Serial.print(ids[i]);
            Serial.println(" to summary");
            #endif
    }
    //alarm ID wasn't in the summary yet, so we add it
    ids.push_back(alarm->getID());
    setSummary(ids);
}

void AlarmPersistence::remove(uint32_t id) {
    Serial.println("Removing alarm!");
    std::vector<uint32_t> ids;
    getSummary(ids);
    for(auto i = ids.begin(); i != ids.end(); ++i) {
        if(*i==id) {
            ids.erase(i);
            char path[30];
            memset(path, 0, 30);
            buildFilename(id, path);
            SPIFFS.remove(path);
            setSummary(ids);
            return;
        }
    }
}

Alarmptr AlarmPersistence::get(uint32_t id) {
    char path[30];
    memset(path, 0, 30);
    buildFilename(id, path);
    #ifdef ALARM_PERSISTENCE_DEBUG
    Serial.println(path);
    #endif
    if(SPIFFS.exists(path)) {
        Serial.println("Path exists");
        File f = SPIFFS.open(path, "r");
        DynamicJsonDocument doc(500);
        deserializeJson(doc, f);

        uint32_t id = doc["id"];
        String name = doc["name"];
        String alias = doc["alias"];
        unsigned long utc_time2000 = doc["next_time_utc2000"];
        unsigned long repeatTime = doc["repeat"];
        uint16_t duration = doc["duration"];
        uint16_t autoTurnOff = doc["auto_turn_off"];
        std::map<String, BulbId>::iterator it = settings.groupIdAliases.find(doc["alias"]);
        if(it == settings.groupIdAliases.end()) {
            return NULL;
        }
        BulbId& bulbId = it->second;        
        GroupStateField field = GroupStateFieldHelpers::getFieldByName(doc["field"]);
        uint16_t startValue = doc["start_value"];
        uint16_t endValue = doc["end_value"];
        JsonObject initDoc = doc["init"];
        return std::make_shared<Alarm>(id, name, alias, utc_time2000, repeatTime, duration, autoTurnOff, bulbId, field, startValue, endValue, initDoc);
    } else
        //this alarm is not stored
        return NULL;
}

void AlarmPersistence::getSummary(std::vector<uint32_t> &data) {
    #ifdef ALARM_PERSISTENCE_DEBUG
    Serial.println("Getting summary");
    #endif
    if(!SPIFFS.exists(SUMMARY_FILE))
        return;
    File f = SPIFFS.open(SUMMARY_FILE, "r");
    while(f.available()) {
        uint32_t val = std::atoi(f.readStringUntil(' ').c_str());
        data.push_back(val);
    }
    f.close();
    #ifdef ALARM_PERSISTENCE_DEBUG
    Serial.print("Got summary, length: ");
    Serial.println(data.size());
    #endif
}

void AlarmPersistence::setSummary(const std::vector<uint32_t> &data) {
    File f = SPIFFS.open(SUMMARY_FILE, "w");
    for(uint32_t i = 0; i < data.size(); i++) {
        char buf[10];
        snprintf(buf, 10, "%d ", data[i]);
        f.print(buf);
    }
    f.close();
}

void AlarmPersistence::clear() {
    std::vector<uint32_t> ids;
    getSummary(ids);
    for(uint32_t i = 0; i < ids.size(); i++) {
        char path[30];
        memset(path, 0, 30);
        buildFilename(ids[i], path);
        SPIFFS.remove(path);
    }
    SPIFFS.remove(SUMMARY_FILE);
}

char* AlarmPersistence::buildFilename(const uint32_t id, char *buffer) {
  return buffer + sprintf(buffer, "%s%x", FILE_PREFIX, id);
}
