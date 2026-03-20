#ifndef SD_CARD_LOGGING_H
#define SD_CARD_LOGGING_H

/*
  sdCardLogging.h  (Arduino IDE + ESP32 core)

  We use SD_MMC (SDIO/SDMMC) because our schematic shows:
    CMD = GPIO4, CLK = GPIO5, D0 = GPIO6, D3/CD = GPIO46
*/

#include <Arduino.h>
#include <SD_MMC.h>   // ESP32 Arduino SDMMC driver (FATFS underneath)

// ---- SDMMC pin mapping from V2 schematic ----
#define SDMMC_CLK  5
#define SDMMC_CMD  4
#define SDMMC_D0   6

extern bool sd_mounted;

// Initialize/mount SD + create CSV header if needed
bool sdlog_init(void);

// Append one row + CRC and verify by reading back last row
bool sdlog_append(int rssi, int rsrp, int rsrq, int sinr, double lat, double lng, int batVoltage, const char* utcTime, const char* device_id );

#endif
