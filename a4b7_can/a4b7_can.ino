#include <Arduino.h>
#include <mcp_can.h>
#include <SPI.h>
#include <Wire.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "FS.h"
#include "SPIFFS.h"

#define DEBUG_SERIAL Serial

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

constexpr unsigned long INACTIVITY_TIMEOUT = 10000'0000;
unsigned long lastCanMessageTime = 0;
uint8_t selectedScreen = 0;

String prevTemperature, prevRPM, prevPressure, prevCoolant, prevIAT, prevBoost, prevSpeed;

int RPM = 0;

portMUX_TYPE spiMutex = portMUX_INITIALIZER_UNLOCKED;

float temperatureVoltages[] = { 2.979, 1.586, 0.79, 0.354, 0.163, 0.102 };
float temperatureValues[] = { 21, 40, 70, 100, 130, 150 };
float pressureVoltages[] = { 0.065, 0.61, 1.194, 1.551, 1.703 };
float pressureValues[] = { 0, 2, 5, 8, 10 };

byte canBuf[8] = { 0 };

bool drawRawImage(const char *filename, int x, int y, int w, int h) {
  DEBUG_SERIAL.printf("Opening file: %s\n", filename);
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    DEBUG_SERIAL.println("File open failed");
    return false;
  }

  uint16_t *lineBuf = (uint16_t *)malloc(w * 2);
  if (!lineBuf) {
    DEBUG_SERIAL.println("malloc failed");
    file.close();
    return false;
  }

  for (int row = 0; row < h; row++) {
    if (file.read((uint8_t *)lineBuf, w * 2) != w * 2) {
      DEBUG_SERIAL.printf("File read failed at row: %d\n", row);
      break;
    }
    portENTER_CRITICAL(&spiMutex);
    tft.drawRGBBitmap(x, y + row, lineBuf, w, 1);
    portEXIT_CRITICAL(&spiMutex);
  }

  free(lineBuf);
  file.close();
  return true;
}

void initDisplay() {
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
  drawRawImage("/startup.raw", 0, 0, 320, 240);
  delay(3000);
  tft.fillScreen(ILI9341_BLACK);
}

void createFilters() {
  // Set mask to match all 11 bits (standard ID)
  CAN.init_Mask(0, false, 0x7FF);  // filters 0 and 1
  CAN.init_Mask(1, false, 0x7FF);  // filters 2–5

  // Set each filter to a specific ID
  CAN.init_Filt(0, false, 0x280);
  CAN.init_Filt(1, false, 0x288);
  CAN.init_Filt(2, false, 0x380);
  CAN.init_Filt(3, false, 0x588);
  CAN.init_Filt(4, false, 0x320);
}

void createGraphics() {
  portENTER_CRITICAL(&spiMutex);

  tft.setTextSize(2);
  tft.setTextWrap(false);
  tft.setTextColor(ILI9341_BLACK);

  // First data row START
  tft.fillRect(0, 60, 320, 20, ILI9341_CYAN);

  tft.setCursor(4, 63);
  tft.print("SPD");
  tft.fillRect(106, 60, 2, 20, ILI9341_BLACK);

  tft.setCursor(112, 63);
  tft.print("MAP");
  tft.fillRect(212, 60, 2, 20, ILI9341_BLACK);

  tft.setCursor(219, 63);
  tft.print("PRS");

  // First data row END

  // Second data row
  tft.fillRect(0, 150, 320, 20, ILI9341_CYAN);

  tft.setCursor(4, 153);
  tft.print("IAT");
  tft.fillRect(106, 150, 2, 20, ILI9341_BLACK);

  tft.setCursor(112, 153);
  tft.print("CLT");
  tft.fillRect(212, 150, 2, 20, ILI9341_BLACK);

  tft.setCursor(216, 153);
  tft.print("OIL");

  // ROW: 0, COL: 0
  drawTextDiff(String(0), prevSpeed, 10, 102, 4, ILI9341_GREEN);

  // ROW: 0, COL: 1
  drawTextDiff(String(0.00), prevBoost, 112, 102, 4, ILI9341_GREEN);

  // ROW: 1, COL: 0
  drawTextDiff(String(0), prevIAT, 10, 188, 5, ILI9341_GREEN);

  // ROW: 1, COL: 1
  drawTextDiff(String(0), prevCoolant, 112, 188, 5, ILI9341_GREEN);

  portEXIT_CRITICAL(&spiMutex);
}

