/*****************  Libraries  *****************/
#include <mcp_can.h>
#include <SPI.h>
#include <Wire.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <Fonts/FreeSans9pt7b.h>

/*****************  Pin map  *******************/
// TFT (HSPI)
#define TFT_DC 2
#define TFT_CS 15
#define TFT_MOSI 13
#define TFT_CLK 14
#define TFT_RST 27
#define TFT_MISO 12
// CAN (VSPI)
#define MCP_CS_PIN 5
#define MCP_INT_PIN 4

/*****************  SPI buses  *****************/
SPIClass tftHSPI(HSPI);

/*****************  Devices  *******************/
Adafruit_ILI9341 tft = Adafruit_ILI9341(&tftHSPI, TFT_DC, TFT_CS, TFT_RST);
MCP_CAN CAN(MCP_CS_PIN);

/*****************  Globals  *******************/
unsigned long lastCanMessageTime = 0;
const unsigned long inactivityTimeout = 10000;  // 10 seconds
int selectedScreen = 0;                         // 0: Main

String prevTemperature = "";
String prevVolt = "";
String prevRPM = "";
String prevPressure = "";

float temperatureVoltages[] = { 2.979, 1.586, 0.79, 0.354, 0.163, 0.102 };
float temperatureValues[] = { 21, 40, 70, 100, 130, 150 };

float pressureVoltages[] = { 0.065, 0.61, 1.194, 1.551, 1.703 };
float pressureValues[] = { 0, 2, 5, 8, 10 };

/*********  Display initialization  ************/
void initDisplay() {
  tftHSPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  delay(1000);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
}

/*********  MCP2515 initialization  ************/
void initCan() {
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    tft.setCursor(0, 50);
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(3);
    tft.println("CAN Init failed");
    tft.println("retrying . . .");
    delay(1000);
  }

  CAN.setMode(MCP_LISTENONLY);

  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 50);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(3);
  tft.println("CAN Init");
  tft.println("successful");
  delay(3000);

  tft.fillScreen(ILI9341_BLACK);
}

void setup() {
  Serial.begin(115200);

  pinMode(MCP_INT_PIN, INPUT_PULLUP);

  initDisplay();
  initCan();

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from CAN interrupt.");
  }

  esp_sleep_enable_ext0_wakeup((gpio_num_t)MCP_INT_PIN, 0);
  lastCanMessageTime = millis();

  // # TESTING #
  randomSeed(analogRead(35));
  // # TESTING #
}

float interpolate(float inputVoltage, float* voltage, float* value, int count) {
  for (int i = 0; i < count - 1; i++) {
    float v1 = voltage[i];
    float v2 = voltage[i + 1];
    float val1 = value[i];
    float val2 = value[i + 1];

    if ((v1 >= inputVoltage && inputVoltage >= v2) || (v2 >= inputVoltage && inputVoltage >= v1)) {
      return val1 + (inputVoltage - v1) * (val2 - val1) / (v2 - v1);
    }
  }

  // Outside range
  if (inputVoltage > voltage[0]) return value[0];
  if (inputVoltage < voltage[count - 1]) return value[count - 1];

  return -1;  // Error
}

void drawTextDiff(String newText, String& prevText, int x, int y, uint8_t textSize, uint16_t color) {
  int charW = 6 * textSize;
  int charH = 8 * textSize;

  tft.setTextSize(textSize);
  tft.setTextColor(color, ILI9341_BLACK);  // use background color to auto-clear

  for (int i = 0; i < newText.length(); i++) {
    char newChar = newText[i];
    char oldChar = (i < prevText.length()) ? prevText[i] : 0;

    if (newChar != oldChar) {
      tft.setCursor(x + i * charW, y);
      tft.print(newChar);
    }
  }

  prevText = newText;
}

void loop() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  float temperatureVoltage = float(analogReadMilliVolts(33)) / 1000;

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  float pressureVoltage = float(analogReadMilliVolts(34)) / 1000;

  float temperatureValue = interpolate(temperatureVoltage, temperatureVoltages, temperatureValues, 6);
  float pressureValue = interpolate(pressureVoltage, pressureVoltages, pressureValues, 5);
  
  int rpmValue = getStableRandomValue();

  String textTemperature = "TEMP: " + String(temperatureValue, 0);
  String textVolt = "VOLT: " + String(pressureVoltage, 2);
  String textRPM = "RPM: " + String(rpmValue);
  String textPressure = "PRESS: " + String(pressureValue, 2);

  drawTextDiff(textTemperature, prevTemperature, 10, 10, 3, ILI9341_YELLOW);
  drawTextDiff(textVolt, prevVolt, 10, 50, 3, ILI9341_GREEN);
  drawTextDiff(textRPM, prevRPM, 10, 90, 3, ILI9341_CYAN);
  drawTextDiff(textPressure, prevPressure, 10, 130, 3, ILI9341_PURPLE);

  /*long unsigned int rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];

    displayData(String((int)getStableRandomValue()), "RPM");

    // CAN processing
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&rxId, &len, rxBuf);

    if (rxId == 0x280 && selectedScreen == 0) {
      unsigned int rpm_raw = (rxBuf[3] << 8 | rxBuf[2]);
      float rpm = rpm_raw / 4.0;
      if (rpm != previousRpm) {
        Serial.print("Engine RPM: ");
        Serial.println(rpm);
        displayData(String((int)rpm), "RPM");
        previousRpm = rpm;
        lastCanMessageTime = millis();
      }
    }

    if (rxId == 0x5A0 && selectedScreen == 1) {
      uint16_t rawBoost = (rxBuf[2] << 8) | rxBuf[3];
      float boost_hpa = rawBoost / 10.0;
      float boost_relative = boost_hpa - 1000.0;
      Serial.print("Boost: ");
      Serial.print(boost_relative);
      Serial.println(" hPa");
      displayData(String((int)boost_relative), "BAR");
      lastCanMessageTime = millis();
    }
    }

    // Deep sleep check
    if (millis() - lastCanMessageTime > inactivityTimeout) {
    Serial.println("No CAN data for 10s. Going to sleep...");
    delay(100);
    lcd.clear();
    lcd.noBacklight();
    esp_deep_sleep_start();
    }*/
}

// # TESTING #
int currentValue = 2000;  // Initial value

int getStableRandomValue() {
  int delta = random(-10, 11);  // Generates number between -10 and 10
  currentValue += delta;

  // Constrain the value to stay between 1000 and 3000
  currentValue = constrain(currentValue, 1000, 3000);

  return currentValue;
}
// # TESTING #
