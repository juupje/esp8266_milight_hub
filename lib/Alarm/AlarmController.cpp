#include <AlarmController.h>
#include <map>
#include <string.h>
#include <MiLightRemoteConfig.h>
#include <ctime>

AlarmController::AlarmController(uint8 dat_pin, uint8 clk_pin, uint8 rst_pin, Settings& settings,
        MiLightClient*& milightClient, NTPHandler*& ntpHandler, TransitionController& transitions) :
    atomicID(2), //0 could be confused for a null
    wire(dat_pin, clk_pin, rst_pin),
    rtc(wire),
    settings(settings),
    milightClient(milightClient),
    ntpHandler(ntpHandler),
    transitions(transitions),
    activeAlarm(nullptr),
    transitionID(0)
{}

bool AlarmController::begin() {
    rtc.Begin();
    if(!rtc.GetIsRunning()) {
        Serial.println("RTC was not running, starting now!");
        rtc.SetIsRunning(true);
    }
    unsigned long now = ntpHandler->requestTime();
    setRTCTime(now);
    //updateTimeFromRTC(); called by setRTCTime
    alarmList.listAlarms();
    return true;
}

void AlarmController::loop() {
    if(activeAlarm) {
        if(activeAlarm->getAlarmTime()+activeAlarm->getDuration()<getMillisTime()) {
            Serial.println("Alarm is done!");
            activeAlarm = nullptr;
            transitionID = 0;
        }
    }
    if(alarmList.size()>0) {
        if(alarmList.first()->getAlarmTime() <= getMillisTime()) {
            char buffer[20];
            RtcDateTime time = rtc.GetDateTime();
            snprintf_P(buffer,20,PSTR("%04u/%02u/%02u %02u:%02u:%02u"),
                    time.Year(), time.Month(), time.Day(), time.Hour(), time.Minute(), time.Second());
            Serial.print("Triggering alarm at time ");
            Serial.println(buffer);
            //Clear active transitions (possible running alarms) before starting the alarm
            transitions.clear();
            
            activeAlarm = alarmList.shift();
            Serial.print("Alarm: ");
            Serial.println(activeAlarm->getID());
            if(activeAlarm->trigger(milightClient)) {
                transitionID = transitions.getTransitions()->data->id;
            } else {
                Serial.println("Activating alarm failed!");
                activeAlarm = nullptr;
                transitionID = 0;
            }
            
            if(activeAlarm->hasRepeat())
                alarmList.add(activeAlarm->repeat());
        }
    }
}

Alarmptr AlarmController::getAlarm(uint32_t id) {
    return alarmList.get(id);
}

ListNode<Alarmptr>* AlarmController::getAlarms() {
    return alarmList.getHead();
}

uint8_t charsToInt(const char* pString){
    uint8_t value = 0;
    // skip leading 0 and spaces
    while ('0' == *pString || *pString == ' ')
        pString++;
    // calculate number until we hit non-numeral char
    while ('0' <= *pString && *pString <= '9') {
        value *= 10;
        value += *pString - '0';
        pString++;
    }
    return value;
}

