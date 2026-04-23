/*
  ============================================================
  sdCardLogging.ino  (Arduino IDE + ESP32 core)
  - Mounts SD (SD_MMC) in 1-bit mode.
  - Creates signal_log.csv 
  - Generates random LTE-style values used for testing logging application
  - Writes row of data every 5 minutes + CRC32 (can be implemented as CRC_OK)
  - Reads back last line and verifies CRC32
  ============================================================
*/

#include <SD_MMC.h>
#include "../lib/sdCardLogging.h"

bool sd_mounted = false;

/* Mount SDMMC (FAT32) and ensure CSV header exists */
bool sdlog_init(void)
{
  Serial.println("Mounting SD card (SD_MMC)...");

  // On ESP32-S3 you can route SDMMC pins through the GPIO matrix.
  // Set pins from schematic.
  if (!SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0)) {
    Serial.println("ERROR: SD_MMC.setPins() failed (check pins).");
    sd_mounted = false;
    return false;
  }

  // begin(mountpoint, 1bitmode)
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("ERROR: SD CARD NOT MOUNTED");
    sd_mounted = false;
    return false;
  }

  sd_mounted = true;
  Serial.println("SD CARD MOUNTED SUCCESSFULLY");

  return true;
}

/* ------------------------------------------------------------
   Append one row + CRC and verify by reading last line
------------------------------------------------------------ */
bool sdlog_append(int rssi, int rsrp, int rsrq, int sinr, double lat, double lng, int batVoltage, const char* utcTime, const char* device_id, bool GPSValid){
  if (!sd_mounted) {
    Serial.println("ERROR: SD CARD NOT MOUNTED");
    return false;
  }

  // Convert time to date
  char utcDate[7];                                                                                                                                     
  strncpy(utcDate, utcTime, 6);                                                                                                                        
  utcDate[6] = '\0'; 

  // Create file + header if missing
  String logPath = "/" + String(utcDate) + "_SignalLogger.csv";

  if (!SD_MMC.exists(logPath)) {
    File f = SD_MMC.open(logPath, FILE_WRITE);
    if (!f) {
      Serial.println("ERROR: Could not create CSV file");
      return false;
    }
    f.println("Device ID,TimeStamp,RSSI,RSRP,RSRQ,SINR,Latitude,Longitude,Battery Voltage (mV)");
    f.close();
  }

  // Open CSV for append
  File f = SD_MMC.open(logPath, FILE_APPEND);
  if (!f) {
    Serial.println("ERROR: Failed to open CSV for append");
    return false;
  } 
  char row[256];
  if(GPSValid){
    snprintf(row, sizeof(row), "%s,%s,%d,%d,%d,%d,%.6f,%.6f,%d",
             device_id, utcTime, rssi, rsrp, rsrq, sinr, lat, lng, batVoltage);
  } else {
    snprintf(row, sizeof(row), "%s,%s,%d,%d,%d,%d,,,%d",
              device_id, utcTime, rssi, rsrp, rsrq, sinr, batVoltage);
  }
  f.println(row);
  f.close();

  return true;
}

// A function that check whether the prev day's file exist
bool checkPrevFile(const char* utcTime){
  // Convert time to date
  char utcDate[7];                                                                                                                                     
  strncpy(utcDate, utcTime, 6);                                                                                                                        
  utcDate[6] = '\0';

  // Extract month, day, year
  int mm, dd, yy;
  sscanf(utcDate, "%2d%2d%2d", &mm, &dd, &yy);
  struct tm t = {};

  t.tm_mon = mm - 1; // Zero-based index month
  t.tm_mday = dd - 1; // Previous day
  t.tm_year = yy + 100; // 1900 + 100 + 26
  mktime(&t); // Normalizes time (modulo)

  // Create new string formatting
  char logPath[40]; 
  snprintf(logPath, sizeof(logPath), "/%02d%02d%02d_SignalLogger.csv",t.tm_mon+1, t.tm_mday, t.tm_year-100);

  return SD_MMC.exists(logPath);
}