#include <Arduino.h>
#include "../lib/dataLogger.h" // Signal Quality, Ping Time, GPS, Battery Data Logger
#include "../lib/wifiConnect.h" // Wifi Communication
#include "../lib/sdCardLogging.h" // Micro SD Card
#include "../lib/firebaseUpload.h" // HTTP Upload to Firebase

// UTC Time
char utc_str[32] = "000000_000000";
// Device ID
char device_id[16] = "SCL-001";

RTC_DATA_ATTR char last_utc[32];
RTC_DATA_ATTR int counterGPS;
RTC_DATA_ATTR int counterFirebase;
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Device Started");

  // Only initialize when esp power on first time
  if (esp_reset_reason() == ESP_RST_POWERON) {
    counterGPS = 0;
    counterFirebase = 0;
  }
  counterGPS++;
  counterFirebase++;

  // Connect to wifi and take the current time
  bool wifiCheck = false;
  if(wifiConnecter()){
    configTime(0, 0, "pool.ntp.org");
    // wait for time to sync
    struct tm timeinfo;
    // extract utc time
    if (getLocalTime(&timeinfo, 5000)) {
        strftime(utc_str, sizeof(utc_str), "%m%d%y_%H%M%S", &timeinfo);
        Serial.println(utc_str);
        Serial.println("Time synced!");
        wifiCheck = true;
    } else {
       Serial.println("NTP sync failed.");
    }
    // check if firebaseUpload will fire, do not turn off the wifi yet

    WiFi.disconnect(true);
  } else {
    Serial.println("WiFi failed.");
  }

  // If wifi works
  if(wifiCheck){
    // Case : Wifi + NTP Success
    // Copy utc time extracted to last_utc RTC variable
    strcpy(last_utc, utc_str);
  } else {
    // Case : Wifi | NTP Unsuccessful
    if(last_utc[0] != '\0'){
      // Add last_utc by 5 minutes for fallback
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
      // last_utc is undefined (first try)
      // enter deep sleep for 5 mins
      Serial.println("Going to sleep...");
      esp_sleep_enable_timer_wakeup(5 * 60 * 1000000ULL); // 5 min in microseconds
      esp_deep_sleep_start();
    }
  }

  // Turn on Modem
  bool modemReady = false;
  for(int i=0; i<10; i++){
    if(!startModem()){
      Serial.println("Modem retrying..");
      continue;
    } else {
      Serial.println("Modem Successful.");
      modemReady = true;
      break;
    }
  }
  
  // Signal Quality & GPS
  if(modemReady){
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
    
    // Get data every 24 hours or First time boot
    bool GPSValid = false;
    gpsData gps = {}; // Default {0,0}
    if(counterGPS >= 288 || esp_reset_reason() == ESP_RST_POWERON){
      gps = getLatLong();
      Serial.println("--- GPS Data ---");
      if (gps.fixValid) {
        Serial.println("Lat: " + String(gps.lat, 6));
        Serial.println("Long: " + String(gps.lng, 6));
        counterGPS = 0;
        GPSValid = true;
      } else {
        Serial.println("GPS Data Error");
      }
    }


    batteryData bat = getBatteryLevel();
    Serial.println("--- Battery Data ---");
    if (bat.batteryValid) {
      Serial.println("Battery Voltage: " + String(bat.voltage_mv) + " mV");
    } else {
      Serial.println("Battery Data Error");
    }

    // Log to SD Card after getting all the data
    // add on later, check whether data is valid, if not then make no input to the file.
    if(sdlog_init()){
      Serial.println("Succesfully initiating SD Card");
      sdlog_append(sq.rssi, sq.rsrp, sq.rsrq, sq.sinr, gps.lat, gps.lng, bat.voltage_mv, utc_str, device_id, GPSValid);
      // check if more than 24 hours and previous day file exist, then upload to firebase
      if(counterFirebase>=288 && checkPrevFile(utc_str)){
        // turn on wifi again to upload firebase
        if(wifiConnecter()){
          if(firebaseUpload(device_id, utc_str)){
            counterFirebase = 0;
          }
          WiFi.disconnect(true);
        }
      }
      SD_MMC.end(); 
    }

  }
      
  // Sleep for 5 minutes then wake and restart setup()
  Serial.println("Going to sleep...");
  esp_sleep_enable_timer_wakeup(5 * 60 * 1000000ULL); // 5 min in microseconds
  esp_deep_sleep_start();
}

void loop() {}
