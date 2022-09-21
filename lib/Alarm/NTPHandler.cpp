#include <NTPHandler.h>

NTPHandler::NTPHandler(uint16_t port) : port(port), udp()
{
    udp.begin(port);
}

unsigned long NTPHandler::requestTime() {
    Serial.println("Getting time");
    IPAddress timeServerIP;
    for(int i = 0; i < 5; i++) {
        WiFi.hostByName("pool.ntp.org", timeServerIP);

        sendPacket(timeServerIP);
        delay(1500);
        int cb = udp.parsePacket();
        if(!cb) {
            Serial.println("No packet yet");
        } else {
            Serial.print("NTP packet received, length=");
            Serial.println(cb);
            udp.read(packetBuffer, NTP_PACKET_SIZE);
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            unsigned long secsSince1900 = highWord<<16 | lowWord;
            Serial.println(secsSince1900);
            return secsSince1900 - 2208988800UL;
            break;
        }
        delay(6000); //try again after 10 seconds
    }
    Serial.println("Failed to get NTP time");
    return 0;
}

void NTPHandler::sendPacket(IPAddress& address) {
    // send an NTP request to the time server at the given address

  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123);  // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}