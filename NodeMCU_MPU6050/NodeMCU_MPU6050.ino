#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Wire.h>

// ==== Réseau ====
const char* ssid = "ESP-ACC";
const char* password = ""; // Pas de mot de passe pour simplicité (AP ouvert)

WebSocketsServer webSocket = WebSocketsServer(81);

// ==== MPU6050 ====
const uint8_t MPU6050SlaveAddress = 0x68;
float AccelX, AccelY, AccelZ;
unsigned long OffsetT = 0;
volatile bool START = false;

// ==== Déclencheur physique ====
//#define PIN_START  D2   // Décommente si tu as un bouton/barrière sur une broche

void setup() {
  Serial.begin(115200);

  // WiFi AP mode (modifie si tu veux station)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);

  // WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // MPU6050 I2C init
  Wire.begin();
  MPU6050_Init();

  // Déclencheur physique
  //pinMode(PIN_START, INPUT_PULLUP);

  Serial.println("Pret sur 192.168.4.1:81");
}

void loop() {
  webSocket.loop();

  if (START) {
    unsigned long cycle = millis() - OffsetT;
    Read_RawValue(MPU6050SlaveAddress);

    char msg[48];
    //int len = snprintf(msg, sizeof(msg), "%lu;%.3f;%.3f;%.3f;\n", cycle, AccelX, AccelY, AccelZ);
    int len = snprintf(msg, sizeof(msg), "%lu;%.3f;\n", cycle, AccelX);
    webSocket.sendTXT(0, msg, len);

    delay(100); // 100Hz, à ajuster
  }

  yield();
}


// === Gestion WebSocket ===
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if(type == WStype_TEXT){
    String msg = String((char*)payload);
    msg.trim();
    Serial.print("Recu WS: "); Serial.println(msg);
    if(msg == "start"){
      START = true;
      OffsetT = millis();
      Serial.println("DEBUT MESURE");
    }
    else if(msg == "stop"){
      START = false;
      Serial.println("ARRET MESURE");
    }
  }
}


// === MPU6050 Low-level ===
void MPU6050_Init(){
  I2C_Write(MPU6050SlaveAddress, 0x6B, 0x00);  // Wake up
  I2C_Write(MPU6050SlaveAddress, 0x1B, 0x00);  // +/-250dps
  I2C_Write(MPU6050SlaveAddress, 0x1C, 0x00);  // +/-2g
}
void I2C_Write(uint8_t dev, uint8_t reg, uint8_t data){
  Wire.beginTransmission(dev); Wire.write(reg); Wire.write(data); Wire.endTransmission();
}
void Read_RawValue(uint8_t dev){
  Wire.beginTransmission(dev); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(dev, (size_t)6, true);
  int16_t x = (Wire.read()<<8)|Wire.read();
  int16_t y = (Wire.read()<<8)|Wire.read();
  int16_t z = (Wire.read()<<8)|Wire.read();
  AccelX = (float)x/16384.0; // Conversion G
  AccelY = (float)y/16384.0;
  AccelZ = (float)z/16384.0;
}
