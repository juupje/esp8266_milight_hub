#include <TimeFormatter.h>

String TimeFormatter::formatTimeEpoch2000(unsigned long time) {
    RtcDateTime dt(time);
    char buffer[20];
    snprintf_P(buffer,20,PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
                    dt.Day(), dt.Month(), dt.Year(),dt.Hour(), dt.Minute(), dt.Second());
    return String(buffer);
}

String TimeFormatter::formatTimeHMS(unsigned long time) {
    uint16_t days = time/86400;
    uint32_t left = time-(days*86400);
    uint8_t hours = left/3600;
    left -= hours*3600;
    uint8_t minutes = left/60;
    left -= minutes*60;

    char buffer[19];
    snprintf_P(buffer,19,PSTR("%4ud %dh %dm %ds"), days, hours, minutes, left);
    return String(buffer);
}