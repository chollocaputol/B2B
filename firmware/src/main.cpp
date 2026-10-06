#include <Arduino.h>
#include <math.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <U8g2lib.h>    
#include <XPowersLib.h> 

#define GPS_RX_PIN 9    
#define GPS_TX_PIN 8    
#define GPS_BAUD   9600
#define GPS_WAKE_PIN 7  

#define OLED_SDA 17
#define OLED_SCL 18
#define PMU_SDA 42
#define PMU_SCL 41
#define USER_BUTTON_PIN 0  

XPowersAXP2101 PMU; 

// CRITICAL: Changed from U8G2_R0 to U8G2_R1 to force a complete 90-degree portrait layout swap!
// Resolution is now 64 pixels wide by 128 pixels tall.
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R1, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA);

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
TaskHandle_t TaskCore0;

// Interface Control State Variables
int currentScreenPage = 1; // Expanded tracking boundary: Page 1 to Page 4
unsigned long lastButtonPressTime = 0; 
const unsigned long DEBOUNCE_DELAY_MS = 250; 

// Thesis Navigation Coordinates
const double EARTH_RADIUS_KM = 6371.0;
const double SHORE_LAT = 8.250000; 
const double SHORE_LON = 124.250000; 

double toRadians(double degree) { return degree * (M_PI / 180.0); }
double calculateHaversineDistance(double nodeLat, double nodeLon) {
    double dLat = toRadians(nodeLat - SHORE_LAT);
    double dLon = toRadians(nodeLon - SHORE_LON);
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) + cos(toRadians(SHORE_LAT)) * cos(toRadians(nodeLat)) * sin(dLon / 2.0) * sin(dLon / 2.0);
    return EARTH_RADIUS_KM * (2.0 * atan2(sqrt(a), sqrt(1.0 - a)));
}

