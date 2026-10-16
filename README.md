# ProxiGuard: Autonomous IoT Multi-Sensor Spatial Sentinel and Hardware Digital Twin

ProxiGuard is an untethered, distributed cyber-physical IoT telemetry and security sentinel workstation. The system integrates multi-modal acoustic, optical, and seismic sensory instrumentation with bi-directional software-defined hardware control, an asynchronous edge gateway, and an interactive real-time WebGL Three.js 3D Digital Twin dashboard.

Designed for operation in untethered mobile environments (such as Android Termux hotspot hosts) and fixed desktop workstations, ProxiGuard maintains continuous high-frequency telemetry streaming and remote circuit management without reliance on static local router infrastructure.

---

## Architectural Overview

```
+---------------------------------------------------------------------------------+
|                                Hardware Layer                                   |
|                                                                                 |
|  [HC-SR04 Ultrasonic] <--- TRIG (D9) / ECHO (D10) ---+                          |
|  [HW-201 IR Tripwire] <--- OUT (D2) -----------------+                          |
|  [SW-420 Shock Guard] <--- DO (D3) ------------------+                          |
|  [Sentinel Green LED] <--- ANODE (D8) ---------------+                          |
|  [16x2 I2C Display]   <--- SDA (A4) / SCL (A5) ------+                          |
|                                                      |                          |
|                                          [Arduino Uno R3 SMD]                   |
|                                          (ATmega328P @ 16MHz)                   |
|                                                      |                          |
|  [ESP8266 ESP-01]     <--- RX (D12) / TX (D11) ------+                          |
|  [3.3V Power Hub]     <--- VCC (3.3V) / CH_PD (3.3V)-+                          |
+---------------------------------------------------------------------------------+
                                       |
                         Wi-Fi 802.11 b/g/n (Port 8888)
                         Multi-Client TCP Socket Stream
                                       |
                                       v
+---------------------------------------------------------------------------------+
|                                Edge Server Tier                                 |
|                               (Python / aiohttp)                                |
|                                                                                 |
|   - Non-blocking Asynchronous TCP Telemetry Stream Client                       |
|   - Adaptive Hostspot & Neighbor ARP Table Auto-Discovery                       |
|   - High-Throughput Bidirectional WebSocket Hub (/ws)                           |
|   - Static Web Application & CAD Asset Distribution Engine                      |
+---------------------------------------------------------------------------------+
                                       |
                             WebSocket (/ws:8080)
                             Bidirectional JSON Pipeline
                                       |
                                       v
+---------------------------------------------------------------------------------+
|                            Presentation & Control Tier                          |
|                           (WebGL / Three.js 3D Twin)                            |
|                                                                                 |
|   - Physically-Based Rendered (PBR) Spatial Sensor Playground                   |
|   - 3D Volumetric Wavefront Engine & Inertial Obstacle Tracking                 |
|   - Authentic CAD-Derived SMD Digital Twin (Official A000073 Alignment)         |
|   - Real-time Pin Hover Telemetry HUD & Circuit Inspector                       |
|   - Software-Defined Component Sleep Gates & Remote GPIO Controller             |
|   - Web Audio API Cinematic Sonar Synthesizer                                   |
+---------------------------------------------------------------------------------+
```

---

## Hardware Specifications and Bill of Materials

### 1. Primary Processing Core: Arduino Uno R3 SMD Edition
- Microcontroller: Microchip ATmega328P-AU in 32-lead Thin Quad Flat Package (TQFP-32).
- Clock Frequency: 16.000 MHz external HC-49S quartz crystal oscillator.
- Operating Voltage: 5.0 V DC nominal.
- Flash Memory: 32 KB (with 0.5 KB utilized by optiboot bootloader).
- SRAM: 2 KB; EEPROM: 1 KB.
- USB-to-UART Bridge: WCH CH340G in SOP-16 package with dedicated 12.000 MHz crystal.
- Integrated Multi-Rail Header Infrastructure:
  - Dual 5V DC power rails: Pin 1 (`5V Aux`) and Pin 4 (`5V Main`).
  - Dual ground return rails: Pin 5 (`GND 1`) and Pin 6 (`GND 2`).
  - Dedicated 3x4 expansion matrix providing direct headers for `3.3V / 3.3V / GND / GND`, `SCL / SDA / 5V / GND`, and `RX / TX / 5V / GND`.

### 2. Spatial Range Transceiver: HC-SR04 Ultrasonic Distance Sensor
- Transducer Frequency: 40 kHz acoustic burst.
- Operating Voltage: 5.0 V DC.
- Measurement Range: 2.0 cm to 400.0 cm.
- Measurement Angle: 15-degree cone of dispersion.
- Trigger Input: 10 microsecond TTL high pulse.
- Echo Output: TTL pulse proportional to acoustic round-trip flight duration.

