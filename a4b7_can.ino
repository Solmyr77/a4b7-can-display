#include <mcp_can.h>
#include <SPI.h>

const int SPI_CS_PIN = 5;  // CS pin for MCP2515
MCP_CAN CAN(SPI_CS_PIN);   // Set CS pin

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

#define MCP2515_INT_PIN 4  // Connect INT pin of MCP2515 to GPIO 33 (wakeup-capable)
unsigned long lastCanMessageTime = 0;
const unsigned long inactivityTimeout = 10000;  // 10 seconds

int selectedScreen = 0;
const unsigned long updateInterval = 300;

// the 8 arrays that form each segment of the custom numbers
byte LT[8] = {
  B00111,
  B01111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};
byte UB[8] = {
  B11111,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
byte RT[8] = {
  B11100,
  B11110,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};
byte LL[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B01111,
  B00111
};
byte LB[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B11111
};
byte LR[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11110,
  B11100
};
byte MB[8] = {
  B11111,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111
};
byte block[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

void display0(int col) {
  lcd.setCursor(col, 0);
  lcd.write(0);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(col, 1);
  lcd.write(3);
  lcd.write(4);
  lcd.write(5);
}

void display1(int col) {
  lcd.setCursor(col, 0);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(col, 1);
  lcd.write(4);
  lcd.write(7);
  lcd.write(4);
}

void display2(int col) {
  lcd.setCursor(col, 0);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(col, 1);
  lcd.write(3);
  lcd.write(4);
  lcd.write(4);
}

void display3(int col) {
  lcd.setCursor(col, 0);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(col, 1);
  lcd.write(4);
  lcd.write(4);
  lcd.write(5);
}

void display4(int col) {
  lcd.setCursor(col, 0);
  lcd.write(3);
  lcd.write(4);
  lcd.write(7);
  lcd.setCursor(col + 2, 1);
  lcd.write(7);
}

void display5(int col) {
  lcd.setCursor(col, 0);
  lcd.write(3);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(col, 1);
  lcd.write(4);
  lcd.write(4);
  lcd.write(5);
}

void display6(int col) {
  lcd.setCursor(col, 0);
  lcd.write(0);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(col, 1);
  lcd.write(3);
  lcd.write(4);
  lcd.write(5);
}

void display7(int col) {
  lcd.setCursor(col, 0);
  lcd.write(1);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(col + 1, 1);
  lcd.write(7);
}

void display8(int col) {
  lcd.setCursor(col, 0);
  lcd.write(0);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(col, 1);
  lcd.write(3);
  lcd.write(4);
  lcd.write(5);
}

void display9(int col) {
  lcd.setCursor(col, 0);
  lcd.write(0);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(col + 2, 1);
  lcd.write(7);
}

void clearnumber() {
  lcd.setCursor(0, 0);
  lcd.print("   ");
  lcd.setCursor(0, 1);
  lcd.print("   ");
}

void displayData(String num, String text) {
  static unsigned long lastUpdateTime = 0;
  const unsigned long updateInterval = 300;

  if (millis() - lastUpdateTime < updateInterval) {
    return;
  }

  lastUpdateTime = millis();

  lcd.clear();

  int col = 0;

  for (int i = 0; i < num.length(); i++) {
    char c = num[i];
    int digit = c - '0';

    switch (digit) {
      case 0: display0(col); break;
      case 1: display1(col); break;
      case 2: display2(col); break;
      case 3: display3(col); break;
      case 4: display4(col); break;
      case 5: display5(col); break;
      case 6: display6(col); break;
      case 7: display7(col); break;
      case 8: display8(col); break;
      case 9: display9(col); break;
    }

    col += 3;
  }

  lcd.setCursor(13, 0);
  lcd.print(text);
  lcd.setCursor(0, 0);
}


void setup() {
  Serial.begin(115200);

  pinMode(MCP2515_INT_PIN, INPUT_PULLUP);  // INT is active LOW

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from CAN interrupt.");
  }

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, LT);
  lcd.createChar(1, UB);
  lcd.createChar(2, RT);
  lcd.createChar(3, LL);
  lcd.createChar(4, LB);
  lcd.createChar(5, LR);
  lcd.createChar(6, MB);
  lcd.createChar(7, block);

  lcd.begin(16, 2);

  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" CAN init error");
    lcd.setCursor(0, 1);
    lcd.print("   retrying..   ");
    delay(1000);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    CAN init    ");
  lcd.setCursor(0, 1);
  lcd.print("    complete    ");

  delay(3000);
  lcd.clear();

  CAN.setMode(MCP_LISTENONLY);

  lcd.print("   Standby...   ");

  // Set wakeup source
  esp_sleep_enable_ext0_wakeup((gpio_num_t)MCP2515_INT_PIN, 0);  // wake on LOW

  lastCanMessageTime = millis();
}

float previousRpm = -1.0;
unsigned long lastUpdateTime = 0;

void loop() {
  long unsigned int rxId;
  unsigned char len = 0;
  unsigned char rxBuf[8];

  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&rxId, &len, rxBuf);

    if (rxId == 0x280 && selectedScreen == 0) {
      unsigned int rpm_raw = (rxBuf[3] << 8 | rxBuf[2]);
      float rpm = rpm_raw / 4.0;

      Serial.print("Engine RPM: ");
      Serial.println(rpm);

      displayData(String((int)rpm), "RPM");

      lastCanMessageTime = millis();  // reset inactivity timer
    }

    if (rxId == 0x5A0 && selectedScreen == 1) {
      uint16_t rawBoost = (rxBuf[2] << 8) | rxBuf[3];
      float boost_hpa = rawBoost / 10.0;  // if it's 0.1 hPa resolution
      float boost_relative = boost_hpa - 1000.0;

      Serial.print("Boost: ");
      Serial.print(boost_relative);
      Serial.println(" hPa");

      displayData(String((float)boost_relative), "BAR");
    }
  }

  // Check for inactivity timeout
  if (millis() - lastCanMessageTime > inactivityTimeout) {
    Serial.println("No CAN data for 10s. Going to sleep...");

    delay(100);  // let serial flush
    lcd.clear();
    lcd.noBacklight();  // optional

    esp_deep_sleep_start();  // never returns
  }
}
