#include <Arduino.h>
#line 1 "C:\\Users\\kavee\\Desktop\\MECH3890\\VSCODE\\transmitter\\transmitter.ino"
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

// WiFi Credentials
#define WIFI_SSID "Enter SSID"
#define WIFI_PASSWORD "hn6vxMhpgyth"

// Firebase Configuration
#define FIREBASE_HOST "https://logistics-parts-tracking-default-rtdb.europe-west1.firebasedatabase.app/"
#define FIREBASE_AUTH "P4NyJrff21NJrsii689KLoSLNuslrMGiJEAhTqSE"

// Firebase Objects
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Serial Communication
HardwareSerial mySerial(1);

// **** Global Editable Variables ****
long rackid = 1110001;    // Global rackid variable (editable)
// **********************************

// Struct Definitions
struct SensorData1 {
    long sensorID;
    int boxCount;
    bool autoMode;       // NEW: Added autoMode field
    int autoBoxSize;     // NEW: Added autoBoxSize field
};

struct SensorData2 {
    long sensorID;
    int boxSize;
    int rackLength;
};

// ** Initial Default SensorData2 Variables **
SensorData2 sensorDataArrayToSend[] = {
    {63600001, 100, 1200},
    {63600002, 150, 1200},
    {63600003, 700, 1200},
    {63600004, 200, 1200},
    {63600005, 120, 1200}
};

// Automatically Count Sensors
const int SENSOR_COUNT = sizeof(sensorDataArrayToSend) / sizeof(sensorDataArrayToSend[0]);

// Global Rack Length (Initial Default Value)
int rackLength = 1200;

// Data storage for received sensor data
SensorData1 receivedSensorDataArray[SENSOR_COUNT];

// Timing Variables
unsigned long lastFetchTime = 0;
const int fetchInterval = 5000; // Check Firebase every 5 seconds

// Function Prototypes
void fetchRackLengthFromFirebase();
void fetchDataFromFirebase();
void uploadReceivedDataToFirebase();

void setup() {
    Serial.begin(115200);
    mySerial.begin(9600, SERIAL_8N1, 4, 5);

    Serial.println("\nStarting ESP32...");

    // Connect to WiFi
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        Serial.print(".");
        delay(1000);
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ Connected to WiFi!");
    } else {
        Serial.println("\n⚠️ WiFi Connection Failed! Continuing with default values...");
    }

    // Firebase Initialization
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial.println("✅ Firebase Initialized!");
}

void loop() {
    // Check Firebase for updates every 5 seconds
    if (millis() - lastFetchTime > fetchInterval) {
        fetchRackLengthFromFirebase();
        fetchDataFromFirebase();
        lastFetchTime = millis(); // Update the last fetch time
    }

    // Receive SensorData1 array via Serial
    if (mySerial.available() >= sizeof(SensorData1) * SENSOR_COUNT) {
        mySerial.readBytes((char*)&receivedSensorDataArray, sizeof(SensorData1) * SENSOR_COUNT);
        Serial.println("\n✅ Received SensorData1:");
        for (int i = 0; i < SENSOR_COUNT; i++) {
            Serial.print("📦 Sensor ID: ");
            Serial.print(receivedSensorDataArray[i].sensorID);
            Serial.print(", Box Count: ");
            Serial.print(receivedSensorDataArray[i].boxCount);
            // Print new fields
            Serial.print(", Auto Mode: ");
            Serial.print(receivedSensorDataArray[i].autoMode ? "true" : "false");
            Serial.print(", Auto Box Size: ");
            Serial.println(receivedSensorDataArray[i].autoBoxSize);
        }
        // Upload received data to Firebase
        uploadReceivedDataToFirebase();
    }

    // Send SensorData2 array (including rackLength) via Serial
    mySerial.write((uint8_t*)&sensorDataArrayToSend, sizeof(SensorData2) * SENSOR_COUNT);
    Serial.println("\n📡 Sent SensorData2 array (with RackLength)");
    // Debug output for SensorData2
    for (int i = 0; i < SENSOR_COUNT; i++) {
        Serial.print("➡️ Sent -> SensorID: ");
        Serial.print(sensorDataArrayToSend[i].sensorID);
        Serial.print(", BoxSize: ");
        Serial.print(sensorDataArrayToSend[i].boxSize);
        Serial.print(", RackLength: ");
        Serial.println(sensorDataArrayToSend[i].rackLength);
    }

    delay(1000);  // Prevent loop overload
}