### 3. Optical Micro-Tripwire: HW-201 / TCRT5000 Active Infrared Sensor
- Emitter: 940 nm gallium arsenide (GaAs) infrared emitting diode.
- Detector: NPN phototransistor in visible light daylight filtering casing.
- Comparator: Texas Instruments LM393 dual differential voltage comparator.
- Detection Envelope: 2 mm to 30 cm adjustable via single-turn potentiometer.
- Output: Active-low digital signal upon optical beam reflectance.

### 4. Seismic and Anti-Tamper Guard: SW-420 Vibration Sensor Module
- Switching Element: High-sensitivity omnidirectional spring-mass contact mechanism.
- Comparator: LM393 with precision trimmer threshold adjustment.
- Operating Voltage: 3.3 V to 5.0 V DC.
- Function: Transient physical shock, enclosure tampering, and vibration detection.

### 5. Wireless Telemetry Gateway: Espressif ESP8266 ESP-01
- Core Architecture: Tensilica Xtensa 32-bit L106 Diamond RISC processor @ 80 MHz.
- Wi-Fi Compliance: IEEE 802.11 b/g/n (2.4 GHz band, WPA/WPA2-PSK security).
- Operating Current: 80 mA to 170 mA (RF active burst peak).
- Transmission Protocol: Multi-client raw TCP stream server on port 8888.

### 6. Local Autonomous Display: 16x2 Character LCD with PCF8574 I2C Backpack
- Display Controller: Hitachi HD44780 standard character generator.
- Bus Translation: NXP PCF8574 remote 8-bit I/O expander for I2C bus.
- Default Bus Address: 0x27 (or 0x3F dependent on A0-A2 jumper configuration).
- Lines: 2 rows of 16 characters with independent backlight control.

### 7. Visual Sentinel Annunciator: Green High-Efficiency LED
- Operating Forward Voltage: 2.1 V to 2.4 V DC.
- Current-Limiting Element: 220 Ohm metal film resistor (12 mA drive current).

---

## Complete Electrical Interconnect Matrix

| Component | Pin Label | Arduino Uno Pin | Voltage Level | Electrical Function | Typical Current |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **HC-SR04** | VCC | 5V (Main) | 5.04V DC | Sensor VCC Supply | 15 mA |
| **HC-SR04** | GND | GND 1 | 0.00V DC | Ground Return | Return |
| **HC-SR04** | TRIG | Pin D9 (PWM) | 5.00V TTL | 10 us Pulse Burst Trigger | < 1 mA |
| **HC-SR04** | ECHO | Pin D10 | 5.00V TTL | Echo Round-Trip Timing Read | < 1 mA |
| **ESP-01** | VCC | 3.3V (Rail Hub) | 3.31V DC | Radio Power Supply | 80 - 140 mA |
| **ESP-01** | CH_PD | 3.3V (Rail Hub) | 3.31V DC | Chip Enable (Active High) | < 1 mA |
| **ESP-01** | GND | GND 2 | 0.00V DC | Ground Return | Return |
| **ESP-01** | UTXD | Pin D11 (RX) | 3.30V TTL | SoftwareSerial Ingestion | < 1 mA |
| **ESP-01** | URXD | Pin D12 (TX) | 5.00V TTL | AT Command / Data Dispatch | ~2 mA |
| **HW-201 IR** | VCC | 5V (Aux) | 5.04V DC | Infrared Emitter / IC Supply | 15 mA |
| **HW-201 IR** | GND | GND 1 | 0.00V DC | Ground Return | Return |
| **HW-201 IR** | OUT | Pin D2 (INT0) | 5.00V TTL | Active-Low Optical Tripwire | < 1 mA |
| **SW-420** | VCC | 5V (Aux) | 5.04V DC | Comparator Bias Voltage | 5 mA |
| **SW-420** | GND | GND 1 | 0.00V DC | Ground Return | Return |
| **SW-420** | DO | Pin D3 (INT1) | 5.00V TTL | Digital Vibration Logic State | < 1 mA |
| **Green LED** | Anode (+) | Pin D8 | 5.00V TTL | Sentinel Armed Status Output | 12 mA |
| **Green LED** | Cathode (-) | GND 2 (via 220R)| 0.00V DC | Current Limiter to Ground | Return |
| **I2C LCD** | VCC | 5V (Aux) | 5.04V DC | Logic and Backlight Power | 30 mA |
| **I2C LCD** | GND | GND 1 | 0.00V DC | Ground Return | Return |
| **I2C LCD** | SDA | Pin A4 (SDA) | 5.00V I2C | Hardware Two-Wire Serial Data| < 1 mA |
| **I2C LCD** | SCL | Pin A5 (SCL) | 5.00V I2C | Hardware Two-Wire Serial Clock| < 1 mA |

