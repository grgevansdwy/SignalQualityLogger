#define ENABLE_FIRESTORE

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include "../lib/firebaseUpload.h"

#define PROJECT_ID "sclweb-85080"

WiFiClientSecure ssl;
AsyncClientClass client(ssl);
FirebaseApp app;
Firestore::Documents Docs;

bool firebaseUpload(const char* device_id, const char* timestamp,
                    int rssi, int rsrp, int rsrq, int sinr,
                    double lat, double lng,
                    int battery, const char* status) {

    ssl.setInsecure();

    NoAuth no_auth;
    initializeApp(client, app, getAuth(no_auth));

    app.getApp<Firestore::Documents>(Docs);

    // Build Firestore document
    Document<Values::Value> doc;
    doc.add("rssi",      Values::Value(Values::IntegerValue(rssi)));
    doc.add("rsrp",      Values::Value(Values::IntegerValue(rsrp)));
    doc.add("rsrq",      Values::Value(Values::IntegerValue(rsrq)));
    doc.add("sinr",      Values::Value(Values::IntegerValue(sinr)));
    doc.add("latitude",  Values::Value(Values::DoubleValue(lat)));
    doc.add("longitude", Values::Value(Values::DoubleValue(lng)));
    doc.add("timestamp", Values::Value(Values::StringValue(timestamp)));
    doc.add("battery",   Values::Value(Values::IntegerValue(battery)));
    doc.add("status",    Values::Value(Values::StringValue(status)));

    // Collection = device_id, Document = timestamp
    String docPath = String(device_id) + "/" + String(timestamp);

    Docs.createDocument(client, Firestore::Parent(PROJECT_ID), docPath, DocumentMask(), doc);

    if (client.lastError().code() != 0) {
        Serial.println("[Firebase] Upload failed: " + client.lastError().message());
        return false;
    }

    Serial.println("[Firebase] Upload success.");
    return true;
}
