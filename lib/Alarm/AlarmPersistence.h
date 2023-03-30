#include <Alarm.h>
#include <vector>

#ifndef _ALARM_PERSISTENCE_H
#define _ALARM_PERSISTENCE_H
#define ALARM_PERSISTENCE_DEBUG
typedef std::shared_ptr<Alarm> Alarmptr;
class AlarmPersistence {
public:
    AlarmPersistence(Settings& settings) : settings(settings) {}
    Alarmptr get(uint32_t id);
    void set(const Alarmptr alarm);
    void remove(uint32_t id);
    void getSummary(std::vector<uint32_t> &data);
    void setSummary(const std::vector<uint32_t> &data);
    void clear();

private:
  Settings& settings;
  static char* buildFilename(const uint32_t id, char* buffer);
};

#endif
