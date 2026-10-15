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
latest = {
    "connected": False,
    "distance": None,
    "message": "Connecting to ProxiGuard IoT telemetry...",
    "source": None,
    "last_seen": None,
    "error": None
}

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

async def index(request):
    return web.FileResponse("index.html")

async def websocket_handler(request):
    ws = web.WebSocketResponse(heartbeat=20)
    await ws.prepare(request)
    clients.add(ws)

    await ws.send_json(latest)

    try:
        async for msg in ws:
            if msg.type == web.WSMsgType.ERROR:
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

    if upper == "NO_ECHO":
        return {
            "connected": True,
            "distance": None,
            "message": f"Sensor echo timeout (out of range / obstructed) [{source}]",
            "source": source,
            "last_seen": now,
            "error": "NO_ECHO"
        }

    candidate = line.split(":", 1)[1].strip() if upper.startswith("DIST:") else line.strip()
    try:
        val = float(candidate)
        if 0.0 < val <= 400.0:
            return {
                "connected": True,
                "distance": round(val, 1),
                "message": f"Receiving live sensor stream [{source}]",
                "source": source,
                "last_seen": now,
                "error": None
            }
        else:
            return {
                "connected": True,
                "distance": None,
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

            while True:
                line_bytes = await reader.readline()
                if not line_bytes:
                    break
                line = line_bytes.decode("utf-8", errors="replace").strip()
                processed = process_reading(line, source=f"Wi-Fi TCP ({target_ip})")
                if processed:
                    await broadcast(processed)

            writer.close()
            await writer.wait_closed()
        except (asyncio.TimeoutError, ConnectionRefusedError, OSError):
            await asyncio.sleep(1.0)
        except asyncio.CancelledError:
            break
        except Exception as e:
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

app = web.Application()
app.router.add_get("/", index)
app.router.add_get("/ws", websocket_handler)

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