---

## Mathematical Formulations and Physics Principles

### 1. Acoustic Time-of-Flight (ToF) Spatial Calculation

Distance determination with the ultrasonic sensor relies on the speed of sound propagation through dry ambient air:

$$d = \frac{v_{\text{sound}} \times \Delta t}{2}$$

where $\Delta t$ represents the round-trip duration recorded by the microcontroller's timer capture, and $v_{\text{sound}}$ is a function of ambient temperature $T$ in degrees Celsius:

$$v_{\text{sound}}(T) = 331.3 \times \sqrt{1 + \frac{T}{273.15}} \approx 343.2 \text{ m/s at } 20^\circ\text{C}$$

Expressing speed in engineering units convenient for microsecond counter evaluation:

$$v_{\text{sound}} = 0.03432 \text{ cm/\mu s}$$

$$d \,(\text{cm}) = \frac{\Delta t \,(\mu\text{s})}{58.28}$$

### 2. Spring-Damper Inertial Interpolation

To eliminate high-frequency acoustic jitter while preserving physical dynamics in the 3D WebGL viewport, incoming spatial points are filtered via an exponential moving relaxation model:

$$x_t = x_{t-1} + \lambda \cdot (x_{\text{target}} - x_{t-1})$$

where the relaxation factor $\lambda = 0.085$ balances responsive tracking against high-frequency transducer noise.

Velocity estimation is derived continuously across discrete sampling intervals:

$$v_t = \frac{x_t - x_{t-1}}{\Delta t}$$

### 3. Dynamic Sonar Synthesizer Modulation

The Web Audio API synthesizer computes auditory parking-assist cues based on normalized proximity:

$$\eta = \text{clamp}\left(\frac{d - d_{\text{min}}}{d_{\text{max}} - d_{\text{min}}}, \, 0, \, 1\right)$$

Synthesizer carrier frequency ($f$) and pulse repetition interval ($\tau$) scale dynamically:

$$f(d) = f_{\text{base}} + (1 - \eta) \times \Delta f = 420\text{ Hz} + (1 - \eta) \times 860\text{ Hz}$$

$$\tau(d) = \tau_{\text{min}} + \eta \times (\tau_{\text{max}} - \tau_{\text{min}}) = 75\text{ ms} + \eta \times 365\text{ ms}$$

When proximity penetrates the critical boundary ($d \le 18\text{ cm}$), the synthesizer transitions from a sinusoidal carrier to a high-order resonant harmonic waveshape supplemented by a 75 Hz sub-bass transient.

### 4. Power Consumption and Headroom Budget

Total continuous load across all concurrent active channels is calculated against the 500 mA standard USB bus constraint:

$$I_{\text{total}} = I_{\text{MCU\_Base}} + I_{\text{Wi-Fi}} + I_{\text{Sonar}} + I_{\text{IR}} + I_{\text{Vibration}} + I_{\text{LCD}} + I_{\text{LED}}$$

$$I_{\text{total}} = 45\text{ mA} + 110\text{ mA} + 15\text{ mA} + 15\text{ mA} + 5\text{ mA} + 30\text{ mA} + 12\text{ mA} = 232\text{ mA}$$

$$\text{Headroom} = 500\text{ mA} - 232\text{ mA} = 268\text{ mA safe operating margin (53.6%)}$$

---

## Firmware and Communication Pipeline

### 1. Blind-Proof Transmission Routine
Traditional Arduino ESP8266 AT firmware implementations frequently stall in blocking loops waiting for the serial character `>`. The ProxiGuard firmware bypasses buffer stalls by employing direct transmission:
- Pre-computes exact payload length headers (`AT+CIPSEND=<channel>,<len>`).
- Enforces synchronous single-pass write operations without CPU blocking.
- Guarantees continuous sampling throughput exceeding 10 Hz without dropping UART bytes.

### 2. Multi-Channel Interleaved TCP Streamer
The ESP8266 operates as an active multi-client TCP server on port 8888. The firmware cycles channels non-destructively:
- Distributes identical distance measurements concurrently across connected endpoints (e.g. Android phone running the gateway and desktop browser).
- Implements connection-watchdog state machines that auto-detect client disconnections and reset the TCP listen socket without requiring an MCU restart.

