#define ENABLE_FIRESTORE

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <SD_MMC.h>
#include "../lib/firebaseUpload.h"

#define PROJECT_ID "sclweb-85080"

WiFiClientSecure ssl;
AsyncClientClass client(ssl);
FirebaseApp app;
Firestore::Documents Docs;

bool uploadRow(const char* device_id, const char* line){
    char devId[16], timestamp[32];
    int rssi, rsrp, rsrq, sinr, battery;
    double lat, lng;

     // Try parsing with GPS (all 9 fields)
    int parsed = sscanf(line, "%15[^,],%31[^,],%d,%d,%d,%d,%lf,%lf,%d",
                        devId, timestamp, &rssi, &rsrp, &rsrq, &sinr, &lat, &lng, &battery);
    
    // Case where not all data is being stored
    if(parsed != 9) {
        // GPS columns are empty, so parse without lat/lng (parse = 7)
        parsed = sscanf(line, "%15[^,],%31[^,],%d,%d,%d,%d,,,%d",
                        devId, timestamp, &rssi, &rsrp, &rsrq, &sinr, &battery);
        if(parsed < 7) {
            Serial.println("[Firebase] Skipping malformed row: " + String(line));
            return false;
        }
    }

    // Build Firestore document
    Document<Values::Value> doc;
    doc.add("rssi",      Values::Value(Values::IntegerValue(rssi)));
    doc.add("rsrp",      Values::Value(Values::IntegerValue(rsrp)));
    doc.add("rsrq",      Values::Value(Values::IntegerValue(rsrq)));
    doc.add("sinr",      Values::Value(Values::IntegerValue(sinr)));
    doc.add("timestamp", Values::Value(Values::StringValue(timestamp)));
    doc.add("battery",   Values::Value(Values::IntegerValue(battery)));

    // Only add GPS fields if valid
    if(parsed == 9) {
        doc.add("latitude",  Values::Value(Values::DoubleValue(lat)));
        doc.add("longitude", Values::Value(Values::DoubleValue(lng)));
    }

    // Path: device_id/timestamp
    // Uploade to Firebase
    String docPath = String(device_id) + "/" + String(timestamp);
    Docs.createDocument(client, Firestore::Parent(PROJECT_ID), docPath, DocumentMask(), doc);

    // Validation
    if(client.lastError().code() != 0) {
        Serial.println("[Firebase] Row upload failed: " + client.lastError().message());
        return false;
    }
    return true;
}

bool firebaseUpload(const char* device_id, const char* utcTime) {
    // -------- Find previous date (REUSE prevCheckFile Logic) --------
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


    // ----------- Reading File -----------
    Serial.println("[Firebase] Uploading file: " + String(logPath));
    File f = SD_MMC.open(logPath, FILE_READ);
    if(!f){
        Serial.println("[Firebase] Failed to open file.");
        return false;
    }

    // ----------- Init Firebase -----------
    ssl.setInsecure();
    NoAuth no_auth;
    initializeApp(client, app, getAuth(no_auth));
    app.getApp<Firestore::Documents>(Docs);

    bool firstLine =true;
    int successCount = 0;
    int failCount = 0;

    while(f.available()){
        String line = f.readStringUntil('\n'); // read per row
        line.trim(); // trim every spaces

        // Don't include first line
        if(firstLine) {
            firstLine = false;
            continue;
        }
        // If line is empty
        if(line.length() == 0) {
            continue;
        }

        if(uploadRow(device_id, line.c_str())) {
            successCount++;
        } else {
            failCount++;
        }
    }
    f.close();

    Serial.println("[Firebase] Done. Success: " + String(successCount) + " Failed: " + String(failCount));

    return (failCount == 0);
}



