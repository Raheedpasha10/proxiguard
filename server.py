import asyncio
import glob
import json
import os
import subprocess
import time
from aiohttp import web, ClientSession, ClientTimeout

try:
    import serial
except ImportError:
    serial = None

DEFAULT_ESP_IP = os.getenv("WIFI_HOST", "10.13.44.103")
WIFI_PORT = int(os.getenv("WIFI_PORT", "8888"))
UDP_PORT = int(os.getenv("UDP_PORT", "8888"))
SERIAL_BAUD = 9600

clients = set()

def discover_esp_ip():
    """Scans ip neigh (Android/Linux) or arp -a (macOS) for Espressif OUI"""
    try:
        out = subprocess.check_output(["ip", "neigh"], text=True, stderr=subprocess.DEVNULL)
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 5:
                ip = parts[0]
                mac = parts[4].lower()
                if any(oui in mac for oui in ["2c:f4:32", "24:0a:c4", "30:ae:a4", "84:cc:a8", "60:01:94", "5c:cf:7f"]):
                    return ip
    except Exception:
        pass

    try:
        out = subprocess.check_output(["arp", "-a"], text=True, stderr=subprocess.DEVNULL)
        for line in out.splitlines():
            if any(oui in line.lower() for oui in ["2c:f4:32", "24:0a:c4", "30:ae:a4", "84:cc:a8", "60:01:94", "5c:cf:7f"]):
                start = line.find("(")
                end = line.find(")")
                if start != -1 and end != -1:
                    return line[start+1:end]
    except Exception:
        pass

    return DEFAULT_ESP_IP

latest = {
    "connected": False,
    "distance": None,
    "hcsr04_active": True,
    "ir_active": True,
    "ir_triggered": False,
    "vib_active": True,
    "vib_triggered": False,
    "led_state": 0,
    "pin_states": {},
    "message": "Connecting to ProxiGuard IoT telemetry...",
    "source": None,
    "last_seen": None,
    "error": None
}

async def send_hardware_command(app, cmd_str):
    """Sends command string down active TCP Wi-Fi connection or local Serial"""
    if not cmd_str.endswith("\n"):
        cmd_str += "\n"
    sent = False

    writer = app.get("tcp_writer")
    if writer and not writer.is_closing():
        try:
            writer.write(cmd_str.encode("utf-8"))
            await writer.drain()
            sent = True
        except Exception as e:
            print(f"[CMD ⚠️] TCP write error: {e}")

    ser = app.get("serial_instance")
    if ser and hasattr(ser, "is_open") and ser.is_open:
        try:
            ser.write(cmd_str.encode("utf-8"))
            sent = True
        except Exception as e:
            print(f"[CMD ⚠️] Serial write error: {e}")

    if sent:
        print(f"[CMD ⚡] Dispatched: {cmd_str.strip()}")
    return sent

async def index(request):
    return web.FileResponse("index.html")

async def websocket_handler(request):
    ws = web.WebSocketResponse(heartbeat=20)
    await ws.prepare(request)
    clients.add(ws)

    await ws.send_json(latest)

    try:
        async for msg in ws:
            if msg.type == web.WSMsgType.TEXT:
                try:
                    payload = json.loads(msg.data)
                    cmd = payload.get("cmd")
                    if cmd == "PIN_SET":
                        pin = payload.get("pin")
                        val = payload.get("val", 0)
                        await send_hardware_command(request.app, f"CMD:PIN:{pin}:{val}")
                    elif cmd == "PIN_PWM":
                        pin = payload.get("pin")
                        val = payload.get("val", 0)
                        await send_hardware_command(request.app, f"CMD:PWM:{pin}:{val}")
                    elif cmd == "MOD_SET":
                        mod = payload.get("module")
                        state = payload.get("state", 0)
                        await send_hardware_command(request.app, f"CMD:MOD:{mod}:{state}")
                    elif cmd == "LCD_TEXT":
                        l1 = payload.get("line1", "")
                        l2 = payload.get("line2", "")
                        await send_hardware_command(request.app, f"CMD:LCD:{l1}|{l2}")
                except Exception as e:
                    print(f"[WS ⚠️] Parse error: {e}")
            elif msg.type == web.WSMsgType.ERROR:
                break
    finally:
        clients.discard(ws)

    return ws

