#include <FS.h>
#include <Alarm.h>
#include <string.h>
#include <map>
#include <Settings.h>
#include <Arduino.h>
#include <ArduinoJson.h>

Alarm::Alarm(const JsonObject& json) :
    id(json[F("id")]),
    name(json[F("name")].as<String>()),
    alias(json[F("alias")].as<char*>()),
    utc_time2000(json[F("next_time_utc2000")]),
    repeatTime(json[F("repeat")]),   
    duration(json[F("duration")]),
    autoTurnOff(json[F("auto_turn_off")]),
    bulbId{json["bulb"][GroupStateFieldNames::DEVICE_ID],
            json["bulb"][GroupStateFieldNames::GROUP_ID],
            MiLightRemoteTypeHelpers::remoteTypeFromString("rgb_cct")},
    field(GroupStateFieldHelpers::getFieldByName(json[F("field")])),
    startValue(json[F("start_value")]),
    endValue(json[F("end_value")]),
    initDoc(json[F("init")].as<JsonObject>())
{
    Serial.println("Decoded alarm from json");
}

bool Alarm::trigger(MiLightClient*& milightClient) {
    const MiLightRemoteConfig* config = MiLightRemoteConfig::fromType(bulbId.deviceType);
    if(!config) {
        Serial.println("Could not play alarm....");
        return false;
    }
    milightClient->prepare(config, bulbId.deviceId, bulbId.groupId);

    milightClient->updateStatus(MiLightStatus::ON);
    if(!initDoc.isNull())
        milightClient->update(initDoc.as<JsonObject>());

    // create a variant
    StaticJsonDocument<100> fakedoc;
    StaticJsonDocument<200> doc;
    JsonObject obj = doc.to<JsonObject>();
    obj[F("field")] = GroupStateFieldHelpers::getFieldName(field);
    obj[F("duration")] = duration;
    doc[F("period")] = (uint16_t) (duration*1000/30);
    doc[F("start_value")] = startValue;
    doc[F("end_value")] = endValue;

    #ifdef ALARM_DEBUG
        char output[200];
        Serial.println("Instruction: ");
        serializeJson(doc, output);
        Serial.println(output);
    #endif
    bool ret = milightClient->handleTransition(doc.as<JsonObject>(), fakedoc);
    #ifdef ALARM_DEBUG
        if(!ret) {
            serializeJson(fakedoc, output);
            Serial.println("Error: ");
            Serial.println(output);
        }
    #endif
    return ret;
}

unsigned long Alarm::getAlarmTime() {
    return utc_time2000;
}

unsigned long Alarm::getTimeUntilAlarm(unsigned long currentTime) {
    return utc_time2000 - currentTime;
}

unsigned long Alarm::getTimeUntilAlarm(RtcDateTime time) {
    return utc_time2000-time.TotalSeconds();
}

bool Alarm::hasRepeat() {
    return repeatTime > 0;
}

uint16_t Alarm::getDuration() {
    return duration;
}

uint32_t Alarm::getID() {
    return id;
}

std::shared_ptr<Alarm> Alarm::snooze(uint32_t newID, unsigned long currentTime, MiLightClient*& milightClient, JsonObject& response) {
    if(snoozes < 3) {
        const MiLightRemoteConfig* config = MiLightRemoteConfig::fromType(bulbId.deviceType);
        if(!config) {
            response[F("error")] = F("Could not snooze alarm.");
            return nullptr;
        }
        milightClient->prepare(config, bulbId.deviceId, bulbId.groupId);
        StaticJsonDocument<30> doc;
        JsonObject obj = doc.to<JsonObject>();
        obj[GroupStateFieldHelpers::getFieldName(field)] = startValue;
        milightClient->update(obj);
        return std::make_shared<Alarm>(newID, name, alias, currentTime+SNOOZE_TIME, 0, duration, 0, bulbId, //don't pass over the autoturnoff!
            field, startValue, endValue, initDoc, snoozes+1);
    } else {
        response[F("error")] = F("You already snoozed three times!");
        return nullptr;
    }
}

std::shared_ptr<Alarm> Alarm::repeat() {
    if(repeatTime>0)
        return std::make_shared<Alarm>(id, name, alias, utc_time2000+repeatTime, repeatTime, duration, autoTurnOff,  bulbId,
            field, startValue, endValue, initDoc);
    return nullptr;
}

void Alarm::serialize(JsonObject& json, bool pretty) {
    json[F("id")] = id;
    json[F("name")] = name;
    json[F("alias")] = alias;
    if(pretty) {
        json[F("next_time")] = TimeFormatter::formatTimeEpoch2000(utc_time2000);
        json[F("repeat")] = TimeFormatter::formatTimeHMS(repeatTime);
    } else {
        json[F("next_time_utc2000")] = utc_time2000;
        json[F("repeat")] = repeatTime;     
    }
    json[F("duration")] = duration;
    json[F("auto_turn_off")] = autoTurnOff;
    json[F("start_value")] = startValue;
    json[F("end_value")] = endValue;
    json[F("field")] = GroupStateFieldHelpers::getFieldName(field);
    json[F("init")] = initDoc.as<JsonObject>();
    JsonObject bulbParams = json.createNestedObject("bulb");
    bulbId.serialize(bulbParams);
}