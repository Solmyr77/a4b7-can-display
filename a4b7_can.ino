#include <Arduino.h>
#include <mcp_can.h>
#include <SPI.h>
#include <Wire.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "FS.h"
#include "SPIFFS.h"

// TFT (HSPI)
#define TFT_DC 2
#define TFT_CS 15
#define TFT_MOSI 13
#define TFT_CLK 14
#define TFT_RST 27
#define TFT_MISO 12
#define TFT_POWER_CTRL 26
// CAN (VSPI)
#define MCP_CS_PIN 5
#define MCP_INT_PIN 4

SPIClass tftHSPI(HSPI);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&tftHSPI, TFT_DC, TFT_CS, TFT_RST);
MCP_CAN CAN(MCP_CS_PIN);

unsigned long lastCanMessageTime = 0;
const unsigned long inactivityTimeout = 10000'0000;
int selectedScreen = 0;

String prevTemperature = "";
String prevVolt = "";
String prevRPM = "";
String prevPressure = "";
String prevCoolant = "";
String prevIAT = "";
String prevBoost = "";
String prevSpeed = "";

float temperatureVoltages[] = { 2.979, 1.586, 0.79, 0.354, 0.163, 0.102 };
float temperatureValues[] = { 21, 40, 70, 100, 130, 150 };
float pressureVoltages[] = { 0.065, 0.61, 1.194, 1.551, 1.703 };
float pressureValues[] = { 0, 2, 5, 8, 10 };

bool drawRawImage(const char *filename, int16_t x, int16_t y, int w, int h) {
  File f = SPIFFS.open(filename, "r");
  if (!f) {
    Serial.print("Failed to open file: ");
    Serial.println(filename);
    return false;
  }
  uint16_t *lineBuf = (uint16_t *)malloc(w * 2);
  if (!lineBuf) {
    Serial.println("Out of memory!");
    f.close();
    return false;
  }
  for (int row = 0; row < h; row++) {
    f.read((uint8_t *)lineBuf, w * 2);
    tft.drawRGBBitmap(x, y + row, lineBuf, w, 1);
  }
  free(lineBuf);
  f.close();
  return true;
}

void initDisplay() {
  digitalWrite(TFT_POWER_CTRL, HIGH);
  delay(100);

  SPIFFS.begin(true);
  tftHSPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  delay(1000);
  tft.begin();
  tft.writeCommand(ILI9341_SLPOUT);
  tft.writeCommand(ILI9341_DISPON);
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
}

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
  drawRawImage("/startup.raw", 0, 0, 320, 240);
  delay(3000);
  tft.fillScreen(ILI9341_BLACK);
}

void setup() {
  Serial.begin(115200);

  esp_reset_reason_t r = esp_reset_reason();
  Serial.printf("Last reset reason: %d\n", r);

  pinMode(MCP_INT_PIN, INPUT_PULLUP);
  pinMode(TFT_POWER_CTRL, OUTPUT);

  initDisplay();
  initCan();

  /*if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from CAN interrupt.");
  }*/

  lastCanMessageTime = millis();
}

float interpolate(float *voltage, float *value, int count, float inputVoltage) {
  for (int i = 0; i < count - 1; i++) {
    float v1 = voltage[i];
    float v2 = voltage[i + 1];
    float val1 = value[i];
    float val2 = value[i + 1];
    if ((v1 >= inputVoltage && inputVoltage >= v2) || (v2 >= inputVoltage && inputVoltage >= v1)) {
      return val1 + (inputVoltage - v1) * (val2 - val1) / (v2 - v1);
    }
  }
  if (inputVoltage > voltage[0]) return value[0];
  if (inputVoltage < voltage[count - 1]) return value[count - 1];
  return -1;
}

void drawTextDiff(String newText, String &prevText, int x, int y, uint8_t textSize, uint16_t color) {
  int charW = 6 * textSize;
  tft.setTextSize(textSize);
  tft.setTextColor(color, ILI9341_BLACK);
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
  float pressureVoltage = float(analogReadMilliVolts(34)) / 1000;

  float temperatureValue = interpolate(temperatureVoltages, temperatureValues, 6, temperatureVoltage);
  float pressureValue = interpolate(pressureVoltages, pressureValues, 5, pressureVoltage);

  String textTemperature = "OIL: " + String(temperatureValue, 0);
  // String textVolt = "VOLT: " + String(pressureVoltage, 2);
  String textPressure = "PRESS: " + String(pressureValue, 2);

  drawTextDiff(textTemperature, prevTemperature, 10, 10, 3, ILI9341_YELLOW);
  // drawTextDiff(textVolt, prevVolt, 10, 50, 3, ILI9341_GREEN);
  drawTextDiff(textPressure, prevPressure, 10, 50, 3, ILI9341_GREEN);

  long unsigned int rxId;
  unsigned char len = 0;
  unsigned char rxBuf[8];

  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&rxId, &len, rxBuf);

    // Read RPM
    if (rxId == 0x280 && selectedScreen == 0) {
      unsigned int rpm_raw = (rxBuf[3] << 8 | rxBuf[2]);
      float rpm = rpm_raw / 4.0;

      String newTextRPM = "RPM: " + String(rpm, 0);
      drawTextDiff(newTextRPM, prevRPM, 10, 90, 3, ILI9341_PINK);

      lastCanMessageTime = millis();
    }

    // Read coolant temp
    if (rxId == 0x288 && selectedScreen == 0) {
      unsigned int rawCoolant = rxBuf[1];
      float coolant = rawCoolant * 0.75 - 48;

      String newTextCoolant = "Coolant: " + String(coolant, 0);
      drawTextDiff(newTextCoolant, prevCoolant, 10, 130, 3, ILI9341_OLIVE);

      lastCanMessageTime = millis();
    }

    // Read IAT
    if (rxId == 0x380 && selectedScreen == 0) {
      unsigned int rawIAT = rxBuf[1];
      float IAT = rawIAT * 0.75 - 48;

      String newTextIAT = "IAT: " + String(IAT, 0);
      drawTextDiff(newTextIAT, prevIAT, 10, 170, 3, ILI9341_ORANGE);

      lastCanMessageTime = millis();
    }

    // Read boost
    if (rxId == 0x588 && selectedScreen == 0) {
      unsigned int rawBoost = rxBuf[4];
      float boost = rawBoost * 0.75 - 48;

      String newTextBoost = "Boost: " + String(boost, 0);
      drawTextDiff(newTextBoost, prevBoost, 10, 210, 3, ILI9341_MAGENTA);

      lastCanMessageTime = millis();
    }

    /*
    // Read speed
    if (rxId == 0x320 && selectedScreen == 0) {
      unsigned int rawSpeed = (rxBuf[4] << 8 | rxBuf[3]);
      float speed = rawSpeed / 4.0;

      String newTextSpeed = "Speed: " + String(speed, 0);
      drawTextDiff(newTextSpeed, prevSpeed, 10, 90, 3, ILI9341_CYAN);

      lastCanMessageTime = millis();
    }*/
  }

  /*
  // Deep sleep logic
  if (millis() - lastCanMessageTime > inactivityTimeout) {
    Serial.println("No CAN data for 10s. Going to sleep...");

    tft.fillScreen(ILI9341_BLACK);
    tft.writeCommand(ILI9341_DISPOFF);
    tft.writeCommand(ILI9341_SLPIN);

    pinMode(TFT_POWER_CTRL, INPUT_PULLDOWN);
    delay(100);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)MCP_INT_PIN, 0);

    esp_deep_sleep_start();
  }*/
}
