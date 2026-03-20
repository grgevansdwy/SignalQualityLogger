#include <WiFi.h>
#include "../credentials.h"


bool wifiReady;

// ========= wifi =======
static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  // all the parameters are coming from .onEvent

  Serial.printf("[WiFi] event=%d\n", (int)event);  // see what’s arriving

  switch (event) {
      case ARDUINO_EVENT_WIFI_READY:
      Serial.println("[WiFi] Ready");
      break;

    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println("[WiFi] STA start");
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] Connected to AP");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
      wifiReady = true;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] Disconnected, reason=%d\n",
                    info.wifi_sta_disconnected.reason);
      wifiReady = false;
      break;

    default:
      break;
  }
}

bool wifiConnecter() {
  WiFi.mode(WIFI_STA); // Tells ESP32 to act as a client
  WiFi.setAutoReconnect(true); // keep retrying in background
  WiFi.setHostname("SCL_SignalLogger");
  WiFi.onEvent(onWiFiEvent); // Call back function to notify when stuff happens
  WiFi.begin(WIFI_SSID, WIFI_PASS); // Start joining the network

  Serial.printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);

  // wait at least 10 seconds to try to connect to wifi
  int attempts = 0;
  while(!wifiReady && attempts < 20){
    delay(500);
    attempts++;
  }
  return wifiReady;
}