async def broadcast(data):
    latest.update(data)
    dead = []
    for ws in list(clients):
        try:
            await ws.send_json(latest)
        except Exception:
            dead.append(ws)
    for ws in dead:
        clients.discard(ws)

def process_reading(line, source="Wi-Fi"):
    line = line.strip()
    if not line:
        return None

    now = time.time()
    upper = line.upper()

    # Handle multi-sensor format: e.g. DIST:24.5,IR:1,VIB:0,LED:1
    if "," in line and ":" in line:
        data = {
            "connected": True,
            "source": source,
            "last_seen": now,
            "error": None
        }
        parts = line.split(",")
        for part in parts:
            if ":" in part:
                k, v = part.split(":", 1)
                k = k.strip().upper()
                v = v.strip()
                if k == "DIST":
                    if v.upper() == "OFF":
                        data["distance"] = "OFF"
                        data["hcsr04_active"] = False
                    elif v.upper() in ["NO_ECHO", "-1"]:
                        data["distance"] = None
                        data["hcsr04_active"] = True
                    else:
                        try:
                            data["distance"] = round(float(v), 1)
                            data["hcsr04_active"] = True
                        except ValueError:
                            data["distance"] = None
                elif k == "IR":
                    # HW-201 active LOW (0 = object detected / beam broken, 1 = clear)
                    data["ir_active"] = True
                    data["ir_triggered"] = (v == "0")
                elif k == "VIB":
                    # SW-420 active HIGH pulse on shock (1 = shock detected, 0 = still)
                    data["vib_active"] = True
                    data["vib_triggered"] = (v == "1")
                elif k == "LED":
                    data["led_state"] = int(v) if v.isdigit() else 0
                elif k.startswith("P") and k[1:].isdigit():
                    pin_num = k[1:]
                    if "pin_states" not in data:
                        data["pin_states"] = dict(latest.get("pin_states", {}))
                    data["pin_states"][pin_num] = int(v) if v.isdigit() else 0

        data["message"] = f"Live multi-sensor telemetry [{source}]"
        return data

    if upper == "NO_ECHO":
        return {
            "connected": True,
            "distance": None,
            "hcsr04_active": True,
            "message": f"Sensor echo timeout (out of range / obstructed) [{source}]",
            "source": source,
            "last_seen": now,
            "error": "NO_ECHO"
        }

    candidate = line.split(":", 1)[1].strip() if upper.startswith("DIST:") else line.strip()
    if candidate.upper() == "OFF":
        return {
            "connected": True,
            "distance": "OFF",
            "hcsr04_active": False,
            "message": f"HC-SR04 in software sleep mode [{source}]",
            "source": source,
            "last_seen": now,
            "error": None
        }

    try:
        val = float(candidate)
        if 0.0 < val <= 400.0:
            return {
                "connected": True,
                "distance": round(val, 1),
                "hcsr04_active": True,
                "message": f"Receiving live sensor stream [{source}]",
                "source": source,
                "last_seen": now,
                "error": None
            }
        else:
            return {
                "connected": True,
                "distance": None,
                "hcsr04_active": True,
                "message": f"Measurement out of range ({val:.1f} cm) [{source}]",
                "source": source,
                "last_seen": now,
                "error": "OUT_OF_RANGE"
            }
    except ValueError:
        return None

class UdpTelemetryProtocol(asyncio.DatagramProtocol):
    def datagram_received(self, data, addr):
        try:
            line = data.decode("utf-8", errors="replace").strip()
            processed = process_reading(line, source=f"UDP {addr[0]}")
            if processed:
                asyncio.create_task(broadcast(processed))
        except Exception as e:
            pass

async def start_udp_listener(app):
    """Listens for UDP broadcast or unicast packets from Arduino on port 8888"""
    loop = asyncio.get_running_loop()
    try:
        transport, protocol = await loop.create_datagram_endpoint(
            lambda: UdpTelemetryProtocol(),
            local_addr=("0.0.0.0", UDP_PORT),
            allow_broadcast=True
        )
        print(f"[UDP 📡] Listening on 0.0.0.0:{UDP_PORT}")
        app["udp_transport"] = transport
    except Exception as e:
        print(f"[UDP ⚠️] Failed to bind: {e}")

