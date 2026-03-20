#include <Arduino.h>
#include "../lib/dataLogger.h"
#include "../lib/wifiConnect.h"
#include "../lib/sdCardLogging.h"
#include "../lib/firebaseUpload.h"

char utc_str[32] = "000000_000000";
char device_id[16] = "SCL-001";

RTC_DATA_ATTR char last_utc[32];

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Device Started");

  // Connect to wifi and take the current time
  bool wifiCheck = false;
  if(wifiConnecter()){
    configTime(0, 0, "pool.ntp.org");
    // wait for time to sync
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
        strftime(utc_str, sizeof(utc_str), "%m%d%y_%H%M%S", &timeinfo);
        Serial.println(utc_str);
        Serial.println("Time synced!");
        wifiCheck = true;
        // firebaseUpload(device_id, timestamp, sq.rssi, sq.rsrp, sq.rsrq, sq.sinr, gps.lat, gps.lng, 100, "Go");
    } else {
       Serial.println("NTP sync failed.");
    }
    WiFi.disconnect(true);  // done with WiFi, save power
  } else {
    Serial.println("WiFi failed.");
  }

  // If wifi works
  if(wifiCheck){
    strcpy(last_utc, utc_str);
  } else {
    // if wifi does not work on x time
    if(last_utc[0] != '\0'){
      struct tm t = {};
      int mm, dd, yy;
      sscanf(last_utc, "%2d%2d%2d_%2d%2d%2d", &mm, &dd, &yy, &t.tm_hour, &t.tm_min, &t.tm_sec); //parse the data
      t.tm_mon  = mm - 1;    // tm_mon is 0-based
      t.tm_mday = dd;
      t.tm_year = yy + 100;  // years since 1900 (26 + 100 = 126 for 2026)
      t.tm_sec += 300; // add by 5 minutes (prediction)
      mktime(&t); // fix overflow
      strftime(utc_str, sizeof(utc_str), "%m%d%y_%H%M%S", &t);
      strcpy(last_utc, utc_str);
    } else {
       // If wifi and NTP can't be connected, then just go to sleep and try again later
      Serial.println("Going to sleep...");
      esp_sleep_enable_timer_wakeup(5 * 60 * 1000000ULL); // 5 min in microseconds
      esp_deep_sleep_start();
    }
  }
  
  // Signal Quality & GPS
  for(int i=0; i<10; i++){
    // Try Modem 10 times
    if(!startModem()){
      Serial.println("Modem retrying..");
      continue;
    }else{
      Serial.println("Modem Successful.");

      // Get the data here
      signalQuality sq = getSignalQuality();
      Serial.println("--- Signal Quality ---");
      if(sq.signalValid){
        Serial.println("RSSI: " + String(sq.rssi));
        Serial.println("RSRP: " + String(sq.rsrp));
        Serial.println("RSRQ: " + String(sq.rsrq));
        Serial.println("SINR: " + String(sq.sinr));
      } else{
        Serial.println("Signal Quality Data Error");
      }
     
      gpsData gps = getLatLong();
      Serial.println("--- GPS Data ---");
      if (gps.fixValid) {
        Serial.println("Lat: " + String(gps.lat, 6));
        Serial.println("Long: " + String(gps.lng, 6));
      } else {
        Serial.println("GPS Data Error");
      }

      batteryData bat = getBatteryLevel();
      Serial.println("--- Battery Data ---");
      if (bat.batteryValid) {
        Serial.println("Battery Voltage: " + String(bat.voltage_mv) + " mV");
      } else {
        Serial.println("Battery Data Error");
      }

      // Log to SD Card after getting all the data
      if(sdlog_init()){
        Serial.println("Succesfully initiating SD Card");
        sdlog_append(sq.rssi, sq.rsrp, sq.rsrq, sq.sinr, gps.lat, gps.lng, bat.voltage_mv, utc_str, device_id);
        SD_MMC.end(); 
      }
      
      break;
    }
  }


  // Sleep for 5 minutes then wake and restart setup()
  Serial.println("Going to sleep...");
  esp_sleep_enable_timer_wakeup(5 * 60 * 1000000ULL); // 5 min in microseconds
  esp_deep_sleep_start();
}

void loop() {}
