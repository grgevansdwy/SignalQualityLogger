# SCL Firmware — LTE Signal Quality Logger

Firmware for an ESP32-S3 device that logs LTE signal quality (RSSI, RSRP, RSRQ, SINR), GPS coordinates, and battery voltage every 5 minutes to a microSD card. Once per day it uploads the previous day's CSV log to Firebase Firestore over Wi-Fi.

---

## Hardware Requirements

| Component | Notes |
|---|---|
| ESP32-S3-DevKitC-1 | Target board |
| LTE modem (SIM7080G or compatible) | Must support AT+CPSI, AT+CBC, AT+CGPSINFO |
| SIM card | Verizon by default (APN: `vzwinternet`) — change if using a different carrier |
| LTE antenna | Connected to modem |
| GPS antenna | Connected to modem |
| microSD card | FAT32 formatted |
| Battery | Voltage read via modem's AT+CBC command |

---

## Wiring

### Modem (UART — Serial1)

| Signal | ESP32-S3 GPIO |
|---|---|
| ESP TX → Modem RX | GPIO 18 |
| ESP RX ← Modem TX | GPIO 17 |

> The header defines these as `MODEM_TX` and `MODEM_RX` — note the names are from the **modem's** perspective in comments but wired from the ESP's side. See `lib/dataLogger.h`.

### microSD Card (SD_MMC — 1-bit mode)

| Signal | ESP32-S3 GPIO |
|---|---|
| CLK | GPIO 5 |
| CMD | GPIO 4 |
| D0 | GPIO 6 |

---

## Software Setup

### 1. Install PlatformIO

Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code, or use the PlatformIO CLI.

### 2. Clone / open the project

Open this folder as a PlatformIO project. Dependencies are declared in `platformio.ini` and will be fetched automatically on first build:

- `erriez/ErriezCRC32`
- `peterus/ESP-FTP-Server-Lib`
- `mobizt/FirebaseClient`

### 3. Fix the `platformio.ini` build flags

The `build_flags` in `platformio.ini` contain **absolute paths** that only work on the original machine:

```ini
build_flags =
    -I/Users/georgeevansdaenuwy/Documents/PlatformIO/Projects/SCLFirmware/lib/SD_MMC/src
    -I/Users/georgeevansdaenuwy/.platformio/packages/framework-arduinoespressif32/libraries/FS/src
```

Replace them with paths relative to your own machine. The second path points to PlatformIO's internal Arduino ESP32 package. You can find your PlatformIO home with:

```bash
pio system info
```

Then update the flags to match your paths, for example:

```ini
build_flags =
    -Ilib/SD_MMC/src
    -I${platformio.packages_dir}/framework-arduinoespressif32/libraries/FS/src
```

---

## Configuration

### credentials.h — Wi-Fi credentials

Copy the example file and fill in your network:

```bash
cp credentials.h.example credentials.h
```

Edit `credentials.h`:

```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"
```

> `credentials.h` should **not** be committed to version control. It contains your real Wi-Fi password.

---

### src/main.cpp — Device ID

Line 10 sets the device identifier used in log filenames and Firestore document paths:

```cpp
char device_id[16] = "SCL-001";
```

Change `"SCL-001"` to a unique ID for each device you deploy.

---

### lib/dataLogger.h — Modem pins and carrier APN

```cpp
#define MODEM_RX 17   // GPIO connected to modem's TX line
#define MODEM_TX 18   // GPIO connected to modem's RX line
#define APN "vzwinternet"  // Verizon APN — change for your carrier
```

Common APNs:
- Verizon: `vzwinternet`
- AT&T: `phone`
- T-Mobile: `fast.t-mobile.com`
- Hologram IoT: `hologram`

---

### lib/sdCardLogging.h — SD card pins

```cpp
#define SDMMC_CLK  5
#define SDMMC_CMD  4
#define SDMMC_D0   6
```

Change these to match your schematic if your SD card is wired to different GPIO pins.

---

### src/firebaseUpload.cpp — Firebase project ID

Line 8 sets the Firestore project:

```cpp
#define PROJECT_ID "sclweb-85080"
```

Replace `"sclweb-85080"` with your own Firebase project ID. You can find it in the Firebase console under **Project Settings**.

#### Firebase setup

1. Create a Firebase project at [console.firebase.google.com](https://console.firebase.google.com).
2. Enable **Cloud Firestore** in the project.
3. Set Firestore rules to allow writes from the device. For testing you can use open rules, but lock these down for production:
   ```
   rules_version = '2';
   service cloud.firestore {
     match /databases/{database}/documents {
       match /{document=**} {
         allow read, write: if true;
       }
     }
   }
   ```
4. The firmware uses **NoAuth** (no API key or service account) — Firestore rules must allow unauthenticated writes for the upload to succeed.

---

## Build and Flash

```bash
# Build
pio run

# Upload to board
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

---

## How It Works

1. **Boot** — connects to Wi-Fi, syncs time from NTP (`pool.ntp.org`), then disconnects.
2. **Modem** — initializes the LTE modem over Serial1 using AT commands.
3. **Data collection** — reads signal quality (AT+CPSI), battery voltage (AT+CBC), and GPS (AT+CGPSINFO, once every 24 hours or on first boot).
4. **SD card** — appends a CSV row to a daily file named `MMDDYY_SignalLogger.csv`.
5. **Firebase upload** — once per day (every 288 wake cycles × 5 min = 24 hours), if the previous day's CSV exists and Wi-Fi is available, it uploads each row as a Firestore document under the path `{device_id}/{timestamp}`.
6. **Deep sleep** — sleeps for 5 minutes then restarts from `setup()`.

### CSV format

```
Device ID,TimeStamp,RSSI,RSRP,RSRQ,SINR,Latitude,Longitude,Battery Voltage (mV)
SCL-001,042226_143000,-85,-105,-12,15,47.658400,-122.307400,3850
```

GPS columns are left empty on readings where no fix was obtained.

---

## Troubleshooting

| Symptom | Check |
|---|---|
| `SD CARD NOT MOUNTED` | Verify SD pin definitions in `lib/sdCardLogging.h` and that the card is FAT32 |
| `Modem retrying..` | Check modem wiring, baud rate (115200), and that the modem is powered |
| `Modem is active but has NO SERVICE` | SIM card seated? Correct APN in `lib/dataLogger.h`? LTE antenna connected? |
| `NTP sync failed` | Wi-Fi credentials correct in `credentials.h`? |
| `[Firebase] Row upload failed` | Check Firestore security rules and project ID in `src/firebaseUpload.cpp` |
| `GPS gave up after 20 attempts` | GPS antenna connected? Clear sky view needed for first fix |
