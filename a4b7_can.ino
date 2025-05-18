#include <SPI.h>
#include <mcp2515.h>

MCP2515 mcp2515(5);  // CS pin (GPIO 5)
struct can_frame canMsg;

void setup() {
  Serial.begin(115200);
  delay(500);

  // Start SPI
  SPI.begin();  // Uses default VSPI: GPIO18=SCK, GPIO19=MISO, GPIO23=MOSI

  // Init MCP2515
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);  // Match your bus speed and crystal
  mcp2515.setNormalMode();

  Serial.println("MCP2515 Ready - Listening for CAN messages...");
}

void loop() {
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    Serial.print("ID: 0x");
    Serial.print(canMsg.can_id, HEX);
    Serial.print("  DLC: ");
    Serial.print(canMsg.can_dlc);
    Serial.print("  Data: ");
    for (uint8_t i = 0; i < canMsg.can_dlc; i++) {
      Serial.print(canMsg.data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}
