#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <FS.h>
#include <SPIFFS.h>
#include <mcp_can.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define DEBUG_SERIAL Serial
#define LOG_PREFIX "[LOG] "
#define LOG_START() DEBUG_SERIAL.printf(LOG_PREFIX "--> %s START\n", __func__);
#define LOG_END()   DEBUG_SERIAL.printf(LOG_PREFIX "<-- %s END\n", __func__);
#define LOG_LINE()  DEBUG_SERIAL.printf(LOG_PREFIX "%s:%d\n", __FILE__, __LINE__);
#define LOG_MSG(msg) DEBUG_SERIAL.printf(LOG_PREFIX "%s: %s\n", __func__, msg);
#define LOG_VAL(label, val) DEBUG_SERIAL.printf(LOG_PREFIX "%s: %s = %s\n", __func__, label, String(val).c_str());

// TFT Display (HSPI)
#define TFT_DC           2
#define TFT_CS           15
#define TFT_MOSI         13
#define TFT_CLK          14
#define TFT_RST          27
#define TFT_MISO         12
#define TFT_POWER_CTRL   26

// CAN Controller (VSPI)
#define MCP_CS_PIN       5
#define MCP_INT_PIN      4

SPIClass tftHSPI(HSPI);
Adafruit_ILI9341 tft(&tftHSPI, TFT_DC, TFT_CS, TFT_RST);
MCP_CAN CAN(MCP_CS_PIN);

constexpr unsigned long INACTIVITY_TIMEOUT = 10000;
unsigned long lastCanMessageTime = 0;
int selectedScreen = 0;

String prevTemperature, prevPressure, prevRPM, prevCoolant, prevIAT, prevBoost;

portMUX_TYPE spiMutex = portMUX_INITIALIZER_UNLOCKED;

const float temperatureVoltages[] = { 2.979, 1.586, 0.79, 0.354, 0.163, 0.102 };
const float temperatureValues[]   = { 21, 40, 70, 100, 130, 150 };
const float pressureVoltages[]    = { 0.065, 0.61, 1.194, 1.551, 1.703 };
const float pressureValues[]      = { 0, 2, 5, 8, 10 };

byte canBuf[8] = {0};  // Global buffer

float interpolate(const float *voltages, const float *values, size_t count, float inputVoltage) {
  LOG_START();
  DEBUG_SERIAL.printf("Input Voltage: %.3f\n", inputVoltage);
  for (size_t i = 0; i < count - 1; ++i) {
    float v1 = voltages[i], v2 = voltages[i + 1];
    float val1 = values[i], val2 = values[i + 1];
    DEBUG_SERIAL.printf("Checking segment: %.3f-%.3f\n", v1, v2);
    if ((v1 >= inputVoltage && inputVoltage >= v2) || (v2 >= inputVoltage && inputVoltage >= v1)) {
      float result = val1 + (inputVoltage - v1) * (val2 - val1) / (v2 - v1);
      DEBUG_SERIAL.printf("Interpolated Result: %.2f\n", result);
      LOG_END();
      return result;
    }
  }
  DEBUG_SERIAL.println("Voltage out of range!");
  LOG_END();
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

bool drawRawImage(const char *filename, int x, int y, int w, int h) {
  LOG_START();
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
  DEBUG_SERIAL.println("Image draw complete");
  LOG_END();
  return true;
}

void initDisplay() {
  LOG_START();
  pinMode(TFT_POWER_CTRL, OUTPUT);
  digitalWrite(TFT_POWER_CTRL, HIGH);
  delay(200);
  SPIFFS.begin(true);
  tftHSPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  delay(100);
  tft.begin();
  tft.writeCommand(ILI9341_SLPOUT);
  tft.writeCommand(ILI9341_DISPON);
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
  LOG_END();
}

void initCAN() {
  LOG_START();
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    DEBUG_SERIAL.println("CAN init failed, retrying...");
    delay(1000);
  }
  CAN.setMode(MCP_LISTENONLY);
  drawRawImage("/startup.raw", 0, 0, 320, 240);
  delay(3000);
  LOG_END();
}