bool AlarmController::createAlarm(BulbId& buldId, JsonObject args, JsonDocument& response) {
    if(!args.containsKey(FS(TransitionParams::FIELD))
        ||!args.containsKey(FS(TransitionParams::END_VALUE))
        ||!args.containsKey(FS(TransitionParams::START_VALUE))
        ||!args.containsKey(FS(TransitionParams::DURATION))) {
            response[F("error")] = F("Must specify transition parameters: field, end_value, start_value, duration");
            return false;
    }
    unsigned long utc_time = 0;
    if(args.containsKey(FS(AlarmParams::UTC_TIME))) {
        utc_time = args[FS(AlarmParams::UTC_TIME)];
    } else if(args.containsKey(FS(AlarmParams::TIME))) {
        const char* time = args[FS(AlarmParams::TIME)];
        if(time[0] == '+') {
            uint8_t hours = charsToInt(time);
            uint8_t minutes = charsToInt(time+4);
            unsigned long seconds = (hours*60+minutes)*60+charsToInt(time+7);
            utc_time = rtc.GetDateTime().Epoch32Time()+seconds;
        } else if(args.containsKey(FS(AlarmParams::DATE))) {
            const char* date = args[FS(AlarmParams::DATE)];
            uint8_t yearFrom2000 = charsToInt(date+1);
            uint8_t month = charsToInt(date+5);
            uint8_t day = charsToInt(date+8);
            if(yearFrom2000*month*day==0) {
                response[F("error")] = F("Invalid date");
                return false;
            }
            uint8_t hour = charsToInt(time);
            uint8_t minute = charsToInt(time+3);
            uint8_t second = charsToInt(time+6);
            if(hour >=24 || minute>=60 || second>=60) {
                response[F("error")] = F("Invalid time");
                return false;
            }
            utc_time = RtcDateTime(yearFrom2000, month, day, hour, minute, second).Epoch32Time();
        } else {
            response[F("error")] = F("No date given");
            return false;
        }
    }
    if(utc_time < c_Epoch32OfOriginYear) {
        response[F("error")] = F("Must specify alarm time");
        return false;
    }
    unsigned long repeatTime = 0;
    if(args.containsKey(FS(AlarmParams::REPEAT_TIME)))
        repeatTime = args[FS(AlarmParams::REPEAT_TIME)];
    uint32_t duration =  args[FS(TransitionParams::DURATION)];

    String name(args.containsKey(FS(AlarmParams::NAME)) ? args[FS(AlarmParams::NAME)].as<char*>() : "");
    
    JsonObject init = args[FS(AlarmParams::INIT)]; 
    //Do some checks
    if(utc_time < rtc.GetDateTime().Epoch32Time()) {
        response[F("error")] = F("Alarm time is in the past.");
        return false;
    }
    if(repeatTime>0 && duration>repeatTime) {
        response[F("error")] = F("Duration is longer than the repeat time.");
        return false;
    }
    if(init.containsKey(FS(TransitionParams::DURATION)) || init.containsKey(FS(RequestKeys::TRANSITION))) {
        response[F("error")] = F("Alarm init cannot be a transition.");
        return false;
    }

    //Verify that the field actually exists
    const char* fieldName = args[FS(TransitionParams::FIELD)];
    GroupStateField field = GroupStateFieldHelpers::getFieldByName(fieldName);
    if (field == GroupStateField::UNKNOWN) {
        char errorMsg[30];
        sprintf_P(errorMsg, PSTR("Unknown transition field: %s\n"), fieldName);
        response[F("error")] = errorMsg;
        return false;
    }
    JsonVariant startValue = args[FS(TransitionParams::START_VALUE)];
    JsonVariant endValue = args[FS(TransitionParams::END_VALUE)];
    
    switch(field) {
        case GroupStateField::HUE:
        case GroupStateField::SATURATION:
        case GroupStateField::BRIGHTNESS:
        case GroupStateField::LEVEL:
        case GroupStateField::KELVIN:
        case GroupStateField::COLOR_TEMP:
            {
            Alarmptr alarm = std::make_shared<Alarm>(atomicID++, name, utc_time-c_Epoch32OfOriginYear, repeatTime, duration, buldId,
                field, startValue.as<uint16_t>(), endValue.as<uint16_t>(), init);
            alarmList.add(alarm);
            break;
            }
        default:
            response[F("error")] = F("Transition field not supported for alarms");
            return false;
    }
    return true;
}

void AlarmController::clearAlarms() {
    alarmList.clear();
    atomicID = 0;
}

bool AlarmController::deleteAlarm(uint32_t id) {
    return alarmList.remove(id);
}

String AlarmController::getFormattedTime() {
    return TimeFormatter::formatTimeEpoch2000(rtc.GetDateTime());
}

unsigned long AlarmController::getTime() {
    return rtc.GetDateTime().Epoch32Time();
}

bool AlarmController::refreshTime() {
    unsigned long now = ntpHandler->requestTime();
    return setRTCTime(now);
}

unsigned long AlarmController::getMillisTimeEpoch() {
    return getMillisTime()+c_Epoch32OfOriginYear;
}

unsigned long AlarmController::getMillisTime() {
    unsigned long delta = (millis()-millisStart)/1000;
    if(delta>3600)//one hour
        return updateTimeFromRTC();
    return delta+offset;
}

