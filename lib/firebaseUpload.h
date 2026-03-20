#pragma once

bool firebaseUpload(const char* device_id, const char* timestamp,
                    int rssi, int rsrp, int rsrq, int sinr,
                    double lat, double lng,
                    int battery, const char* status);
