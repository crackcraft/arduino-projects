#include <OneWire.h>

OneWire  ds(10);

void setup(void) {
  Serial.begin(9600);
}

void loop(void) {

  byte addr[8];
  
  if ( !ds.search(addr)) {
      ds.reset_search();
      return;
  }
  
  Serial.print("\nR=");
  for(int i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX);
    Serial.print(" ");
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  
  if ( addr[0] != 0x01) {
      Serial.println("Device is not a DS1990A family device.");
      return;
  }
  Serial.println();
  ds.reset();
  
  delay(1000);
}