// Processing Execution Thread
void core0Task(void *pvParameters) {
  for (;;) {
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

    // Capture user manual button interaction clicks
    if (digitalRead(USER_BUTTON_PIN) == LOW) {
      unsigned long currentTime = millis();
      if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY_MS) {
        currentScreenPage++;
        if (currentScreenPage > 4) currentScreenPage = 1; // Cycle loop wrap
        lastButtonPressTime = currentTime;
      }
    }

    display.clearBuffer();

    // Ingest battery and operational runtime data
    unsigned long uptimeSeconds = millis() / 1000;
    int upMin = (uptimeSeconds / 60) % 60;
    int upSec = uptimeSeconds % 60;
    int battPct = PMU.getBatteryPercent(); 
    if (battPct > 100) battPct = 100;

    // ==================================================
    // PAGE 1: SMART-LOCKSCREEN STYLE DASHBOARD
    // ==================================================
    if (currentScreenPage == 1) {
      // Top battery flag indicator layout
      display.setFont(u8g2_font_profont11_tf);
      display.setCursor(35, 10); display.print(battPct); display.print("%");
      display.drawFrame(10, 15, 44, 2); // Simple border line breakout
      
      // Giant Digital Time block execution
      display.setFont(u8g2_font_logisoso24_tn); // Large bold numeric font profile
      if (gps.time.isValid()) {
        int localHour = (gps.time.hour() + 8) % 24; // Adjust for Philippine Standard Time
        
        display.setCursor(5, 55); 
        if (localHour < 10) display.print("0"); display.print(localHour);
        
        display.setFont(u8g2_font_logisoso24_tf);
        display.print(":");
        
        display.setFont(u8g2_font_logisoso24_tn);
        if (gps.time.minute() < 10) display.print("0"); display.print(gps.time.minute());
      } else {
        display.setFont(u8g2_font_profont12_tf);
        display.drawStr(5, 50, "Syncing...");
      }

      // Minimalist running uptime tracker positioned at canvas floor
      display.setFont(u8g2_font_profont11_tf);
      display.setCursor(5, 115);
      display.print("Up: "); display.print(upMin); display.print("m "); display.print(upSec); display.print("s");
    }
    
    // ==================================================
    // PAGE 2: RADAR RADIAL NAV TELEMETRY (HALVED GRID)
    // ==================================================
    else if (currentScreenPage == 2) {
      if (gps.location.isValid()) {
        double myLat = gps.location.lat();
        double myLon = gps.location.lng();
        double distanceToShore = calculateHaversineDistance(myLat, myLon);
        double mockNearestBoatDist = 1.45; // Simulated placeholder until we initialize LoRa networks next

        // Section 1: Distance to Shore Data Layout
        display.setFont(u8g2_font_profont10_tf);
        display.drawStr(2, 12, "SHORE DIST");
        
        display.setFont(u8g2_font_logisoso20_tn); // Medium-Large sharp text profile
        display.setCursor(2, 38); display.print(distanceToShore, 1);
        display.setFont(u8g2_font_profont12_tf); display.print(" km");

        // Center split dashboard partition line
        display.drawHLine(0, 48, 64);

        // Section 2: Distance to Nearest Vessel Data Layout
        display.setFont(u8g2_font_profont10_tf);
        display.drawStr(2, 64, "NEAREST BOAT");
        
        display.setFont(u8g2_font_logisoso20_tn);
        display.setCursor(2, 90); display.print(mockNearestBoatDist, 1);
        display.setFont(u8g2_font_profont12_tf); display.print(" km");
      } else {
        display.setFont(u8g2_font_profont11_tf);
        display.drawStr(2, 45, "Searching");
        display.drawStr(12, 60, "GPS...");
        
        // Print character debugging tracks silently down at the bottom row boundary
        display.setFont(u8g2_font_profont10_tf);
        display.setCursor(2, 110); display.print("Sats: "); display.print(gps.satellites.value());
      }
    }
    
    // ==================================================
    // PAGE 3: SIMPLIFIED FISHERMEN WEATHER OUTLOOK
    // ==================================================
    else if (currentScreenPage == 3) {
      display.setFont(u8g2_font_profont11_tf);
      display.drawStr(2, 15, "WEATHER");
      display.drawHLine(0, 22, 64);

      // Large central condition layout block
      display.setFont(u8g2_font_profont15_tf); 
      display.drawStr(5, 55, "FAIR SKY"); // Dynamic condition statement container

      display.setFont(u8g2_font_profont10_tf);
      display.drawStr(2, 85, "Pres: 1011 hPa");
      
      // Fixed Underline Time-Horizon Context Label
      display.drawHLine(0, 100, 64);
      display.setFont(u8g2_font_profont10_tf);
      display.drawStr(2, 115, "Next 12 Hours");
    }

    // ==================================================
    // PAGE 4: LOCAL MESH NETWORKING RADAR REGISTER
    // ==================================================
    else if (currentScreenPage == 4) {
      display.setFont(u8g2_font_profont11_tf);
      display.drawStr(2, 12, "MESH FLEET");
      display.drawHLine(0, 18, 64);

      // Clean list tracking relative node positions over data tracks
      display.setFont(u8g2_font_profont11_tf);
      display.drawStr(2, 38, "BOAT A");
      display.setFont(u8g2_font_profont10_tf); display.drawStr(2, 48, "-> 1.25 km");

      display.setFont(u8g2_font_profont11_tf);
      display.drawStr(2, 68, "BOAT B");
      display.setFont(u8g2_font_profont10_tf); display.drawStr(2, 78, "-> 3.68 km");

      display.setFont(u8g2_font_profont11_tf);
      display.drawStr(2, 98, "BOAT C");
      display.setFont(u8g2_font_profont10_tf); display.drawStr(2, 108, "-> 7.41 km");
    }

    display.sendBuffer(); 
    vTaskDelay(pdMS_TO_TICKS(100)); 
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GPS_WAKE_PIN, OUTPUT);
  digitalWrite(GPS_WAKE_PIN, HIGH); 
  delay(100);

  Wire1.begin(PMU_SDA, PMU_SCL, 100000); 
  if (!PMU.begin(Wire1, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
      PMU.begin(Wire1, 0x34, PMU_SDA, PMU_SCL);
  }

  PMU.setALDO1Voltage(3300); PMU.enableALDO1(); 
  PMU.setALDO2Voltage(3300); PMU.enableALDO2(); 
  PMU.setALDO3Voltage(3300); PMU.enableALDO3(); 
  PMU.setALDO4Voltage(3300); PMU.enableALDO4();        
  PMU.setBLDO1Voltage(3300); PMU.enableBLDO1();        
  delay(200); 

  Wire.begin(OLED_SDA, OLED_SCL, 100000);
  display.setI2CAddress(0x3C * 2); 
  display.begin();
  
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  xTaskCreatePinnedToCore(core0Task, "TaskCore0", 10000, NULL, 1, &TaskCore0, 0);
}

void loop() {
  vTaskDelete(NULL); 
}
