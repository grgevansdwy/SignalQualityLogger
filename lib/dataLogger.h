#ifndef SIGNAL_QUALITY_LOGGING_H
#define SIGNAL_QUALITY_LOGGING_H

/*
  SignalQualityLogging.h  (Arduino IDE + ESP32 core)
 
  Verizon SIM Card
  Modem
  LTE Antenna
  GPS Antenna
 
 Serial0 --> Serial Monitor
 Serial1 --> Modem
*/


#include "Arduino.h"

// ----  Modem Pin assignment based on datasheet ----
#define MODEM_RX 17 //Flipped on datasheet with tx
#define MODEM_TX 18
#define APN "vzwinternet" // Verizon access point name (APN) - address to connect to carrier's network

extern String response;

struct signalQuality{
    int rssi;
    int rsrp;
    int rsrq;
    int sinr;
    bool signalValid;
};

struct gpsData{
    double lat;
    double lng;
    bool fixValid;
};

struct batteryData{
    int voltage_mv;
    bool batteryValid;
};

extern signalQuality sq;

extern gpsData gps;

bool startModem();

signalQuality getSignalQuality();

gpsData getLatLong();

batteryData getBatteryLevel();



#endif // !SIGNAL_QUALITY_LOGGING_H
