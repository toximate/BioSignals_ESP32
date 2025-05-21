#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"  // for token status callback

// WiFi credentials
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// Firebase credentials
#define FIREBASE_API_KEY "your_firebase_api_key" 
#define FIREBASE_DATABASE_URL "your_firebase_db_url"

// User credentials
#define USER_EMAIL "your_firebase_email"
#define USER_PASSWORD "your_firebase_password"

// Sensor config
#define PPG_PIN 35
#define ECG_PIN 36
#define SAMPLE_RATE 100        // Hz
#define WINDOW_SIZE 10         // Moving average window size for PPG
#define IIR_ALPHA 0.2          // IIR filter smoothing factor for PPG

// ECG filter parameters (from second code)
#define ECG_MOVING_AVG_SIZE 15
#define ECG_ALPHA 0.15

// Data collection
#define BATCH_SIZE 50
#define FIREBASE_INTERVAL 180000  // 3 minutes (milliseconds)
#define AUTH_RETRY_DELAY 60000    // 1 minute

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Buffers
struct SensorData {
  float ppg;
  float ecg;
  uint32_t timestamp;
};
SensorData dataBuffer[BATCH_SIZE];
int bufferIndex = 0;

// PPG filtering
float ppgBuffer[WINDOW_SIZE] = {0};
int bufIndex = 0;
float filteredPPG = 0;

// ECG filtering buffers and variables
float ecgDcOffset = 0.0;
int ecgRawValues[ECG_MOVING_AVG_SIZE] = {0};
int ecgBufferIndex = 0;
float filteredECG = 0.0;

// Auth control
unsigned long lastAuthAttempt = 0;
bool authInProgress = false;

// Timing
unsigned long lastFirebaseSend = 0;

bool checkFirebaseAuth() {
  if (Firebase.ready()) return true;

  if (!authInProgress && millis() - lastAuthAttempt > AUTH_RETRY_DELAY) {
    authInProgress = true;
    lastAuthAttempt = millis();

    Serial.println("Attempting Firebase auth...");
    Firebase.reconnectWiFi(true);
    authInProgress = false;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long wifiTimeout = millis() + 20000;
  while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed. Restarting...");
    ESP.restart();
  }

  Serial.println("\nWiFi connected");

  // Firebase setup
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;
  config.timeout.serverResponse = 10 * 1000;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase initialized");
}

float processPPG(float rawValue) {
  ppgBuffer[bufIndex] = rawValue;
  bufIndex = (bufIndex + 1) % WINDOW_SIZE;

  float avg = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    avg += ppgBuffer[i];
  }
  avg /= WINDOW_SIZE;

  filteredPPG = IIR_ALPHA * avg + (1 - IIR_ALPHA) * filteredPPG;

  return (filteredPPG - 2000) * 0.2;  // PPG_RESTING_ADC and PPG_SCALE_FACTOR
}

float processECG(int rawECG) {
  // DC offset removal (IIR filter)
  ecgDcOffset = ECG_ALPHA * rawECG + (1 - ECG_ALPHA) * ecgDcOffset;
  int centeredValue = rawECG - ecgDcOffset;

  // Moving average filter
  ecgRawValues[ecgBufferIndex] = centeredValue;
  ecgBufferIndex = (ecgBufferIndex + 1) % ECG_MOVING_AVG_SIZE;

  long sum = 0;
  for (int i = 0; i < ECG_MOVING_AVG_SIZE; i++) {
    sum += ecgRawValues[i];
  }
  filteredECG = sum / (float)ECG_MOVING_AVG_SIZE;

  // Scale and offset for visualization
  float scaledECG = filteredECG * 0.1 - 1000;

  return scaledECG;
}

void sendToFirebase() {
  if (bufferIndex == 0) return;

  FirebaseJson jsonData;
  jsonData.clear();

  FirebaseJsonArray dataPointsArray;
  dataPointsArray.clear();

  for (int i = 0; i < bufferIndex; i++) {
    FirebaseJson dataPoint;
    dataPoint.set("ppg", dataBuffer[i].ppg);
    dataPoint.set("ecg", dataBuffer[i].ecg);
    dataPoint.set("timestamp", dataBuffer[i].timestamp);
    dataPointsArray.add(dataPoint);
  }

  jsonData.set("dataPoints", dataPointsArray);
  jsonData.set("sample_rate", SAMPLE_RATE);

  String path = "/sensor_data/" + String(millis() / 1000);

  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &jsonData)) {
    Serial.println("Upload successful");
  } else {
    Serial.printf("Failed to upload data: %s\n", fbdo.errorReason().c_str());
  }
}

void loop() {
  static unsigned long lastSample = 0;
  static unsigned long lastHeapCheck = 0;

  if (millis() - lastHeapCheck > 5000) {
    lastHeapCheck = millis();
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  }

  if (micros() - lastSample < (1000000 / SAMPLE_RATE)) return;
  lastSample = micros();

  if (ESP.getFreeHeap() < 4000) {
    Serial.println("Low memory, skipping sample");
    delay(100);
    return;
  }

  float ppgRaw = analogRead(PPG_PIN);
  int ecgRaw = analogRead(ECG_PIN);

  float ppgPerfusion = processPPG(ppgRaw);
  float ecgVoltage = processECG(ecgRaw);

  if (bufferIndex < BATCH_SIZE) {
    dataBuffer[bufferIndex] = {ppgPerfusion, ecgVoltage, millis()};
    bufferIndex++;
  }

  if ((bufferIndex >= BATCH_SIZE || (millis() - lastFirebaseSend >= FIREBASE_INTERVAL && bufferIndex > 0))) {
    if (checkFirebaseAuth()) {
      sendToFirebase();
      bufferIndex = 0;
      lastFirebaseSend = millis();
    }
  }
}