### 3. Bi-directional Command Dispatch Syntax
Remote commands generated from the dashboard travel through the reverse TCP channel to the Arduino:
- `CMD:PIN:<id>:<state>` — Toggles individual GPIO logic levels directly.
- `CMD:MOD:<module>:<state>` — Sets software sleep/active states for attached sensor modules.
- `CMD:LCD:<text>` — Writes alphanumeric text strings directly to the physical 16x2 I2C display.

---

## 3D Digital Twin and Board Status Inspector

### 1. Spatial Telemetry Playground
- Physically-based material simulation for the HC-SR04 ultrasonic sensor with brushed aluminum specular reflections, gold pins, and fiberglass PCB substrate.
- Dynamic 3D acoustic wavefront rings propagating down the Z-axis that deform upon intersecting obstacle geometries.
- Full 360-degree orbital controls with touch gesture support, perspective presets (Isometric, Top-Down, Front-Facing), and continuous smooth inertia.

### 2. Authentic CAD-Derived SMD Digital Twin (A000073)
- Rendered using exact sub-millimeter coordinates extracted directly from official factory Autodesk EAGLE board layouts (`UNOSMD_V3.brd`).
- Faithfully represents the physical board layout:
  - Royal blue solder mask finish.
  - Square ATmega328P-AU 32-TQFP surface-mount IC with 32 perimeter solder leads.
  - WCH CH340G USB interface controller in SOP-16 package.
  - Correct power header configuration: `[5V Aux] [RES] [3.3V] [5V Main] [GND 1] [GND 2] [VIN]`.
  - On-board 3x4 multi-rail expansion matrix (`3.3V/3.3V/GND/GND`, `SCL/SDA/5V/GND`, `RX/TX/5V/GND`).

### 3. Real-Time Pin Hover HUD
- Interactive vector-aligned detection zones for every pin socket on the board.
- Instant HUD tooltip anchor over the hovered pin detailing:
  - Hardware Role and Circuit Assignment.
  - Electrical Logic Level and Active Voltage.
  - Signal State and Pulse Timing.
  - Real-Time Load Current Draw and Connected Target.
- Touch-compatible pin selection with synchronized telemetry card expansion.

---

## Deployment and Operation Guide

### 1. Firmware Compilation and Flashing

Prerequisites: Install `arduino-cli` and the AVR core:

```bash
arduino-cli core update-index
arduino-cli core install arduino:avr
```

Compile and flash the firmware to the Arduino Uno R3 SMD:

```bash
arduino-cli compile --fqbn arduino:avr:uno ProxiGuard
arduino-cli upload -p /dev/cu.usbserial-1110 --fqbn arduino:avr:uno ProxiGuard
```

### 2. Mobile Edge Gateway Deployment (Android via Termux)

1. Enable your Android hotspot (2.4 GHz band, SSID matching firmware configuration).
2. Open Termux and ensure prerequisites are present:
   ```bash
   pkg update
   pkg install python git
   pip install aiohttp
   ```
3. Clone or pull the repository:
   ```bash
   git clone https://github.com/Raheedpasha10/proxiguard.git
   cd proxiguard
   ```
4. Start the gateway server:
   ```bash
   python3 server.py
   ```
5. Open Google Chrome on the phone and navigate to:
   ```
   http://localhost:8080
   ```

### 3. Workstation Deployment (macOS / Linux)

1. Ensure Python 3.10+ is installed on the workstation.
2. Install dependencies:
   ```bash
   pip install aiohttp pyserial
   ```
3. Start the server:
   ```bash
   python3 server.py
   ```
4. Access the dashboard from any browser:
   ```
   http://localhost:8080
   ```

---

## Repository Structure

```
proxiguard/
├── .gitignore                      # Comprehensive build, system, and environment ignore rules
├── A000073-cad-files/              # Official Autodesk EAGLE CAD hardware source files
│   ├── License.txt                 # CC BY-SA 4.0 International Hardware License
│   ├── UNOSMD_V3.brd               # Factory PCB layout, copper routing, and silkscreen
│   └── UNOSMD_V3.sch               # Complete multi-sheet electrical circuit schematic
├── ProxiGuard/
│   └── ProxiGuard.ino              # Production multi-client non-blocking streaming firmware
├── ProxiGuard.ino                  # Root firmware reference
├── arduino_uno_smd.svg             # Millimeter-accurate vector CAD digital twin
├── index.html                      # Three.js 3D spatial twin and interactive pinout console
├── server.py                       # Asynchronous Python TCP/WebSocket gateway
└── README.md                       # Comprehensive engineering documentation
```

---

## License

The hardware design files located in `A000073-cad-files/` are distributed under the Creative Commons Attribution-ShareAlike 4.0 International License (CC BY-SA 4.0). All software, firmware, and WebGL assets are licensed under the MIT License.
