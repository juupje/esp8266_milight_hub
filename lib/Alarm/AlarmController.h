#include <Arduino.h>
#include <LinkedList.h>
#include <Alarm.h>
#include <AlarmPersistence.h>
#include <RtcDS1302.h>
#include <ThreeWire.h>
#include <memory>
#include <Settings.h>
#include <MiLightClient.h>
#include <NTPHandler.h>
#include <TransitionController.h>
#pragma once

//#define ALARM_DEBUG
#define AUTOTURNOFF 300 //5 minutes in seconds
#define RTC_RESYNC_INTERVAL 3600 //one hour
typedef std::shared_ptr<Alarm> Alarmptr;

namespace AlarmParams {
  static const char UTC_TIME[] PROGMEM = "utc_time";
  static const char REPEAT_TIME[] PROGMEM = "repeat_time";
  static const char NAME[] PROGMEM = "name";
  static const char INIT[] PROGMEM = "init";
  static const char TIME[] PROGMEM = "time";
  static const char DATE[] PROGMEM = "date";
  static const char AUTO_TURN_OFF[] PROGMEM = "auto_turn_off";
  static const char ALIAS[] PROGMEM = "alias";
}

class AlarmController {
    class AlarmList {
        public:
            AlarmList(Settings& settings) : persistence(settings) {};
            void add(Alarmptr alarm);
            bool remove(uint32_t id);
            Alarmptr get(uint32_t id);
            size_t size() { return list.size();}
            ListNode<Alarmptr>* getHead();
            void clear();
            Alarmptr first();
            Alarmptr shift();
            void loadPersistent(unsigned long time);
            AlarmPersistence persistence;
        private:
            void add(Alarmptr alarm, bool add_to_persistence);
            LinkedList<Alarmptr> list;

    };
    public:
        AlarmController(uint8 dat_pin, uint8 clk_pin, uint8 rst_pin,
            Settings& settings, MiLightClient*& milightClient, NTPHandler*& ntpHandler, TransitionController& transitions);
        bool begin();
        void clearAlarms();
        bool createAlarm(BulbId& id, JsonObject json, JsonDocument& response);
        bool deleteAlarm(uint32_t id);
        bool refreshTime();
        Alarmptr getAlarm(uint32_t id);
        ListNode<Alarmptr>* getAlarms();
        void getStoredAlarms(std::vector<uint32_t> out);
        void loop();
        String getFormattedTime();
        unsigned long getTime(); //in Epoch time (since 1970)
        unsigned long getMillisTime(); //in Epoch2000 time (since 2000, using millis())
        unsigned long getMillisTimeEpoch(); //in Epoch time (since 1970, using millis())
        bool stop();
        bool snooze(JsonObject& response);
        void stopAutoTurnOff();

    private:
        bool setRTCTime(unsigned long time);
        unsigned long updateTimeFromRTC();
        unsigned long millisStart;
        unsigned long offset; // in seconds

        size_t atomicID;
        ThreeWire wire;
        RtcDS1302<ThreeWire> rtc;
        Settings& settings;
        AlarmList alarmList;
        MiLightClient*& milightClient;
        NTPHandler*& ntpHandler;

        TransitionController& transitions;
        Alarmptr activeAlarm;
        size_t transitionID;
        unsigned long autoTurnOffTime = 0;
        const BulbId* activatedBulbId;
};