void setup() {
  pinMode(MCP_INT_PIN, INPUT_PULLUP);
  pinMode(TFT_POWER_CTRL, OUTPUT);

  digitalWrite(TFT_POWER_CTRL, HIGH);

  delay(250);

  DEBUG_SERIAL.begin(115200);

  delay(250);

  initDisplay();
  initCan();

  createGraphics();

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

void drawTextDiff(const String &newText, String &prevText, int x, int y, uint8_t size, uint16_t color) {
  if (newText == prevText) return;
  int charWidth = 6 * size;
  portENTER_CRITICAL(&spiMutex);
  tft.setTextSize(size);
  for (int i = 0; i < newText.length(); i++) {
    char newChar = newText[i];
    char oldChar = (i < prevText.length()) ? prevText[i] : 0;
    if (newChar != oldChar) {
      tft.setCursor(x + i * charWidth, y);
      tft.setTextColor(color, ILI9341_BLACK);
      tft.print(newChar);
    }
  }
  portEXIT_CRITICAL(&spiMutex);
  prevText = newText;
}

void loop() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  float pressureVoltage = float(analogReadMilliVolts(34)) / 1000;
  float temperatureVoltage = float(analogReadMilliVolts(33)) / 1000;

  float oilPress = interpolate(pressureVoltages, pressureValues, 5, pressureVoltage);
  float oilTemp = interpolate(temperatureVoltages, temperatureValues, 6, temperatureVoltage);

  // ROW: 0, COL: 2
  drawTextDiff(String(oilPress, 1), prevPressure, 226, 98, 5, ILI9341_GREEN);

  // ROW: 1, COL: 2
  drawTextDiff(String(oilTemp, 0), prevTemperature, 226, 188, 5, ILI9341_GREEN);

  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    unsigned long rxId = 0;
    byte len = 0;

    memset(canBuf, 0, sizeof(canBuf));

    if (CAN.readMsgBuf(&rxId, &len, canBuf) == CAN_OK && len > 0 && len <= 8) {

      // Logging
      DEBUG_SERIAL.print("CAN ID: 0x");
      DEBUG_SERIAL.print(rxId, HEX);
      DEBUG_SERIAL.print(" DLC: ");
      DEBUG_SERIAL.print(len);
      DEBUG_SERIAL.print(" Data: ");
      for (int i = 0; i < len; i++) {
        if (canBuf[i] < 0x10) DEBUG_SERIAL.print("0");  // Leading zero
        DEBUG_SERIAL.print(canBuf[i], HEX);
        DEBUG_SERIAL.print(" ");
      }
      DEBUG_SERIAL.println();

      switch (rxId) {
        case 0x280:
          if (len >= 4) {
            uint16_t rawRPM = (canBuf[3] << 8) | canBuf[2];
            float rpm = rawRPM / 4.0;
            RPM = rpm;
          } else {
            DEBUG_SERIAL.println("Invalid len < 4 for RPM");
          }
          break;
        case 0x288:
          if (len >= 2) {
            float coolant = canBuf[1] * 0.75 - 48;
            String displayCoolant = String(coolant, 0);
            drawTextDiff(displayCoolant, prevCoolant, 112, 188, 5, ILI9341_GREEN);
          } else {
            DEBUG_SERIAL.println("Invalid len < 2 for Coolant");
          }
          break;
        case 0x380:
          if (len >= 2) {
            float iat = canBuf[1] * 0.75 - 48;
            String displayIat = String(iat, 0);
            drawTextDiff(displayIat, prevIAT, 226, 188, 3, ILI9341_GREEN);
          } else {
            DEBUG_SERIAL.println("Invalid len < 2 for IAT");
          }
          break;
        case 0x588:
          if (len >= 5) {
            float rawBoost = canBuf[4] * 10 - 1000;
            float boost = rawBoost / 1000;
            String displayBoost = String(boost, 2);
            drawTextDiff(displayBoost, prevBoost, 112, 102, 4, ILI9341_GREEN);
          } else {
            DEBUG_SERIAL.println("Invalid len < 5 for Boost");
          }
          break;
        case 0x320:
          if (len >= 5) {
            float speed = (canBuf[4] << 8) | canBuf[3];
            String displaySpeed = String(speed, 0);
            drawTextDiff(displaySpeed, prevSpeed, 10, 102, 4, ILI9341_GREEN);
          } else {
            DEBUG_SERIAL.println("Invalid len < 5 for Speed");
          }
          break;
        default:
          break;
      }

      lastCanMessageTime = millis();
    } else {
      DEBUG_SERIAL.println("CAN readMsgBuf failed or invalid len!");
    }
  }
}