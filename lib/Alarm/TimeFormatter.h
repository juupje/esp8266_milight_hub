#include <string.h>
#include <Arduino.h>
#include <RtcDateTime.h>
#ifndef TIMEFORMATTER
#define TIMEFORMATTER

class TimeFormatter {
    public:
    static String formatTimeEpoch2000(unsigned long time);

    static String formatTimeHMS(unsigned long time);
};
#endif