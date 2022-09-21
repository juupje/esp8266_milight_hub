#include <Arduino.h>
#include <WiFiManager.h>
#pragma once

class NTPHandler {
    static const int NTP_PACKET_SIZE = 48; 
    public:
        NTPHandler(uint16_t port);
        unsigned long requestTime();
    private:
        void sendPacket(IPAddress& address);
        uint16_t port;
        WiFiUDP udp;
         // NTP time stamp is in the first 48 bytes of the message
        byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets
};

