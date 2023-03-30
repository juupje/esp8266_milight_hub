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
    alarmList(settings),
    milightClient(milightClient),
    ntpHandler(ntpHandler),
    transitions(transitions),
    activeAlarm(nullptr),
    transitionID(0),
    autoTurnOffTime(0),
    activatedBulbId()
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

    //load saved alarms
    alarmList.loadPersistent(getMillisTime());
    return true;
}

void AlarmController::loop() {
    if(autoTurnOffTime > 0) {
        if(getMillisTime() >= autoTurnOffTime) {
            //There has been no interaction with the controller since the alarm was done
            autoTurnOffTime = 0; //so we don't trigger this again
            //turn off the light
            if(activatedBulbId != NULL) {
                const MiLightRemoteConfig* config = MiLightRemoteConfig::fromType(activatedBulbId->deviceType);
                if(config) {
                    milightClient->prepare(config, activatedBulbId->deviceId, activatedBulbId->groupId);
                    milightClient->updateStatus(MiLightStatus::OFF);
                }
                activatedBulbId = NULL;
            }
        }
    }
    if(activeAlarm) {
        if(activeAlarm->getAlarmTime()+activeAlarm->getDuration()<getMillisTime()) {
            if(activeAlarm->autoTurnOff != 0) {
                autoTurnOffTime = getMillisTime()+activeAlarm->autoTurnOff;
                activatedBulbId = &activeAlarm->bulbId;
            }
            activeAlarm = nullptr;
            transitionID = 0;
        }
    }
    if(alarmList.size()>0) {
        if(alarmList.first()->getAlarmTime() <= getMillisTime()) {
            #ifdef ALARM_DEBUG
            char buffer[20];
            RtcDateTime time = rtc.GetDateTime();
            snprintf_P(buffer,20,PSTR("%04u/%02u/%02u %02u:%02u:%02u"),
                    time.Year(), time.Month(), time.Day(), time.Hour(), time.Minute(), time.Second());
            Serial.print("Triggering alarm at time ");
            Serial.println(buffer);
            #endif
            //Clear active transitions (possible running alarms) before starting the alarm
            transitions.clear();
            
            activeAlarm = alarmList.shift();
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
    if(!args.containsKey(FS2(TransitionParams::FIELD))
        ||!args.containsKey(FS2(TransitionParams::END_VALUE))
        ||!args.containsKey(FS2(TransitionParams::START_VALUE))
        ||!args.containsKey(FS2(TransitionParams::DURATION))) {
            response[F("error")] = F("Must specify transition parameters: field, end_value, start_value, duration");
            return false;
    }
    unsigned long utc_time = 0;
    if(args.containsKey(FS2(AlarmParams::UTC_TIME))) {
        utc_time = args[FS2(AlarmParams::UTC_TIME)];
    } else if(args.containsKey(FS2(AlarmParams::TIME))) {
        const char* time = args[FS2(AlarmParams::TIME)];
        if(time[0] == '+') {
            uint8_t hours = charsToInt(time);
            uint8_t minutes = charsToInt(time+4);
            unsigned long seconds = (hours*60+minutes)*60+charsToInt(time+7);
            utc_time = rtc.GetDateTime().Epoch32Time()+seconds;
        } else if(args.containsKey(FS2(AlarmParams::DATE))) {
            const char* date = args[FS2(AlarmParams::DATE)];
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
    if(args.containsKey(FS2(AlarmParams::REPEAT_TIME)))
        repeatTime = args[FS2(AlarmParams::REPEAT_TIME)];

    unsigned long autoTurnOff = 0;
    if(args.containsKey(FS2(AlarmParams::AUTO_TURN_OFF)))
        autoTurnOff = args[FS2(AlarmParams::AUTO_TURN_OFF)];

    uint32_t duration =  args[FS2(TransitionParams::DURATION)];

    String name(args.containsKey(FS2(AlarmParams::NAME)) ? args[FS2(AlarmParams::NAME)].as<char*>() : "");
    String alias(args[FS2(AlarmParams::ALIAS)].as<char*>());
    JsonObject init = args[FS2(AlarmParams::INIT)]; 
    //Do some checks
    if(utc_time < rtc.GetDateTime().Epoch32Time()) {
        response[F("error")] = F("Alarm time is in the past.");
        return false;
    }
    if(repeatTime>0 && duration>repeatTime) {
        response[F("error")] = F("Duration is longer than the repeat time.");
        return false;
    }
    if(init.containsKey(FS2(TransitionParams::DURATION)) || init.containsKey(FS2(RequestKeys::TRANSITION))) {
        response[F("error")] = F("Alarm init cannot be a transition.");
        return false;
    }

    //Verify that the field actually exists
    const char* fieldName = args[FS2(TransitionParams::FIELD)];
    GroupStateField field = GroupStateFieldHelpers::getFieldByName(fieldName);
    if (field == GroupStateField::UNKNOWN) {
        char errorMsg[30];
        sprintf_P(errorMsg, PSTR("Unknown transition field: %s\n"), fieldName);
        response[F("error")] = errorMsg;
        return false;
    }
    JsonVariant startValue = args[FS2(TransitionParams::START_VALUE)];
    JsonVariant endValue = args[FS2(TransitionParams::END_VALUE)];
    
    switch(field) {
        case GroupStateField::HUE:
        case GroupStateField::SATURATION:
        case GroupStateField::BRIGHTNESS:
        case GroupStateField::LEVEL:
        case GroupStateField::KELVIN:
        case GroupStateField::COLOR_TEMP:
            {
            Alarmptr alarm = std::make_shared<Alarm>(atomicID++, name, alias, utc_time-c_Epoch32OfOriginYear, repeatTime, duration, autoTurnOff, buldId,
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

/*
Note that this method still returns the time in seconds!
*/
unsigned long AlarmController::getMillisTime() {
    unsigned long delta = (millis()-millisStart)/1000;
    if(delta>RTC_RESYNC_INTERVAL)
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
            transitionID = 0;
            autoTurnOffTime = 0;
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
        transitionID = 0;
        autoTurnOffTime = 0;
        return true;
    }
    return false;
}

// Epoch 1970 UTC time 
bool AlarmController::setRTCTime(unsigned long time) {
    if(rtc.GetIsWriteProtected()) {
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
    if(!rtc.GetIsWriteProtected()) {
        rtc.SetIsWriteProtected(true);
    }
    updateTimeFromRTC();
    return true;
}

void AlarmController::stopAutoTurnOff() {
    autoTurnOffTime = 0;
}

void AlarmController::getStoredAlarms(std::vector<uint32_t> out) {
    alarmList.persistence.getSummary(out);
}

void AlarmController::AlarmList::loadPersistent(unsigned long time) {
    std::vector<uint32_t> ids;
    persistence.getSummary(ids);
    for(auto it = ids.begin(); it!=ids.end(); ++it) {
        #ifdef ALARM_PERSISTENCE_DEBUG
        Serial.print("Loading alarm ");
        Serial.println(*it);
        #endif
        Alarmptr alarm = persistence.get(*it);
        if(alarm == NULL) {
            #ifdef ALARM_PERSISTENCE_DEBUG
            Serial.println("Got null alarm");
            #endif
            persistence.remove(*it);
            continue;
        }
        // check if the alarm time has passed
        Serial.print(alarm->getAlarmTime());
        Serial.print(" ");
        Serial.println(time);
        if(alarm->getAlarmTime() > time) {
            add(alarm, false);
        } else if(alarm->hasRepeat()) {
            // if the alarm has a repeat time, we can repeat is as many times
            //  as necessary until it is in the future again
            uint16_t n_repeats = (time-alarm->getAlarmTime())/alarm->repeatTime;
            alarm->utc_time2000 = n_repeats*alarm->repeatTime;
            add(alarm, true); //overwrite the existing alarm with the new alarm time
        } else {
            persistence.remove(*it); //remove this alarm
        }
    }
}

void AlarmController::AlarmList::add(Alarmptr alarm) {
    add(alarm, true);
}

void AlarmController::AlarmList::add(Alarmptr alarm, bool add_to_persistence) {
    if(alarm) {
        unsigned long time = alarm->getAlarmTime();
        ListNode<Alarmptr>* curr = list.getHead();
        if(add_to_persistence)
            // Store alarm in persistent storage
            persistence.set(alarm);

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
            persistence.remove(id);
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
    persistence.clear();
}

Alarmptr AlarmController::AlarmList::first() {
    if(list.size()>0)
        return list.getHead()->data;
    return NULL;
}

Alarmptr AlarmController::AlarmList::shift() {
    if(list.size()>0) {
        Alarmptr alarm = list.shift();
        persistence.remove(alarm->getID());
        return alarm;
    }
    return NULL;
}