unsigned long AlarmController::updateTimeFromRTC() {
    offset = rtc.GetDateTime().TotalSeconds();
    millisStart = millis();
    return offset;
}

bool AlarmController::snooze(JsonObject& response) {
    if(activeAlarm) {
        transitions.deleteTransition(transitionID);
        Alarmptr snoozedAlarm = activeAlarm->snooze(atomicID++, rtc.GetDateTime().TotalSeconds(), milightClient, response);
        if(snoozedAlarm) {
            alarmList.add(snoozedAlarm);
            activeAlarm = nullptr;
            return true;
        } else
            return false;
    }
    response[F("error")] = F("No active alarm");
    return false;
}

bool AlarmController::stop() {
    if(activeAlarm) {
        transitions.deleteTransition(transitionID);
        activeAlarm = nullptr;
        return true;
    }
    return false;
}

// Epoch 1970 UTC time 
bool AlarmController::setRTCTime(unsigned long time) {
    if(rtc.GetIsWriteProtected()) {
        Serial.println("Disabling RTC write protection.");
        rtc.SetIsWriteProtected(false);
    }
    ListNode<Alarmptr>* curr = alarmList.getHead();
    while(curr) {
        if(curr->data->getAlarmTime()<time)
            return false;
        curr = curr->next;
    }
    RtcDateTime dt;
    dt.InitWithEpoch32Time(time);
    rtc.SetDateTime(dt);
    Serial.print("Set RTC to ");
    char dateString[20];
    snprintf_P(dateString,20,PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
        dt.Month(), dt.Day(), dt.Year(),dt.Hour(), dt.Minute(), dt.Second());
    Serial.println(dateString);
    Serial.print("Difference: ");
    Serial.println(dt.TotalSeconds()-rtc.GetDateTime().TotalSeconds());
    if(!rtc.GetIsWriteProtected()) {
        Serial.println("Enabling RTC write protection.");
        rtc.SetIsWriteProtected(true);
    }
    updateTimeFromRTC();
    return true;
}

AlarmController::AlarmList::AlarmList() {}

void AlarmController::AlarmList::listAlarms() {
    ListNode<Alarmptr>* curr = list.getHead();
    while(curr) {
        char text[30];
        RtcDateTime dt(curr->data->getAlarmTime());
        snprintf_P(text,30,PSTR("Alarm %d: %02u/%02u/%04u %02u:%02u:%02u"), curr->data->getID(),
                dt.Month(), dt.Day(), dt.Year(),dt.Hour(), dt.Minute(), dt.Second());
        Serial.println(text);
        Serial.println(dt);
        Serial.println(dt.TotalSeconds());
        curr = curr->next;
    }
}
void AlarmController::AlarmList::add(Alarmptr alarm) {
    if(alarm) {
        unsigned long time = alarm->getAlarmTime();
        ListNode<Alarmptr>* curr = list.getHead();
        if(!curr) {
            // list is empty
            list.add(alarm);
            return;
        }
        int index = 0;
        while(curr && time > curr->data->getAlarmTime()) {
            index++;
            curr = curr->next;
        }
        list.add(index, alarm);
    }
}

Alarmptr AlarmController::AlarmList::get(uint32_t id) {
    ListNode<Alarmptr>* curr = list.getHead();
    while(curr) {
        if(curr->data->getID() == id)
            return curr->data;
    }
    return NULL;
}

bool AlarmController::AlarmList::remove(uint32_t id) {
    ListNode<Alarmptr>* curr = list.getHead();
    int index = 0;
    while(curr) {
        if(curr->data->getID() == id) {
            if(list.remove(index))
                return true;
            return false;
        }
        curr = curr->next;
        index++;
    }
    return false;
}

ListNode<Alarmptr>* AlarmController::AlarmList::getHead() {
    return list.getHead();
}

void AlarmController::AlarmList::clear() {
    list.clear();
}

Alarmptr AlarmController::AlarmList::first() {
    if(list.size()>0)
        return list.getHead()->data;
    return NULL;
}

Alarmptr AlarmController::AlarmList::shift() {
    if(list.size()>0)
        return list.shift();
    return NULL;
}