async def tcp_stream_reader(app):
    """High-reliability persistent TCP client that streams directly from Arduino (port 8888)"""
    while True:
        target_ip = discover_esp_ip()
        try:
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(target_ip, WIFI_PORT),
                timeout=1.5
            )
            print(f"[TCP 🔌] Connected to ProxiGuard stream at {target_ip}:{WIFI_PORT}!")
            app["tcp_writer"] = writer

            while True:
                line_bytes = await reader.readline()
                if not line_bytes:
                    break
                line = line_bytes.decode("utf-8", errors="replace").strip()
                processed = process_reading(line, source=f"Wi-Fi TCP ({target_ip})")
                if processed:
                    await broadcast(processed)

            app["tcp_writer"] = None
            writer.close()
            await writer.wait_closed()
        except (asyncio.TimeoutError, ConnectionRefusedError, OSError):
            app["tcp_writer"] = None
            await asyncio.sleep(1.0)
        except asyncio.CancelledError:
            app["tcp_writer"] = None
            break
        except Exception as e:
            app["tcp_writer"] = None
            await asyncio.sleep(1.0)

def find_serial_port():
    patterns = [
        "/dev/cu.usbserial*",
        "/dev/cu.usbmodem*",
        "/dev/cu.wchusbserial*"
    ]
    for pattern in patterns:
        matches = glob.glob(pattern)
        if matches:
            return matches[0]
    return None

async def serial_reader(app):
    """Fallback reader for USB serial line from Arduino"""
    while True:
        try:
            port = find_serial_port()
            if serial and port and os.path.exists(port):
                s = await asyncio.to_thread(serial.Serial, port, SERIAL_BAUD, timeout=1)
                app["serial_instance"] = s
                try:
                    while True:
                        raw = await asyncio.to_thread(s.readline)
                        if raw:
                            line = raw.decode("utf-8", errors="replace")
                            processed = process_reading(line, source="USB Serial")
                            if processed:
                                await broadcast(processed)
                        await asyncio.sleep(0.01)
                finally:
                    app["serial_instance"] = None
                    await asyncio.to_thread(s.close)
            else:
                await asyncio.sleep(3)
        except asyncio.CancelledError:
            break
        except Exception:
            await asyncio.sleep(3)

async def watchdog(app):
    """Marks connection as offline if no packets received for 3 seconds"""
    while True:
        try:
            await asyncio.sleep(1.0)
            now = time.time()
            if latest.get("connected") and latest.get("last_seen"):
                if now - latest["last_seen"] > 3.0:
                    await broadcast({
                        "connected": False,
                        "distance": None,
                        "message": "Signal lost — waiting for ProxiGuard data...",
                        "source": None,
                        "error": "DISCONNECTED"
                    })
        except asyncio.CancelledError:
            break
        except Exception:
            pass

async def start_background(app):
    await start_udp_listener(app)
    app["tcp_task"] = asyncio.create_task(tcp_stream_reader(app))
    app["serial_task"] = asyncio.create_task(serial_reader(app))
    app["watchdog_task"] = asyncio.create_task(watchdog(app))

async def cleanup(app):
    if "udp_transport" in app:
        app["udp_transport"].close()
    for task_name in ["tcp_task", "serial_task", "watchdog_task"]:
        if task_name in app:
            app[task_name].cancel()
    try:
        await asyncio.gather(
            app["tcp_task"], app["serial_task"], app["watchdog_task"],
            return_exceptions=True
        )
    except Exception:
        pass

async def static_file(request):
    filename = request.match_info["filename"]
    safe_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), filename)
    if os.path.isfile(safe_path):
        return web.FileResponse(safe_path)
    return web.Response(status=404, text="Not Found")

app = web.Application()
app.router.add_get("/", index)
app.router.add_get("/ws", websocket_handler)
app.router.add_get("/{filename:.*\\.(?:glb|gltf|bin|png|jpg|svg|json|ico|css|js)}", static_file)

app.on_startup.append(start_background)
app.on_cleanup.append(cleanup)

if __name__ == "__main__":
    port = int(os.getenv("PORT", "8080"))
    print("=" * 60)
    print(f"🚀 ProxiGuard Smart Adaptive Server")
    print(f"🌐 Web Dashboard: http://localhost:{port}")
    print(f"🔌 Auto-Target:   TCP/UDP Port {WIFI_PORT}")
    print("=" * 60)
    web.run_app(app, host="0.0.0.0", port=port)
