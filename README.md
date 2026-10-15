# ProxiGuard

ProxiGuard is an untethered, wireless IoT proximity telemetry system combining micro-controller hardware (Arduino Uno and ESP8266 ESP-01), acoustic sensing (HC-SR04 ultrasonic sensor), and an interactive WebGL Three.js 3D Digital Twin dashboard.

The system streams continuous distance measurements over Wi-Fi (TCP socket connection) with low latency, enabling real-time monitoring on mobile devices (Android Termux) and desktop workstations without dependency on fixed local router infrastructure.

---

## Architectural Overview

```
+-------------------------------------------------------------+
|                      Hardware Layer                         |
|                                                             |
|   [HC-SR04 Sensor] <--- TRIG (D9) / ECHO (D10) ---+         |
|                                                    |         |
|                                            [Arduino Uno]    |
|                                                    |         |
|   [ESP8266 ESP-01] <--- RX (D12) / TX (D11) ------+         |
+-------------------------------------------------------------+
                               |
                   Wi-Fi 802.11 b/g/n (Port 8888)
                   TCP Multi-Client Socket
                               |
                               v
+-------------------------------------------------------------+
|                      Backend Server                         |
|                    (Python / aiohttp)                       |
|                                                             |
|   - TCP Telemetry Client Reader                             |
|   - Adaptive Hostspot & Neighbor ARP Discovery              |
|   - High-throughput WebSocket Broadcaster                   |
|   - Static Web Application Host                             |
+-------------------------------------------------------------+
                               |
                       WebSocket (/ws)
                               |
                               v
+-------------------------------------------------------------+
|                   Presentation Layer                        |
|              (WebGL / Three.js 3D Twin)                     |
|                                                             |
|   - Physically-Based Rendered (PBR) HC-SR04 Sensor Model    |
|   - 3D Volumetric Wavefront Propagation Engine              |
|   - Dynamic Obstacle Surface with Spring-Damper Interpolation|
|   - 360-Degree Orbit Controls and Studio Lighting           |
|   - Acoustic Park-Assist Synthesizer (Web Audio API)        |
+-------------------------------------------------------------+
```

---

## Hardware Specifications and Pin Configuration

### Components
1. Arduino Uno (ATmega328P, 16 MHz)
2. HC-SR04 Ultrasonic Distance Sensor (2 cm to 400 cm range, 3 mm resolution)
3. ESP8266 ESP-01 Wi-Fi Transceiver Module (802.11 b/g/n, 2.4 GHz)
4. Dedicated 3.3V Power Rail (Breadboard power supply module recommended)

### Pinout Mapping

| Component Pin | Arduino Uno Pin | Logic Level | Description |
| :--- | :--- | :--- | :--- |
| HC-SR04 VCC | 5V | 5.0V | Sensor Operating Voltage |
| HC-SR04 GND | GND | 0V | Common System Ground |
| HC-SR04 TRIG | Pin 9 | 5.0V | 10 microsecond Trigger Pulse Output |
| HC-SR04 ECHO | Pin 10 | 5.0V | Echo Timing Input |
| ESP-01 VCC | 3.3V (Ext Rail) | 3.3V | Transceiver Operating Power |
| ESP-01 CH_PD | 3.3V (Ext Rail) | 3.3V | Chip Enable (High for active) |
| ESP-01 GND | GND | 0V | Common System Ground |
| ESP-01 URXD | Pin 12 | 5.0V / 3.3V | AT Command Stream (Arduino TX) |
| ESP-01 UTXD | Pin 11 | 3.3V | Transceiver Response (Arduino RX) |

Note: Common ground between the Arduino, ESP-01, and external power supply is strictly required for stable serial signaling and pulse timing.

---

## Firmware Implementation

The firmware is located in `ProxiGuard/ProxiGuard.ino`.

### Key Characteristics
- Blind-Proof Direct Transmission: Eliminates blocking AT command handshakes (`find(">")`) that fail when 3.3V UART signals operate near the threshold of ATmega328P interrupt pins.
- Interleaved Multi-Client TCP Server: Implements non-blocking channel cycling (Sockets 0, 1, 2) on port 8888, enabling concurrent telemetry streams to multiple network consumers (e.g., both a mobile host and a laptop workstation).
- Self-Healing Connection Loop: Continuously checks the Wi-Fi server state and re-establishes the TCP listener on port 8888 if the access point resets or re-associates.

---

## Backend Server Architecture

The server (`server.py`) is built using Python 3 and `aiohttp`.

### Operational Modes
1. Direct TCP Socket Streaming: Actively connects to the ESP8266 server endpoint at `10.13.44.103:8888`.
2. Automatic Hardware Discovery: Inspects Linux/Android `ip neigh` cache and macOS `arp -a` tables to dynamically locate Espressif MAC addresses (OUIs: `2c:f4:32`, `24:0a:c4`, `30:ae:a4`, `84:cc:a8`, `60:01:94`, `5c:cf:7f`).
3. Dual-Channel WebSocket Ingestion: Ingests telemetry strings (e.g. `DIST:16.5`), computes statistical bounds and sample rates, and broadcasts clean JSON payloads to all connected browser clients.

---

## 3D Digital Twin Dashboard

The dashboard (`index.html`) is rendered via WebGL utilizing Three.js.

### Features
- Physically-Based Rendering (PBR): Brushed aluminum metallic cylinders (`metalness: 0.92`, `roughness: 0.22`), fiberglass PCB board with gold-plated mounting vias, crystal oscillator, and header pins.
- Dynamic Acoustic Propagation: Toroidal 3D soundwave rings physically emit from the transmitter cylinder, travel down the Z-axis, and reflect upon contact with the obstacle.
- Physics-Based Obstacle Tracking: Employs linear interpolation (`lerp`) to simulate fluid inertia as the obstacle surface translates in real time.
- Integrated Studio Acoustic Synthesizer: Utilizes the browser Web Audio API to synthesize acoustic parking-assist chimes with variable intervals based on real-time proximity.
- Interactive Camera Controls: Supports full 360-degree rotation, tilt, and zoom via touch and mouse gestures, accompanied by isometric, top-down, and frontal camera angle presets.

---

## Installation and Deployment

### 1. Arduino Firmware Upload

Compile and flash the firmware using `arduino-cli`:

```bash
arduino-cli compile --fqbn arduino:avr:uno ProxiGuard
arduino-cli upload -p /dev/cu.usbserial-1110 --fqbn arduino:avr:uno ProxiGuard
```

### 2. Running on Mobile (Android via Termux)

1. Connect the phone hotspot (2.4 GHz band).
2. Launch Termux and install dependencies:
   ```bash
   pkg update
   pkg install python git
   pip install aiohttp
   ```
3. Run the telemetry server:
   ```bash
   python3 server.py
   ```
4. Access the dashboard in Chrome at:
   `http://localhost:8080`

### 3. Running on Desktop (macOS / Linux)

1. Ensure Python 3.10+ is installed.
2. Install dependencies:
   ```bash
   pip install aiohttp pyserial
   ```
3. Start the server:
   ```bash
   python3 server.py
   ```
4. Open your browser at:
   `http://localhost:8080`

---

## License

This project is licensed under the MIT License.
