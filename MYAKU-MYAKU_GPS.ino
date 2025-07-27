#include <M5Unified.h>
#include "MultipleSatellite.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Ambient.h>
#include <time.h>
#include <Preferences.h>

// ===== Settings =====
// WiFi settings
#define WIFI_SSID "xxxxxxxxxx"
#define WIFI_PASSWORD "xxxxxxxxxxx"

// Ambient settings
#define AMBIENT_CHANNEL_ID xxxxx
#define AMBIENT_WRITE_KEY "xxxxxxxxxxxxxxxxxxx"

// GAS Webhook settings
#define GAS_WEBHOOK_URL "https://script.google.com/macros/s/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx/exec"

// GPS settings
static const int RXPin = 33, TXPin = 32;
static const uint32_t GPSBaud = 115200;

// Transmission logic settings
#define INITIAL_WAIT_MS 2000      // Stabilization wait time after boot (milliseconds)
#define CHECK_INTERVAL_MS 5000     // Position check interval (milliseconds)
#define LCD_UPDATE_INTERVAL_MS 1000 // Screen update interval (milliseconds)
#define WIFI_CHECK_INTERVAL_MS 300000 // WiFi connection check interval (5 minutes)

// Global variables
WiFiClient client;
Ambient ambient;
Preferences preferences;
MultipleSatellite gps(Serial1, GPSBaud, SERIAL_8N1, RXPin, TXPin);

// Choices for movement distance threshold to trigger sending
const int distanceThresholds[] = {50, 100, 200, 500, 1000, 2000, 5000};
const int numThresholds = sizeof(distanceThresholds) / sizeof(distanceThresholds[0]);
int currentThresholdIndex = 2; // Default is 200m (index 2)
int currentDistanceThreshold = 200;

// Previously sent location information
double previousLat = 0.0;
double previousLng = 0.0;

// Control flags/timers
bool initialSendDone = false;
unsigned long lastCheckTime = 0;
unsigned long lastLcdUpdateTime = 0;
unsigned long lastWifiCheckTime = 0;
String wifiStatus = "OK";

// Function prototypes
void ensureWifiConnected();
bool sendToAmbient(double distance, double altitude, double lat, double lng);
bool sendToGAS(double distance, double altitude, double lat, double lng);
void updateLcdDisplay();
double calculateDistance(double lat1, double lon1, double lat2, double lon2);

// Display background drawing
bool myakumyakuBackDisplay = true;

// Canvas for eye animation
M5Canvas eyeCanvas(&M5.Display);

// Checks WiFi connection and reconnects if disconnected
void ensureWifiConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Reconnecting...");
        wifiStatus = "NG";
        M5.Lcd.setTextFont(1);
        M5.Lcd.setTextColor(WHITE, BLUE);
        M5.Display.fillRect(5, 125, 60, 10, BLUE);
        M5.Display.setCursor(5, 125); 
        M5.Display.printf("WiFi: %s\n", wifiStatus.c_str());
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        int retry_count = 0;
        while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
            delay(500);
            Serial.print(".");
            retry_count++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi reconnected.");
            wifiStatus = "OK";
        } else {
            Serial.println("\nWiFi reconnection failed.");
            wifiStatus = "NG";
        }
    } else {
        wifiStatus = "OK";
    }
}

// Calculates the distance between two points (Haversine formula)
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000; // Earth's radius (meters)
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double deltaPhi = (lat2 - lat1) * M_PI / 180.0;
    double deltaLambda = (lon2 - lon1) * M_PI / 180.0;

    double a = sin(deltaPhi / 2) * sin(deltaPhi / 2) +
               cos(phi1) * cos(phi2) *
               sin(deltaLambda / 2) * sin(deltaLambda / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return R * c; // Distance in meters
}

