#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Servo.h>

// ==== Réseau ====
const char* ssid = "ESP-ACC";
const char* password = ""; // Pas de mot de passe pour simplicité (AP ouvert)

WebSocketsServer webSocket = WebSocketsServer(81);

// ==== MPU6050 ====
const uint8_t MPU6050SlaveAddress = 0x68;
int16_t AccelX, AccelY, AccelZ;
unsigned long OffsetT = 0;
volatile bool START = false;
const unsigned long SAMPLE_INTERVAL = 2; // 10 ms interval (100 Hz)
unsigned long lastSampleTime = 0;
bool sendX = true, sendY = true, sendZ = true;

// ==== Servo ====
Servo myServo;
const int SERVO_PIN = D5;
int trimOffset = 0;

// ==== Déclencheur physique ====
//#define PIN_START  D2   // Décommente si tu as un bouton/barrière sur une broche

void setup() {
  //Serial.begin(115200);

  // WiFi AP mode (modifie si tu veux station)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);

  // WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // MPU6050 I2C init
  Wire.begin();
  MPU6050_Init();

  // Déclencheur physique
  //pinMode(PIN_START, INPUT_PULLUP);

  //Serial.println("Pret sur 192.168.4.1:81");
}

void loop() {
  webSocket.loop();

    if (START) {
    unsigned long now = millis();
    if (now - lastSampleTime >= SAMPLE_INTERVAL) {
      lastSampleTime = now;
      unsigned long cycle = now - OffsetT;
      Read_RawValue(MPU6050SlaveAddress);

        char msg[64];
      int len = snprintf(msg, sizeof(msg), "%lu;", cycle);
      if(sendX) len += snprintf(msg+len, sizeof(msg)-len, "%d;", AccelX);
      if(sendY) len += snprintf(msg+len, sizeof(msg)-len, "%d;", AccelY);
      if(sendZ) len += snprintf(msg+len, sizeof(msg)-len, "%d;", AccelZ);
      len += snprintf(msg+len, sizeof(msg)-len, "\n");
      webSocket.sendTXT(0, msg, len);
    }
  }

  yield();
}


// === Gestion WebSocket ===
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if(type == WStype_TEXT){
    String msg = String((char*)payload);
    msg.trim();
    //Serial.print("Recu WS: "); Serial.println(msg);
    if(msg.startsWith("start")){
      sendX = sendY = sendZ = false;
      if(msg.startsWith("start:")){
        String axes = msg.substring(6);
        if(axes.indexOf('X') >= 0) sendX = true;
        if(axes.indexOf('Y') >= 0) sendY = true;
        if(axes.indexOf('Z') >= 0) sendZ = true;
      } else {
        sendX = sendY = sendZ = true;
      }
      START = true;
      OffsetT = millis();
      //Serial.println("DEBUT MESURE");
    }
    else if(msg == "stop"){
      START = false;
      //Serial.println("ARRET MESURE");
    }
    else if(msg.startsWith("servo:")){
      int angle = msg.substring(6).toInt();
      myServo.write(angle + trimOffset);
    }
    else if(msg.startsWith("trim:")){
      trimOffset = msg.substring(5).toInt();
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
  AccelX = (Wire.read() << 8) | Wire.read();
  AccelY = (Wire.read() << 8) | Wire.read();
  AccelZ = (Wire.read() << 8) | Wire.read();
}
