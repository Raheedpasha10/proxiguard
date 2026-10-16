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
 *   - 16x2 I2C Character LCD: SDA -> A4, SCL -> A5 (5V Rail)
 *   - ESP8266 ESP-01     : UTXD -> D11, URXD -> D12 (3.3V Breadboard Hub)
 *
 * Features:
 *   - High-rate non-blocking multi-sensor telemetry stream (DIST, IR, VIB, LED)
 *   - Bidirectional hardware command parser (+IPD and Serial command dispatcher)
 *   - Software-defined module power-gating and sleep modes (HCSR04, LED, IR, VIB, LCD)
 *   - Hardware I2C bus timeout protection (prevents hanging if LCD is disconnected)
 *   - Interleaved multi-client TCP socket server on port 8888
 */

#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

SoftwareSerial esp(11, 12);
LiquidCrystal_I2C lcd(0x27, 16, 2);

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
bool lcd_enabled    = true;
bool custom_lcd_msg = false;
unsigned long custom_lcd_timeout = 0;

byte currentChannel = 0;
unsigned long lastServerCheck = 0;
unsigned long lastLcdUpdate = 0;

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
      } else if (mod == F("LCD")) {
        lcd_enabled = (state != 0);
        if (lcd_enabled) {
          lcd.backlight();
        } else {
          lcd.noBacklight();
          lcd.clear();
        }
      }
    }
  }
  // CMD:LCD:<line1>|<line2>
  else if (payload.startsWith(F("LCD:"))) {
    int s1 = payload.indexOf(':');
    if (s1 > 0 && lcd_enabled) {
      String text = payload.substring(s1 + 1);
      int pipeIdx = text.indexOf('|');
      String l1 = (pipeIdx >= 0) ? text.substring(0, pipeIdx) : text;
      String l2 = (pipeIdx >= 0) ? text.substring(pipeIdx + 1) : "";
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(l1.substring(0, 16));
      lcd.setCursor(0, 1);
      lcd.print(l2.substring(0, 16));
      custom_lcd_msg = true;
      custom_lcd_timeout = millis() + 5000; // Hold custom text for 5s
    }
  }
}

void checkIncomingCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    parseCommand(cmd);
  }

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

  // Non-blocking I2C with bus reset timeout (protects if LCD is unattached)
  Wire.begin();
  Wire.setWireTimeout(3000, true);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("ProxiGuard Edge"));
  lcd.setCursor(0, 1);
  lcd.print(F("Initializing..."));

  // Ultrasonic Pins
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
  Serial.println(F("Sensors: HC-SR04 (D9/D10) | IR (D2) | VIB (D3) | LED (D8) | LCD (A4/A5)"));

  delay(2000);
  initServer();
  Serial.println(F("=== Server Port 8888 Ready ==="));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Sentinel Ready"));
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

  // 1. Read HC-SR04 Distance
  String distStr = "OFF";
  float distValue = -1.0;
  if (hcsr04_enabled) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 26000); // ~4.4m timeout
    if (duration > 0) {
      distValue = duration * 0.0343 / 2.0;
      distStr = String(distValue, 1);
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

  // 5. Update LCD display smoothly at 4 Hz (250ms interval) without flicker
  if (lcd_enabled && millis() - lastLcdUpdate >= 250) {
    lastLcdUpdate = millis();

    if (custom_lcd_msg && millis() > custom_lcd_timeout) {
      custom_lcd_msg = false;
      lcd.clear();
    }

    if (!custom_lcd_msg) {
      // Line 1: Distance & Sentinel LED status
      lcd.setCursor(0, 0);
      lcd.print(F("D:"));
      if (hcsr04_enabled) {
        if (distValue > 0) {
          lcd.print(distValue, 0);
          lcd.print(F("cm   "));
        } else {
          lcd.print(F("CLEAR  "));
        }
      } else {
        lcd.print(F("STANDBY"));
      }

      lcd.setCursor(10, 0);
      lcd.print(led_state ? F("L:ON ") : F("L:OFF"));

      // Line 2: IR tripwire and Vibration alert status
      lcd.setCursor(0, 1);
      lcd.print(F("IR:"));
      lcd.print((irVal == 0) ? F("BREACH ") : F("SECURE "));

      lcd.setCursor(10, 1);
      lcd.print((vibVal == 1) ? F("V:SHK") : F("V:OK "));
    }
  }

  // Auto-healing TCP server watchdog
  if (millis() - lastServerCheck > 10000) {
    lastServerCheck = millis();
    esp.println(F("AT+CIPSERVER=1,8888"));
    delay(30);
    checkIncomingCommands();
  }

  delay(50);
}