// Updates the screen display
void updateLcdDisplay() {
    //M5.Display.clear();
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextColor(WHITE, BLUE);

    if(myakumyakuBackDisplay){
            // --- Start myakumyaku drawing ---
            // Fill the entire screen with blue
            M5.Display.fillScreen(BLUE);
            // --- Draw fixed parts of the face ---
            //--mouse--
            {
                M5Canvas canvas(&M5.Display);
                canvas.setColorDepth(M5.Display.getColorDepth());
                canvas.createSprite(80, 80);
                canvas.fillSprite(BLUE);
                canvas.setPivot(40, 40);
                canvas.fillEllipse(40, 40, 15, 35, WHITE);
                canvas.pushRotateZoom(116, 46, -10.0, 1.0, 1.0, BLUE);
                canvas.deleteSprite();
            } //mouse01
            M5.Display.fillEllipse(116, 43, 26, 19, BLUE); //mouse02
            M5.Display.fillRect(68, 0, 108, 51, BLUE); //mask of mouse01 and mouse02

            //--cell--
            M5.Display.fillCircle(124, 16, 16, RED); //cell01
            M5.Display.fillCircle(122, 36, 14, RED); //cell02
            M5.Display.fillCircle(144, 31, 14, RED); //cell03
            M5.Display.fillCircle(165, 47, 15, RED); //cell04
            M5.Display.fillCircle(170, 80, 20, RED); //cell05

            {
                M5Canvas canvas(&M5.Display);
                canvas.setColorDepth(M5.Display.getColorDepth());
                canvas.createSprite(56, 56);
                canvas.fillSprite(TFT_BLACK);
                canvas.setPivot(28, 28);
                canvas.fillEllipse(28, 28, 12, 26, RED);
                canvas.pushRotateZoom(150, 100, -18.0, 1.0, 1.0, TFT_BLACK);
                canvas.deleteSprite();
            } // cell06

            M5.Display.fillEllipse(128, 105, 16, 15, RED); //cell07
            M5.Display.fillEllipse(97, 113, 19, 20, RED); //cell08
            M5.Display.fillCircle(69, 92, 19, RED); //cell09
            M5.Display.fillCircle(62, 65, 12, RED); //cell10

            {
                M5Canvas canvas(&M5.Display);
                canvas.setColorDepth(M5.Display.getColorDepth());
                canvas.createSprite(40, 40);
                canvas.fillSprite(TFT_BLACK);
                canvas.setPivot(20, 20);
                canvas.fillEllipse(20, 20, 16, 11, RED);
                canvas.pushRotateZoom(78, 47, -21.0, 1.0, 1.0, TFT_BLACK);
                canvas.deleteSprite();
            }  // cell11

            M5.Display.fillCircle(96, 30, 18, RED); //cell12

            //--whiteEye--
            M5.Display.fillCircle(176, 74, 8, WHITE); //whiteEye02
            M5.Display.fillCircle(99, 120, 9, WHITE); //whiteEye03
            M5.Display.fillCircle(59, 88, 7, WHITE); //whiteEye04
            M5.Display.fillCircle(95, 35, 8, WHITE); //whiteEye05
            //--blueEye--
            M5.Display.fillCircle(180, 77, 4, BLUE); //blueEye02
            M5.Display.fillCircle(103, 124, 4, BLUE); //blueEye03
            M5.Display.fillCircle(57, 90, 3, BLUE); //blueEye04
            M5.Display.fillCircle(98, 34, 4, BLUE); //blueEye05

            // Prepare Canvas for animation of whiteEye01
            eyeCanvas.setColorDepth(M5.Display.getColorDepth());
            eyeCanvas.createSprite(20, 20);

            myakumyakuBackDisplay = false;
    }else{
        // Number of satellites
            M5.Display.fillRect(5, 110, 40, 10, BLUE);
            M5.Display.setCursor(5, 110); 
            M5.Display.printf("Sats: %d\n", gps.satellites.value());

        if (gps.location.isValid()) {
            // Latitude
            M5.Display.fillRect(5, 5, 90, 10, BLUE);
            M5.Display.setCursor(5, 5); 
            M5.Display.printf("Lat: %.4f\n", gps.location.lat());
            // Longitude
            M5.Display.fillRect(150, 5, 90, 10, BLUE);
            M5.Display.setCursor(150, 5); 
            M5.Display.printf("Lng: %.4f\n", gps.location.lng());

            if (initialSendDone) {
                double distance = calculateDistance(previousLat, previousLng, gps.location.lat(), gps.location.lng());
                // Traveled distance
                M5.Display.fillRect(175, 110, 65, 10, BLUE);
                M5.Display.setCursor(175, 110); 
                M5.Display.printf("Dist: %.0fm\n", distance);
            } else {
                M5.Display.fillRect(175, 110, 65, 10, BLUE);
                M5.Display.setCursor(175, 110); 
                M5.Display.println("Wait send...");
            }
        } else {
            M5.Display.fillRect(5, 110, 40, 10, BLUE);
            M5.Display.setCursor(5, 110); 
            M5.Display.println("No GPS fix");
        }
        // Send threshold
        M5.Display.fillRect(175, 125, 65, 10, BLUE);
        M5.Display.setCursor(175, 125); 
        M5.Display.printf("Thr: %dm\n", currentDistanceThreshold);

        // WiFi connection status
        M5.Display.fillRect(5, 125, 60, 10, BLUE);
        M5.Display.setCursor(5, 125); 
        M5.Display.printf("WiFi: %s\n", wifiStatus.c_str());
    }
}