// Fetch Rack Length from Firebase (stored at /StoredData/<rackid>/rackLength)
// This function now always updates the global rackLength and each SensorData2 struct.
void fetchRackLengthFromFirebase() {
    Serial.println("\n📡 Fetching Rack Length from Firebase...");
    String rackPath = "/StoredData/" + String(rackid) + "/rackLength";
    if (Firebase.getInt(firebaseData, rackPath.c_str())) {
        int newRackLength = firebaseData.intData();
        // Always update, even if equal, to ensure our array is current
        rackLength = newRackLength;
        Serial.print("✅ Updated Global Rack Length: ");
        Serial.println(rackLength);
        for (int i = 0; i < SENSOR_COUNT; i++) {
            sensorDataArrayToSend[i].rackLength = rackLength;
        }
    } else {
        Serial.println("⚠️ Failed to fetch Rack Length, keeping default value.");
    }
}

// Fetch SensorData2 from Firebase using actual sensorID as key, from path /StoredData/<rackid>/sensors/<sensorID>
void fetchDataFromFirebase() {
    Serial.println("\n📡 Fetching SensorData from Firebase...");
    for (int i = 0; i < SENSOR_COUNT; i++) {
        String path = "/StoredData/" + String(rackid) + "/sensors/" + String(sensorDataArrayToSend[i].sensorID);
        if (Firebase.getJSON(firebaseData, path.c_str())) {
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, firebaseData.jsonObject().raw());
            if (!error) {
                if (doc.containsKey("sensorID")) {
                    sensorDataArrayToSend[i].sensorID = doc["sensorID"];
                }
                if (doc.containsKey("boxSize")) {
                    sensorDataArrayToSend[i].boxSize = doc["boxSize"];
                }
            } else {
                Serial.print("⚠️ Error deserializing JSON for sensor ");
                Serial.print(sensorDataArrayToSend[i].sensorID);
                Serial.println();
            }
        } else {
            Serial.print("⚠️ Failed to fetch sensor data for sensor ");
            Serial.print(sensorDataArrayToSend[i].sensorID);
            Serial.println(", keeping default values.");
        }
    }
    Serial.println("✅ Data Fetch Complete.");
}

// Upload received SensorData1 to Firebase (Update fields) to path /ReceivedData/<rackid>/Sensor_<sensorID>
void uploadReceivedDataToFirebase() {
    Serial.println("\n📡 Updating sensor data in Firebase...");
    for (int i = 0; i < SENSOR_COUNT; i++) {
        String path = "/ReceivedData/" + String(rackid) + "/Sensor_" + String(receivedSensorDataArray[i].sensorID);
        FirebaseJson json;
        json.set("boxCount", receivedSensorDataArray[i].boxCount);
        json.set("autoMode", receivedSensorDataArray[i].autoMode);
        json.set("autoBoxSize", receivedSensorDataArray[i].autoBoxSize);
        if (Firebase.updateNode(firebaseData, path.c_str(), json)) {
            Serial.print("✅ Updated Sensor ");
            Serial.print(receivedSensorDataArray[i].sensorID);
            Serial.println(" data successfully!");
        } else {
            Serial.print("⚠️ Failed to update Sensor ");
            Serial.print(receivedSensorDataArray[i].sensorID);
            Serial.print(": ");
            Serial.println(firebaseData.errorReason());
        }
    }
}

