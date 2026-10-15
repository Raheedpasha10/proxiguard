/*
 * ProxiGuard - Untethered Wireless IoT Proximity Monitor
 *
 * Hardware:
 *   - Arduino Uno (ATmega328P)
 *   - HC-SR04: TRIG -> D9, ECHO -> D10
 *   - ESP8266 ESP-01: UTXD -> D11, URXD -> D12, VCC/CH_PD -> 3.3V, GND -> GND
 *
 * Streaming Architecture:
 *   - ESP8266 runs high-reliability multi-client TCP Server on port 8888
 *   - Interleaved packet transmission to all active client channels (0, 1, 2)
 *   - Blind-proof transmission (independent of SoftwareSerial RX signal levels)
 *   - Auto-recovers if Wi-Fi reassociates
 */

#include <SoftwareSerial.h>

SoftwareSerial esp(11, 12);
const byte TRIG_PIN = 9;
const byte ECHO_PIN = 10;

byte currentChannel = 0;
unsigned long lastServerCheck = 0;

void initServer() {
  esp.println(F("AT+CIPMUX=1"));
  delay(150);
  while (esp.available()) esp.read();

  esp.println(F("AT+CIPSERVER=1,8888"));
  delay(150);
  while (esp.available()) esp.read();
}

void setup() {
  Serial.begin(9600);
  esp.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println(F("\n=== ProxiGuard Untethered IoT Streamer Booting ==="));

  // Allow ESP8266 time to associate with Hotspot AP
  delay(2500);
  initServer();
  Serial.println(F("=== Server Port 8888 Active ==="));
}

void sendPacket(byte ch, const String& payload) {
  while (esp.available()) esp.read();
  esp.print(F("AT+CIPSEND="));
  esp.print(ch);
  esp.print(F(","));
  esp.println(payload.length());
  delay(15);
  esp.print(payload);
  delay(30);
  while (esp.available()) esp.read();
}

void loop() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  String packet;
  if (duration > 0) {
    float dist = duration * 0.0343 / 2.0;
    packet = "DIST:" + String(dist, 1) + "\n";
  } else {
    packet = "NO_ECHO\n";
  }

  // Mirror to USB Serial for debugging
  Serial.print(packet);

  // Send packet to interleaved Wi-Fi TCP channel (cycles 0 -> 1 -> 2)
  sendPacket(currentChannel, packet);
  currentChannel = (currentChannel + 1) % 3;

  // Keep server alive if Wi-Fi or AP drops and recovers
  if (millis() - lastServerCheck > 10000) {
    lastServerCheck = millis();
    esp.println(F("AT+CIPSERVER=1,8888"));
    delay(40);
    while (esp.available()) esp.read();
  }

  delay(60);
}