// Send data to Ambient
bool sendToAmbient(double distance, double altitude, double lat, double lng) {
    ensureWifiConnected(); // Check WiFi connection before sending
    if (WiFi.status() != WL_CONNECTED) return false;

    static bool ambientInitialized = false;
    if (!ambientInitialized) {
        if (!ambient.begin(AMBIENT_CHANNEL_ID, AMBIENT_WRITE_KEY, &client)) {
            Serial.println("Ambient begin failed");
            return false;
        }
        ambientInitialized = true;
    }

    char latbuf[16], lngbuf[16];

    ambient.set(1, distance);
    ambient.set(2, altitude);
    dtostrf(lat, 11, 7, latbuf);
    ambient.set(9, latbuf);
    dtostrf(lng, 11, 7, lngbuf);
    ambient.set(10, lngbuf);

    if (ambient.send()) {
        Serial.println("Ambient data sent successfully");
        return true;
    } else {
        Serial.println("Ambient data send failed");
        ambientInitialized = false;
        return false;
    }
}

// Send data to GAS
bool sendToGAS(double distance, double altitude, double lat, double lng) {
    ensureWifiConnected(); // Check WiFi connection before sending
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client_secure;
    HTTPClient http;
    client_secure.setInsecure();

    if (http.begin(client_secure, GAS_WEBHOOK_URL)) {
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(10000); // 10 second timeout

        StaticJsonDocument<256> doc;
        String jsonString;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[20];
            snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            doc["timestamp"] = String(timeStr);
        } else {
            doc["timestamp"] = "N/A";
        }
        doc["distance"] = distance;
        doc["altitude"] = altitude;
        doc["latitude"] = lat;
        doc["longitude"] = lng;
        serializeJson(doc, jsonString);

        int httpResponseCode = http.POST(jsonString);

        if (httpResponseCode > 0) {
            Serial.printf("GAS send successful - HTTP Response code: %d\n", httpResponseCode);
            http.end();
            return true;
        } else {
            Serial.printf("GAS send failed - Error code: %d, %s\n", httpResponseCode, http.errorToString(httpResponseCode).c_str());
            http.end();
            return false;
        }
    } else {
        Serial.println("HTTPS connection failed");
        return false;
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    // Font settings
    // M5.Display.setFont(&fonts::NotoSans_Bold15); // Use standard font
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    // Load settings from NVS
    preferences.begin("gps-tracker", false);
    currentThresholdIndex = preferences.getUInt("distIndex", 2);
    if (currentThresholdIndex >= numThresholds) currentThresholdIndex = 2;
    currentDistanceThreshold = distanceThresholds[currentThresholdIndex];

    M5.Display.setRotation(1);
    M5.Display.setCursor(5, 5);
    M5.Display.print("Searching GPS...");

    Serial.println("=== GPS Initialization ===");
    gps.begin();
    gps.setSystemBootMode(BOOT_FACTORY_START);
    gps.setSatelliteMode(SATELLITE_MODE_GPS);

    Serial.print("Waiting for GPS satellite lock...");
    while (gps.satellites.value() < 3 || !gps.location.isValid()) {
        gps.updateGPS();
        Serial.print(".");
        M5.Display.print(".");
        delay(500);
    }
    Serial.println("\nGPS satellite locked!");
    M5.Display.clear();
    M5.Display.setCursor(5, 5);
    M5.Display.print("GPS Locked.");

    Serial.println("=== Starting Network Services ===");
    M5.Display.setCursor(5, 25);
    M5.Display.print("Connecting WiFi...");
    ensureWifiConnected(); // Call WiFi connection function

    if(WiFi.status() == WL_CONNECTED) {
        M5.Display.setCursor(5, 45);
        M5.Display.print("WiFi Connected.");
        configTime(9 * 3600, 0, "ntp.nict.jp", "time.google.com", "pool.ntp.org");
    } else {
        M5.Display.setCursor(5, 45);
        M5.Display.print("WiFi Failed.");
    }

    M5.Display.setCursor(5, 65);
    M5.Display.print("Stabilizing GPS...");
    Serial.printf("Waiting for %d seconds to stabilize GPS...\n", INITIAL_WAIT_MS / 1000);
    delay(INITIAL_WAIT_MS);

    Serial.println("Sending initial location...");
    if (gps.location.isValid()) {
        double currentLat = gps.location.lat();
        double currentLng = gps.location.lng();
        double currentAltitude = gps.altitude.meters();

        bool ambientSuccess = sendToAmbient(0.0, currentAltitude, currentLat, currentLng);
        bool gasSuccess = sendToGAS(0.0, currentAltitude, currentLat, currentLng);

        if(ambientSuccess && gasSuccess) {
            Serial.println("Initial location sent successfully.");
        } else {
            Serial.println("Warning: Initial location send failed. Will retry later.");
        }

        previousLat = currentLat;
        previousLng = currentLng;
        initialSendDone = true;
        lastCheckTime = millis();

        Serial.println("Initial location stored.");
    } else {
        Serial.println("Failed to get valid location for initial send.");
        M5.Display.clear();
        M5.Display.setCursor(5, 5);
        M5.Display.print("Error: GPS invalid.");
    }
    lastWifiCheckTime = millis(); // Initialize WiFi check timer
}

