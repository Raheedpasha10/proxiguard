/*
 * ProxiGuard - Autonomous IoT Multi-Sensor Spatial Sentinel
 *
 * Microcontroller: Arduino Uno R3 SMD (ATmega328P @ 16 MHz)
 * Telemetry Bridge: ESP8266 ESP-01 on SoftwareSerial (11, 12)
 *
 * Hardware Map:
 *   - HC-SR04 Ultrasonic : TRIG -> D9, ECHO -> D10 (5V Rail)
 *   - HW-201 Active IR   : OUT  -> D2 (5V Aux Rail)
 *   - SW-420 Shock Guard : DO   -> D3 (5V Aux Rail)
 *   - Green Sentinel LED : ANODE-> D8 (through 220R to GND)
 *   - ESP8266 ESP-01     : UTXD -> D11, URXD -> D12 (3.3V Breadboard Hub)
 *
 * Features:
 *   - High-rate non-blocking multi-sensor telemetry stream (DIST, IR, VIB, LED)
 *   - Bidirectional hardware command parser (+IPD command dispatcher)
 *   - Software-defined module power-gating and sleep modes (HCSR04, LED, IR, VIB)
 *   - Interleaved multi-client TCP socket server on port 8888
 */

#include <SoftwareSerial.h>

SoftwareSerial esp(11, 12);

// Hardware Pin Assignments
const byte TRIG_PIN = 9;
const byte ECHO_PIN = 10;
const byte IR_PIN   = 2;
const byte VIB_PIN  = 3;
const byte LED_PIN  = 8;

// Software-Defined Module States
bool hcsr04_enabled = true;
bool ir_enabled     = true;
bool vib_enabled    = true;
bool led_state      = false;

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

void parseCommand(const String& cmd) {
  int idx = cmd.indexOf(F("CMD:"));
  if (idx < 0) return;
  String payload = cmd.substring(idx + 4);
  payload.trim();

  // CMD:PIN:<id>:<val>
  if (payload.startsWith(F("PIN:"))) {
    int s1 = payload.indexOf(':');
    int s2 = payload.indexOf(':', s1 + 1);
    if (s1 > 0 && s2 > s1) {
      int pin = payload.substring(s1 + 1, s2).toInt();
      int val = payload.substring(s2 + 1).toInt();
      pinMode(pin, OUTPUT);
      digitalWrite(pin, val ? HIGH : LOW);
      if (pin == LED_PIN) led_state = (val != 0);
    }
  }
  // CMD:PWM:<id>:<val>
  else if (payload.startsWith(F("PWM:"))) {
    int s1 = payload.indexOf(':');
    int s2 = payload.indexOf(':', s1 + 1);
    if (s1 > 0 && s2 > s1) {
      int pin = payload.substring(s1 + 1, s2).toInt();
      int val = payload.substring(s2 + 1).toInt();
      pinMode(pin, OUTPUT);
      analogWrite(pin, constrain(val, 0, 255));
    }
  }
  // CMD:MOD:<name>:<state>
  else if (payload.startsWith(F("MOD:"))) {
    int s1 = payload.indexOf(':');
    int s2 = payload.indexOf(':', s1 + 1);
    if (s1 > 0 && s2 > s1) {
      String mod = payload.substring(s1 + 1, s2);
      int state = payload.substring(s2 + 1).toInt();
      mod.toUpperCase();
      if (mod == F("HCSR04")) {
        hcsr04_enabled = (state != 0);
        if (!hcsr04_enabled) {
          digitalWrite(TRIG_PIN, LOW);
          pinMode(ECHO_PIN, INPUT);
        }
      } else if (mod == F("LED")) {
        led_state = (state != 0);
        digitalWrite(LED_PIN, led_state ? HIGH : LOW);
      } else if (mod == F("IR")) {
        ir_enabled = (state != 0);
      } else if (mod == F("VIB")) {
        vib_enabled = (state != 0);
      }
    }
  }
}

void checkIncomingCommands() {
  // Check Hardware Serial (USB)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    parseCommand(cmd);
  }

  // Check SoftwareSerial (ESP8266 Wi-Fi)
  while (esp.available()) {
    String line = esp.readStringUntil('\n');
    if (line.indexOf(F("+IPD")) >= 0 || line.indexOf(F("CMD:")) >= 0) {
      parseCommand(line);
    }
  }
}

void setup() {
  Serial.begin(9600);
  esp.begin(9600);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Optical & Seismic Sensors with Internal Pullup
  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(VIB_PIN, INPUT_PULLUP);

  // Green Sentinel LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println(F("\n=== ProxiGuard Sentinel Booting ==="));
  Serial.println(F("Sensors: HC-SR04 (D9/D10) | IR (D2) | VIB (D3) | LED (D8)"));

  delay(2500);
  initServer();
  Serial.println(F("=== Server Port 8888 Ready ==="));
}

void sendPacket(byte ch, const String& payload) {
  esp.print(F("AT+CIPSEND="));
  esp.print(ch);
  esp.print(F(","));
  esp.println(payload.length());
  delay(15);
  esp.print(payload);
  delay(25);
  checkIncomingCommands();
}

void loop() {
  checkIncomingCommands();

  // 1. Read HC-SR04 Distance (or report OFF if in sleep standby)
  String distStr = "OFF";
  if (hcsr04_enabled) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 26000); // ~4.4m timeout
    if (duration > 0) {
      float dist = duration * 0.0343 / 2.0;
      distStr = String(dist, 1);
    } else {
      distStr = "NO_ECHO";
    }
  }

  // 2. Read HW-201 IR Sensor (Active LOW: 0 = object detected / beam broken, 1 = clear)
  int irVal = ir_enabled ? digitalRead(IR_PIN) : 1;

  // 3. Read SW-420 Vibration Sensor (Active HIGH pulse on shock: 1 = shock, 0 = still)
  int vibVal = vib_enabled ? digitalRead(VIB_PIN) : 0;

  // 4. Construct Multi-Sensor Telemetry Packet
  String packet = "DIST:" + distStr +
                 ",IR:" + String(irVal) +
                 ",VIB:" + String(vibVal) +
                 ",LED:" + String(led_state ? 1 : 0) + "\n";

  // Mirror to USB Serial
  Serial.print(packet);

  // Transmit over active Wi-Fi TCP channel (cycles 0 -> 1 -> 2)
  sendPacket(currentChannel, packet);
  currentChannel = (currentChannel + 1) % 3;

  // Auto-healing TCP server watchdog
  if (millis() - lastServerCheck > 10000) {
    lastServerCheck = millis();
    esp.println(F("AT+CIPSERVER=1,8888"));
    delay(30);
    checkIncomingCommands();
  }

  delay(55);
}
