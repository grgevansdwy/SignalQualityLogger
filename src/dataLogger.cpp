// UART Communication (Pre defined by esp32)
// Serial --> Serial Monitor
// Serial1 --> Modem
#include "../lib/dataLogger.h"

String response;
signalQuality sq;
gpsData gps;
batteryData bat;

// Helper function to send command char one by one
static void sendRaw(const char* cmd) {
  for (int i = 0; i < strlen(cmd); i++) {
    // ESP32 connect to modem using serial1
    Serial1.write(cmd[i]);
    delay(10); //delay is needed to gives the modem time to receive
  }
  Serial1.write('\r'); // \r mean carriage return
  delay(10);
  Serial1.write('\n'); // \n newline
  delay(10);
}

// Helper function to send and receive response
static bool sendAT(const char* cmd, unsigned long timeout = 2000) {
  // Send command to modem
  sendRaw(cmd);
  unsigned long start = millis();
  // Loop for 2 seconds until response available
  while (millis() - start < timeout) {
    // Success Case : return true and print response in serial
    if (Serial1.available()) {
      response = Serial1.readString();
      Serial.println("<< " + response);
      if (response.indexOf("OK") != -1) {
        return true;
      }
    }
  }
  // Fail case : Timeout after 2 seconds
  Serial.println("Timeout: " + String(cmd));
  return false;
}

bool startModem(){
  // Begin modem communicaton
  Serial1.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  // If modem not turn on, then keep trying
  if(sendAT("AT", 2000)){
    return true;
  }
  return false;
}

signalQuality getSignalQuality() {
  signalQuality sq = {0, 0, 0, 0, false};

  // Try 5 times, modem might be to early
  for(int i=0; i<5; i++){
    if (sendAT("AT+CPSI?", 1000)) {
    // Check if we actually have LTE service before parsing
      if (response.indexOf("LTE") != -1) {
        int dummy_int;
        char dummy_str[32];
            
        const char* cpsiStart = strstr(response.c_str(), "+CPSI:");
        if (cpsiStart != nullptr){
          sscanf(cpsiStart, "+CPSI: %[^,],%[^,],%[^,],%[^,],%d,%d,%[^,],%d,%d,%d,%d,%d,%d,%d",
              dummy_str, dummy_str, dummy_str, dummy_str, &dummy_int, &dummy_int,
              dummy_str, &dummy_int, &dummy_int, &dummy_int, &sq.sinr, &sq.rsrp, &sq.rssi, &sq.rsrq);
           Serial.println("Successfully parsed signal quality data.");
           Serial.println("RAW: [" + response + "]");
           sq.signalValid = true;
           break;
        } 
        Serial.println("Parsing error.");
      } else {
        // LTE Fail
        Serial.println("Modem is active but has NO SERVICE.");
      }
    } else {
      // MODEM Fail
      Serial.println("Modem failed to respond to CPSI query.");
    }
  }
  delay(3000);
  return sq;
}

batteryData getBatteryLevel() {
  bat = {0, false};

  if (!sendAT("AT+CBC", 2000)) {
    Serial.println("Battery query failed.");
    return bat;
  }

  // Response format: +CBC: 4.236V
  const char* cbcStart = strstr(response.c_str(), "+CBC:");
  if (cbcStart == nullptr) {
    Serial.println("Battery parse failed.");
    return bat;
  }

  float voltage = 0;
  sscanf(cbcStart, "+CBC: %fV", &voltage);
  bat.voltage_mv = (int)(voltage * 1000);
  bat.batteryValid = true;

  Serial.println("Battery: " + String(bat.voltage_mv) + " mV");
  return bat;
}

// Get GPS Lat, Long
// Logic in 
// power on gps on the main.ino

gpsData getLatLong(){
  Serial.println("Getting GPS data...");
  // 1. turns on the GPS Power (GNSS)
  gpsData gps = {0.0, 0.0, false};

  // Power on GPS and wait 3 seconds for it to wake up
  if (!sendAT("AT+CGNSSPWR=1", 1000)) {
    Serial.println("Failed to power on GPS.");
    return gps; // exit early, fixValid = false
  }

  delay(3000);

  // 2. Poll for fix — max 20 attempts, 3 seconds apart (60 seconds total)
  for (int attempt = 1; attempt <= 20; attempt++) {
    Serial.println("GPS attempt " + String(attempt) + " of 20...");

    // Case 1 : GPS see no response
    if (!sendAT("AT+CGPSINFO", 1000)) {
      Serial.println("GPS modem not responding. (case 1)");
      delay(3000);
      continue;
    }

    // Case 2 : GNSS on but still searching for radar
    if (response.indexOf(",,") != -1 || response.length() < 20) {
      Serial.println("No fix yet, waiting... (case 2)");
      Serial.println("RAW: [" + response + "]");
      delay(3000);
      continue;
    }

    // Example: +CGPSINFO: 4739.493202,N,12218.428135,W,020326,071113,45.2,0.0,0.0
    char ns, ew;
    double rawLat, rawLon;
    float alt, speed, course;
    int date, time;

    const char* cgpsStart = strstr(response.c_str(), "+CGPSINFO:");
    // Case 3 : Parse fail
    if (cgpsStart == nullptr) {
      Serial.println("Parse Fail (case 3)");
      delay(3000);
      continue;
    }

    // Case 4 : Success
    int parsed = sscanf(cgpsStart, "+CGPSINFO: %lf,%c,%lf,%c,%d,%d,%f,%f,%f",
                        &rawLat, &ns, &rawLon, &ew, &date, &time, &alt, &speed, &course);

    if (parsed >= 4) { // We at least got Lat and Lon
        // Convert DDMM.MMMM to Decimal Degrees
        // Latitude: 4739.493202 -> 47 + (39.493202 / 60)
        double latDeg = (int)(rawLat / 100);
        gps.lat = latDeg + (rawLat - (latDeg * 100)) / 60.0;
        if (ns == 'S') gps.lat *= -1;

        // Longitude: 12218.428135 -> 122 + (18.428135 / 60)
        double lonDeg = (int)(rawLon / 100);
        gps.lng = lonDeg + (rawLon - (lonDeg * 100)) / 60.0;
        if (ew == 'W') gps.lng *= -1;

        gps.fixValid = true;
        Serial.printf("Success! Lat: %.6f, Lon: %.6f\n", gps.lat, gps.lng);
        sendAT("AT+CGNSSPWR=0", 1000); 
        return gps;
    } else {
        Serial.println("Parse failed, retrying...");
        delay(3000);
    }
  }

  sendAT("AT+CGNSSPWR=0", 1000); 

  Serial.println("GPS gave up after 20 attempts.");
  return gps;
}