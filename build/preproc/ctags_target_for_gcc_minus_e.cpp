# 1 "C:\\Users\\kavee\\Desktop\\MECH3890\\VSCODE\\transmitter\\transmitter.ino"
# 2 "C:\\Users\\kavee\\Desktop\\MECH3890\\VSCODE\\transmitter\\transmitter.ino" 2
# 3 "C:\\Users\\kavee\\Desktop\\MECH3890\\VSCODE\\transmitter\\transmitter.ino" 2
# 4 "C:\\Users\\kavee\\Desktop\\MECH3890\\VSCODE\\transmitter\\transmitter.ino" 2
# 5 "C:\\Users\\kavee\\Desktop\\MECH3890\\VSCODE\\transmitter\\transmitter.ino" 2

// WiFi Credentials



// Firebase Configuration



// Firebase Objects
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Serial Communication
HardwareSerial mySerial(1);

// **** Global Editable Variables ****
long rackid = 1110001; // Global rackid variable (editable)
// **********************************

// Struct Definitions
struct SensorData1 {
    long sensorID;
    int boxCount;
    bool autoMode; // NEW: Added autoMode field
    int autoBoxSize; // NEW: Added autoBoxSize field
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
    Serial0.begin(115200);
    mySerial.begin(9600, SERIAL_8N1, 4, 5);

    Serial0.println("\nStarting ESP32...");

    // Connect to WiFi
    Serial0.print("Connecting to WiFi...");
    WiFi.begin("Horny signals in your area", "hn6vxMhpgyth");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        Serial0.print(".");
        delay(1000);
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial0.println("\n✅ Connected to WiFi!");
    } else {
        Serial0.println("\n⚠️ WiFi Connection Failed! Continuing with default values...");
    }

    // Firebase Initialization
    config.host = "https://logistics-parts-tracking-default-rtdb.europe-west1.firebasedatabase.app/";
    config.signer.tokens.legacy_token = "P4NyJrff21NJrsii689KLoSLNuslrMGiJEAhTqSE";
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial0.println("✅ Firebase Initialized!");
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
        Serial0.println("\n✅ Received SensorData1:");
        for (int i = 0; i < SENSOR_COUNT; i++) {
            Serial0.print("📦 Sensor ID: ");
            Serial0.print(receivedSensorDataArray[i].sensorID);
            Serial0.print(", Box Count: ");
            Serial0.print(receivedSensorDataArray[i].boxCount);
            // Print new fields
            Serial0.print(", Auto Mode: ");
            Serial0.print(receivedSensorDataArray[i].autoMode ? "true" : "false");
            Serial0.print(", Auto Box Size: ");
            Serial0.println(receivedSensorDataArray[i].autoBoxSize);
        }
        // Upload received data to Firebase
        uploadReceivedDataToFirebase();
    }

    // Send SensorData2 array (including rackLength) via Serial
    mySerial.write((uint8_t*)&sensorDataArrayToSend, sizeof(SensorData2) * SENSOR_COUNT);
    Serial0.println("\n📡 Sent SensorData2 array (with RackLength)");
    // Debug output for SensorData2
    for (int i = 0; i < SENSOR_COUNT; i++) {
        Serial0.print("➡️ Sent -> SensorID: ");
        Serial0.print(sensorDataArrayToSend[i].sensorID);
        Serial0.print(", BoxSize: ");
        Serial0.print(sensorDataArrayToSend[i].boxSize);
        Serial0.print(", RackLength: ");
        Serial0.println(sensorDataArrayToSend[i].rackLength);
    }

    delay(1000); // Prevent loop overload
}

// Fetch Rack Length from Firebase (stored at /StoredData/<rackid>/rackLength)
// This function now always updates the global rackLength and each SensorData2 struct.
void fetchRackLengthFromFirebase() {
    Serial0.println("\n📡 Fetching Rack Length from Firebase...");
    String rackPath = "/StoredData/" + String(rackid) + "/rackLength";
    if (Firebase.getInt(firebaseData, rackPath.c_str())) {
        int newRackLength = firebaseData.intData();
        // Always update, even if equal, to ensure our array is current
        rackLength = newRackLength;
        Serial0.print("✅ Updated Global Rack Length: ");
        Serial0.println(rackLength);
        for (int i = 0; i < SENSOR_COUNT; i++) {
            sensorDataArrayToSend[i].rackLength = rackLength;
        }
    } else {
        Serial0.println("⚠️ Failed to fetch Rack Length, keeping default value.");
    }
}

// Fetch SensorData2 from Firebase using actual sensorID as key, from path /StoredData/<rackid>/sensors/<sensorID>
void fetchDataFromFirebase() {
    Serial0.println("\n📡 Fetching SensorData from Firebase...");
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
                Serial0.print("⚠️ Error deserializing JSON for sensor ");
                Serial0.print(sensorDataArrayToSend[i].sensorID);
                Serial0.println();
            }
        } else {
            Serial0.print("⚠️ Failed to fetch sensor data for sensor ");
            Serial0.print(sensorDataArrayToSend[i].sensorID);
            Serial0.println(", keeping default values.");
        }
    }
    Serial0.println("✅ Data Fetch Complete.");
}

// Upload received SensorData1 to Firebase (Update fields) to path /ReceivedData/<rackid>/Sensor_<sensorID>
void uploadReceivedDataToFirebase() {
    Serial0.println("\n📡 Updating sensor data in Firebase...");
    for (int i = 0; i < SENSOR_COUNT; i++) {
        String path = "/ReceivedData/" + String(rackid) + "/Sensor_" + String(receivedSensorDataArray[i].sensorID);
        FirebaseJson json;
        json.set("boxCount", receivedSensorDataArray[i].boxCount);
        json.set("autoMode", receivedSensorDataArray[i].autoMode);
        json.set("autoBoxSize", receivedSensorDataArray[i].autoBoxSize);
        if (Firebase.updateNode(firebaseData, path.c_str(), json)) {
            Serial0.print("✅ Updated Sensor ");
            Serial0.print(receivedSensorDataArray[i].sensorID);
            Serial0.println(" data successfully!");
        } else {
            Serial0.print("⚠️ Failed to update Sensor ");
            Serial0.print(receivedSensorDataArray[i].sensorID);
            Serial0.print(": ");
            Serial0.println(firebaseData.errorReason());
        }
    }
}
