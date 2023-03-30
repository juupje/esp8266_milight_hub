#include <ArduinoJson.h>
#include <BulbId.h>
#include <RtcDateTime.h>
#include <memory>
#include <MiLightClient.h>
#include <TimeFormatter.h>
#pragma once

#define SNOOZE_TIME 300 //5 minutes

class Alarm {
    public:
        Alarm(uint32_t id, String name, String bulbAlias, unsigned long utc_time, unsigned long repeatTime,
                uint16_t duration, uint16_t autoTurnOff, const BulbId& bulbId,
                GroupStateField field, uint16_t startValue, uint16_t endValue, JsonObject init, uint8_t snoozes = 0):
            id(id),
            name(name),
            alias(bulbAlias),
            utc_time2000(utc_time),
            repeatTime(repeatTime),
            duration(duration),
            autoTurnOff(autoTurnOff),
            bulbId(bulbId),
            field(field),
            startValue(startValue),
            endValue(endValue),
            initDoc(init.memoryUsage()+JSON_OBJECT_SIZE(2)),
            snoozes(snoozes)
            {
                initDoc.set(init);
            };
         Alarm(uint32_t id, String name, String bulbAlias, unsigned long utc_time, unsigned long repeatTime,
                uint16_t duration, uint16_t autoTurnOff, const BulbId& bulbId,
                GroupStateField field, uint16_t startValue, uint16_t endValue, JsonDocument init, uint8_t snoozes = 0):
            id(id),
            name(name),
            alias(bulbAlias),
            utc_time2000(utc_time),
            repeatTime(repeatTime),
            duration(duration),
            autoTurnOff(autoTurnOff),
            bulbId(bulbId),
            field(field),
            startValue(startValue),
            endValue(endValue),
            initDoc(init),
            snoozes(snoozes) {}
        Alarm(const JsonObject& serial);
        unsigned long getAlarmTime();
        unsigned long getTimeUntilAlarm(unsigned long currentTime);
        unsigned long getTimeUntilAlarm(RtcDateTime time);
        bool hasRepeat();
        uint16_t getDuration();
        std::shared_ptr<Alarm> repeat();
        uint32_t getID();
        bool trigger(MiLightClient*& client);
        void serialize(JsonObject &json, bool pretty);
        std::shared_ptr<Alarm> snooze(uint32_t newID, unsigned long currentTime, MiLightClient*& client, JsonObject& response);
    private:
        uint32_t id;
        String name;
        String alias;
        unsigned long utc_time2000; //seconds since 2000
        unsigned long repeatTime;
        uint16_t duration;
        uint16_t autoTurnOff;
        const BulbId& bulbId;

        GroupStateField field;
        uint16_t startValue, endValue;
        DynamicJsonDocument initDoc;
        uint8_t snoozes;
    friend class AlarmController;
    friend class AlarmPersistence;
};