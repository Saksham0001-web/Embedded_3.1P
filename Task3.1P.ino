#include <Wire.h>              // this is Library for I2C communication
#include <BH1750.h>            // this is Library for BH1750 light sensor
#include <WiFiNINA.h>          // this is Library for WiFi on Nano 33 IoT
#include <ArduinoHttpClient.h> // this is Library for making HTTP/HTTPS requests

// ==================== USER SETTINGS ====================
const char* WIFI_SSID = "SAKSHAM 8584";   // This is my WiFi network name
const char* WIFI_PASS = "Saksham@1424";         // WiFi password

const char* IFTTT_HOST = "maker.ifttt.com";    // IFTTT server host
const int   IFTTT_PORT = 443;                  // HTTPS port

// Direct Webhook paths
const char* PATH_START = "/trigger/sunlight_started/with/key/hZZptGi_h7OsDv8yQR8xrQcY5KO5ctWqMArkv_ozJ0R";
const char* PATH_STOP  = "/trigger/sunlight_stopped/with/key/hZZptGi_h7OsDv8yQR8xrQcY5KO5ctWqMArkv_ozJ0R";

// ---------------- Indoor Test Thresholds ----------------
// Room light ~150 lux, Torchlight goes above 200 lux.
// These values decide when "sunlight started" or "stopped" is triggered
const float SUNLIGHT_THRESHOLD_HIGH = 200.0;  // Trigger START when lux >= this
const float SUNLIGHT_THRESHOLD_LOW  = 150.0;  // Trigger STOP when lux <= this
// --------------------------------------------------------

BH1750 lightMeter;                   // This will create BH1750 sensor object
WiFiSSLClient ssl;                   // SSL client for secure HTTPS connection
HttpClient http(ssl, IFTTT_HOST, IFTTT_PORT); // HTTP client object

bool inSun = false;                  // This will track whether currently in sunlight
unsigned long stateChangeMs = 0;     // The time when state last changed
unsigned long sunAccumTodayMs = 0;   // Total sunlight accumulated today (in ms)
unsigned long dayStartMs = 0;        // This is the starting time of the day counter

// ==================== WIFI ====================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return; // If already connected, do nothing

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);          // Start WiFi connection
  while (WiFi.status() != WL_CONNECTED) {    // Keep trying until connected
    delay(500);                              // Wait half a second
    Serial.print(".");                       // Print dots while waiting
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());            // Print assigned IP address
}

// ==================== SENSOR ====================
float readLux() {
  return lightMeter.readLightLevel(); // Read light level from BH1750 in lux
}

// ==================== IFTTT ====================
bool sendIFTTTEvent(const char* path, const String& v1="", const String& v2="", const String& v3="") {
  connectWiFi(); // Ensure WiFi is connected before sending request

  // Build JSON body with up to 3 values (value1, value2, value3)
  String body = "{";
  if (v1.length()) { body += "\"value1\":\"" + v1 + "\""; }
  if (v2.length()) { if (v1.length()) body += ","; body += "\"value2\":\"" + v2 + "\""; }
  if (v3.length()) { if (v1.length() || v2.length()) body += ","; body += "\"value3\":\"" + v3 + "\""; }
  body += "}";

  http.beginRequest();                       // Start HTTP request
  http.post(path);                           // Send POST request to given path
  http.sendHeader("Content-Type", "application/json"); // Send header
  http.sendHeader("Content-Length", body.length());    // Send content length
  http.endRequest();                         // End headers section
  http.print(body);                          // Send request body

  int statusCode = http.responseStatusCode(); // Get HTTP response code
  String response = http.responseBody();      // Get HTTP response body

  Serial.print("IFTTT status: ");
  Serial.println(statusCode);                 // Print status code

  return (statusCode >= 200 && statusCode < 300); // Return true if success
}

// ==================== STATE MACHINE ====================
void checkSunlight() {
  float lux = readLux();                      // Read light level from sensor
  unsigned long now = millis();               // Get current time in ms

  Serial.print("Lux: ");
  Serial.println(lux);                        // Print lux value

  if (!inSun && lux >= SUNLIGHT_THRESHOLD_HIGH) {
    // If not currently in sunlight AND lux goes above high threshold
    inSun = true;                             // Update state to "in sunlight"
    stateChangeMs = now;                      // Record the time of change
    sendIFTTTEvent(PATH_START, "Lux=" + String(lux)); // Notify IFTTT
    Serial.println("☀ Light STARTED (Indoor Test)");
  }
  else if (inSun && lux <= SUNLIGHT_THRESHOLD_LOW) {
    // If currently in sunlight AND lux falls below low threshold
    inSun = false;                            // Update state to "not in sunlight"
    unsigned long thisBurst = now - stateChangeMs; // Duration of last sunlight burst
    sunAccumTodayMs += thisBurst;             // Add it to total sunlight for the day

    // Send STOP event to IFTTT with accumulated time info
    sendIFTTTEvent(PATH_STOP, 
                   "Total minutes today=" + String(sunAccumTodayMs/60000),
                   "Last burst seconds=" + String(thisBurst/1000),
                   "Lux=" + String(lux));
    Serial.println("🌑 Light STOPPED (Indoor Test)");
  }

  // Reset daily counter every 24 hours
  if (now - dayStartMs >= 24UL*60UL*60UL*1000UL) {
    sunAccumTodayMs = 0;                      // Reset daily sunlight
    dayStartMs = now;                         // Restart day counter
  }
}

// ==================== SETUP + LOOP ====================
void setup() {
  Serial.begin(115200);                       // Start serial communication
  Wire.begin();                               // Initialize I2C bus

  lightMeter.begin();                         // Initialize BH1750 sensor
  Serial.println("BH1750 started.");          // Print status

  connectWiFi();                              // Connect to WiFi

  dayStartMs = millis();                      // Initialize day counter
}

void loop() {
  checkSunlight();                            // Check light and handle events
  delay(1000);                                // Wait 1 second before next check
}