void loop() {
    gps.updateGPS();
    M5.update();

    // Manual send with button A
    if (M5.BtnA.wasPressed()) {
        Serial.println("Button A pressed. Manual send triggered.");
        if (gps.location.isValid()) {
            double currentLat = gps.location.lat();
            double currentLng = gps.location.lng();
            double currentAltitude = gps.altitude.meters();
            double distance = calculateDistance(previousLat, previousLng, currentLat, currentLng);

            Serial.println("Sending current location manually...");
            bool ambientSuccess = sendToAmbient(distance, currentAltitude, currentLat, currentLng);
            bool gasSuccess = sendToGAS(distance, currentAltitude, currentLat, currentLng);

            if (ambientSuccess && gasSuccess) {
                previousLat = currentLat;
                previousLng = currentLng;
                Serial.println("Manual send successful. Base location updated.");
            } else {
                Serial.println("Manual send failed. Base location not updated.");
            }
            updateLcdDisplay(); // Update screen immediately
        } else {
            Serial.println("Manual send failed: No valid GPS data.");
        }
    }

    // Change send distance threshold with button B
    if (M5.BtnB.wasPressed()) {
        currentThresholdIndex = (currentThresholdIndex + 1) % numThresholds;
        currentDistanceThreshold = distanceThresholds[currentThresholdIndex];
        preferences.putUInt("distIndex", currentThresholdIndex);
        Serial.printf("Distance threshold changed to: %d m\n", currentDistanceThreshold);
        updateLcdDisplay(); // Update screen immediately
    }

    // Update screen display (every 1 second)
    if (millis() - lastLcdUpdateTime >= LCD_UPDATE_INTERVAL_MS) {
        updateLcdDisplay();
        lastLcdUpdateTime = millis();
    }

    // Check WiFi connection status (every 5 minutes)
    if (millis() - lastWifiCheckTime >= WIFI_CHECK_INTERVAL_MS) {
        myakumyakuBackDisplay = true;
        ensureWifiConnected();
        lastWifiCheckTime = millis();
    }

    // Automatic send logic when moved a set distance (every 5 seconds)
    if (initialSendDone && (millis() - lastCheckTime >= CHECK_INTERVAL_MS)) {
        lastCheckTime = millis();

        if (gps.location.isValid()) { // Removed isUpdated() check
            double currentLat = gps.location.lat();
            double currentLng = gps.location.lng();
            double currentAltitude = gps.altitude.meters();

            double distance = calculateDistance(previousLat, previousLng, currentLat, currentLng);
            Serial.printf("Distance from last point: %.2f m\n", distance);

            if (distance >= currentDistanceThreshold) {
                Serial.printf("Threshold exceeded. Sending new location...\n");

                bool ambientSuccess = sendToAmbient(distance, currentAltitude, currentLat, currentLng);
                bool gasSuccess = sendToGAS(distance, currentAltitude, currentLat, currentLng);

                if (ambientSuccess && gasSuccess) {
                    Serial.println("Both sends successful. Updating base location.");
                    previousLat = currentLat;
                    previousLng = currentLng;
                } else {
                    Serial.println("One or more sends failed. Will retry with same base location.");
                }
            }
        }
    }
    // move animation of Eye01
    // move speed = 1cycle/4seconds
    float angle = (millis() % 4000) * 2 * PI / 4000.0;

    // center of whiteEye01 = (10,10), radius= 4pixels
    float blue_x = 10 + 4 * cos(angle - PI / 4);
    float blue_y = 10 - 4 * sin(angle - PI / 4);

    // draw in Canvas
    eyeCanvas.fillSprite(RED);
    eyeCanvas.fillCircle(10, 10, 8, WHITE); // whiteEye01
    eyeCanvas.fillCircle(blue_x, blue_y, 4, BLUE);   // blueEye01

    // transfer Canvas to the position as center of whiteEye01 = (124,11)
    eyeCanvas.pushSprite(124 - 10, 11 - 10);

    delay(10);

}