void setup() {
  LOG_START();
  DEBUG_SERIAL.begin(115200);
  delay(300);
  DEBUG_SERIAL.printf("Reset reason: %d\n", esp_reset_reason());
  initDisplay();
  initCAN();
  lastCanMessageTime = millis();
  LOG_END();
}

void loop() {
  LOG_START();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  float temperatureVoltage = analogReadMilliVolts(33) / 1000.0;
  float pressureVoltage    = analogReadMilliVolts(34) / 1000.0;

  float oilTemp = interpolate(temperatureVoltages, temperatureValues, 6, temperatureVoltage);
  float oilPress = interpolate(pressureVoltages, pressureValues, 5, pressureVoltage);

  drawTextDiff("OIL: " + String(oilTemp, 0), prevTemperature, 10, 10, 3, ILI9341_YELLOW);
  drawTextDiff("PRESS: " + String(oilPress, 2), prevPressure, 10, 50, 3, ILI9341_GREEN);

  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    DEBUG_SERIAL.println("CAN message available");
    unsigned long rxId = 0;
    byte len = 0;

    memset(canBuf, 0, sizeof(canBuf)); // clear buffer before read

    if (CAN.readMsgBuf(&rxId, &len, canBuf) == CAN_OK && len > 0 && len <= 8) {
      DEBUG_SERIAL.printf("CAN ID: 0x%03lX, Len: %u\n", rxId, len);
      for (int i = 0; i < len; i++) DEBUG_SERIAL.printf("buf[%d] = 0x%02X\n", i, canBuf[i]);
      DEBUG_SERIAL.println();

      if (selectedScreen != 0) return;

      switch (rxId) {
        case 0x280:
          DEBUG_SERIAL.println("Handling 0x280 - RPM");
          if (len >= 4) {
            uint16_t rawRPM = (canBuf[3] << 8) | canBuf[2];
            float rpm = rawRPM / 4.0;
            DEBUG_SERIAL.printf("Parsed RPM: %.2f\n", rpm);
            String displayRPM = "RPM: " + String(rpm, 0);
            drawTextDiff(displayRPM, prevRPM, 10, 90, 3, ILI9341_PINK);
          } else {
            DEBUG_SERIAL.println("Invalid len < 4 for RPM");
          }
          break;
        case 0x288:
          DEBUG_SERIAL.println("Handling 0x288 - Coolant");
          if (len >= 2) {
            float coolant = canBuf[1] * 0.75 - 48;
            String displayCoolant = "Coolant: " + String(coolant, 0);
            drawTextDiff(displayCoolant, prevCoolant, 10, 130, 3, ILI9341_OLIVE);
          } else {
            DEBUG_SERIAL.println("Invalid len < 2 for Coolant");
          }
          break;
        case 0x380:
          DEBUG_SERIAL.println("Handling 0x380 - IAT");
          if (len >= 2) {
            float iat = canBuf[1] * 0.75 - 48;
            String displayIAT = "IAT: " + String(iat, 0);
            drawTextDiff(displayIAT, prevIAT, 10, 170, 3, ILI9341_ORANGE);
          } else {
            DEBUG_SERIAL.println("Invalid len < 2 for IAT");
          }
          break;
        case 0x588:
          DEBUG_SERIAL.println("Handling 0x588 - Boost");
          if (len >= 5) {
            float boost = canBuf[4] * 0.75 - 48;
            String displayBoost = "Boost: " + String(boost, 0);
            drawTextDiff(displayBoost, prevBoost, 10, 210, 3, ILI9341_MAGENTA);
          } else {
            DEBUG_SERIAL.println("Invalid len < 5 for Boost");
          }
          break;
        default:
          DEBUG_SERIAL.printf("Unknown CAN ID: 0x%03lX\n", rxId);
          break;
      }

      lastCanMessageTime = millis();
    } else {
      DEBUG_SERIAL.println("CAN readMsgBuf failed or invalid len!");
    }
  }

  LOG_END();
}
