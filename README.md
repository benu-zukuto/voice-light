# 🎙️ Voice Light
### Offline Voice & Web Controlled Smart Classroom

> **A privacy-first smart classroom automation system powered by ESP32-S3 N8R8 — controlled by offline voice commands or a real-time local web dashboard.**

---

## ✨ Overview

**Voice Light** is a dual-mode smart classroom automation system built around the **ESP32-S3 N8R8** microcontroller. It combines **Espressif ESP-SR MultiNet 7 English speech recognition** with a device-hosted web dashboard, allowing classroom utilities to be controlled without cloud services, internet access, or third-party voice-assistant APIs.

### 🎯 Core Goals

- 🔒 **Privacy-first** — speech recognition happens locally on the ESP32-S3.
- ⚡ **Low-latency control** — commands are processed directly on-device.
- 🎙️ **Offline voice control** — no cloud speech API is required.
- 🌐 **Local web control** — control the classroom from a phone, tablet, or laptop.
- 🔄 **Real-time synchronization** — voice and web controls share the same relay state.
- 🏫 **Classroom-focused automation** — individual loads and useful classroom scenes can be controlled quickly.

---

## 🚀 Key Features

| Feature | Description |
|---|---|
| 🎙️ Offline Speech Recognition | Espressif **ESP-SR MultiNet 7 English** runs locally on the ESP32-S3. |
| 🌐 Local Web Dashboard | A responsive dashboard is hosted directly by the ESP32-S3. |
| 🔌 4-Channel Relay Control | Independently controls TV/screen, stage lighting, and classroom lighting circuits. |
| 🎬 Scene Presets | Quickly activate **Class Start**, **Presentation Mode**, or **Power Off**. |
| 🔄 WebSocket Sync | Voice and web actions are synchronized in real time. |
| 🧠 Dual-Core Architecture | Voice processing and networking are separated across the ESP32-S3 cores. |
| 📶 SoftAP Mode | Creates its own local Wi-Fi network for direct device access. |
| 🛡️ No Cloud Dependency | Designed to operate without external servers or internet connectivity. |

---

# 🧩 System Architecture

```text
                         ┌─────────────────────────┐
                         │      USER / TEACHER     │
                         └────────────┬────────────┘
                                      │
                    ┌─────────────────┴─────────────────┐
                    │                                   │
              🎙️ Voice Command                    📱 Web Dashboard
                    │                                   │
                    ▼                                   ▼
             INMP441 Microphone                  Local Wi-Fi / SoftAP
                    │                                   │
                    ▼                                   ▼
             I2S DMA Audio                       WebSocket / HTTP
                    │                                   │
                    ▼                                   │
             Audio DSP Pipeline                       │
                    │                                   │
                    ▼                                   │
             ESP-SR MultiNet 7                         │
                    │                                   │
                    └──────────────┬────────────────────┘
                                   ▼
                            ┌──────────────┐
                            │ StateManager │
                            └──────┬───────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │                             │
                    ▼                             ▼
              Relay Outputs                JSON State Updates
                    │                             │
                    ▼                             │
             Classroom Loads ◄────────────────────┘
```

---

# 🧠 Dual-Core Processing

Voice Light uses a **dual-core processing and hybrid control architecture** so voice recognition can continue while the web interface handles network traffic.

### Core 0 — 🎙️ Voice DSP Pipeline

- Continuously captures audio from the **INMP441** through I2S DMA.
- Applies a high-pass DC-blocker filter:

```text
y[n] = x[n] - x[n-1] + 0.995 · y[n-1]
```

- Applies dynamic digital gain processing.
- Normalizes PCM audio chunks.
- Feeds audio into the **MultiNet 7** speech recognition engine.
- Matches recognized speech against predefined classroom commands.

### Core 1 — 🌐 Networking & Web Control

- Hosts the local Wi-Fi SoftAP.
- Runs the asynchronous web server.
- Manages the `/ws` WebSocket endpoint.
- Serves a responsive mobile-first HTML/CSS dashboard.
- Receives web commands and updates the shared `StateManager`.
- Broadcasts relay state changes to connected clients in real time.

---

# 🔌 Hardware

## Required Components

| Component | Specification |
|---|---|
| 🧠 Microcontroller | **ESP32-S3 N8R8** — 8MB Flash + 8MB OPI PSRAM |
| 🎙️ Microphone | **INMP441** I2S omnidirectional digital microphone |
| 🔌 Relay | **4-channel relay module** |
| ⚡ Power | Stable **5V / 2A+ DC** power source |
| 📱 Client | Smartphone, tablet, or laptop with Wi-Fi |

---

## 📍 Pin Mapping

| Peripheral | Function / Signal | ESP32-S3 GPIO | Notes |
|---|---|---:|---|
| 🎙️ INMP441 | WS / LRCLK | `GPIO 15` | I2S standard mode |
| 🎙️ INMP441 | SD / DOUT | `GPIO 16` | I2S standard mode |
| 🎙️ INMP441 | SCK / BCLK | `GPIO 17` | I2S standard mode |
| 🔌 Relay CH1 | Smart TV / Screen | `GPIO 4` | Active LOW |
| 💡 Relay CH2 | Stage Light | `GPIO 5` | Active LOW |
| 💡 Relay CH3 | Classroom Lights A | `GPIO 6` | Active LOW |
| 💡 Relay CH4 | Classroom Lights B | `GPIO 7` | Active LOW |

> ⚠️ **Safety:** Relay outputs may switch mains-voltage equipment. Use properly rated relays, fuses, enclosures, isolation, and qualified electrical installation practices. Never work on exposed mains wiring while powered.

---

# 🛠️ Arduino IDE Configuration

Configure the ESP32-S3 board from **Tools → Board / Tools** as follows:

| Setting | Recommended Value |
|---|---|
| **Board** | `ESP32S3 Dev Module` |
| **Flash Size** | `8MB (64Mb)` |
| **PSRAM** | `OPI PSRAM` |
| **Flash Mode** | `QIO 80MHz` / `OPI 80MHz`* |
| **CPU Frequency** | `240MHz (WiFi)` |
| **Partition Scheme** | `8MB with spiffs` / custom model partition* |
| **Upload Speed** | `921600` |
| **USB CDC On Boot** | `Enabled` |
| **USB Mode** | `Hardware CDC and JTAG` |
| **Events Run On** | `Core 1` |
| **Arduino Runs On** | `Core 1` |

\* Exact flash mode and partition options can depend on the ESP32 board package and board variant. The model partition must have enough space for the selected ESP-SR model.

---

# 📚 Software & Libraries

## Development Environment

- **Arduino IDE**
- **Espressif ESP32 Arduino Core v3.x / ESP-IDF-supported environment**
- **ESP-SR MultiNet 7**

## Required Libraries / Components

```text
WiFi
AsyncTCP
ESPAsyncWebServer
esp_mn_iface.h
esp_mn_models.h
model_path.h
```

`WiFi` is provided by the ESP32 Arduino core. The ESP-SR headers/components must be available in the selected ESP32-S3 build environment.

---

# 🧠 ESP-SR MultiNet 7

Voice Light uses **Espressif's native ESP-SR MultiNet 7 English speech recognition engine** for local command recognition.

### Recognition Flow

```text
INMP441
   │
   ▼
I2S DMA
   │
   ▼
Audio Filtering
   │
   ▼
Gain + PCM Normalization
   │
   ▼
ESP-SR MultiNet 7
   │
   ▼
Command Match
   │
   ▼
StateManager
   │
   ▼
Relay Action
```

The recognition model is stored in a dedicated flash/model partition.

---

# 🎤 Voice Commands

Voice Light is designed around simple classroom-friendly commands.

| ID | Action | Example Spoken Phrases |
|:---:|---|---|
| **1** | 🟢 **Class Start** — All Relays ON | `Hey zuku, class start` · `zuku, class start` |
| **2** | 🔴 **Class End** — All Relays OFF | `Hey zuku, power off` · `zuku, power off` |
| **3** | 🎬 **Presentation Mode** | `Hey zuku, presentation mode` · `zuku, presentation mode` |
| **4** | 📺 **Turn TV / Screen ON** | `Hey zuku, screen activate` · `zuku, screen activate` |
| **5** | 📺 **Turn TV / Screen OFF** | `Hey zuku, tv off` · `zuku, tv off` |
| **6** | 💡 **Stage Light ON** | `Hey zuku, stage on` · `zuku, stage on` |
| **7** | 💡 **Stage Light OFF** | `Hey zuku, stage off` · `zuku, stage off` |
| **8** | 💡 **Classroom Lights ON** | `Hey zuku, lights on` · `zuku, lights on` |
| **9** | 🌙 **Classroom Lights OFF** | `Hey zuku, lights off` · `zuku, lights off` |

> 💡 **Tip:** Keep commands short and distinct to improve reliable recognition in a real classroom environment.

---

# 🎬 Classroom Scene Presets

## 🟢 Class Start

Activates the classroom's configured loads together.

```text
Class Start
   ├── Relay 1 → ON
   ├── Relay 2 → ON
   ├── Relay 3 → ON
   └── Relay 4 → ON
```

## 🎬 Presentation Mode

Activates the configured presentation scene.

> The exact relay combination for this scene should match the implementation in the firmware.

## 🔴 Power Off

Turns all configured relay outputs OFF.

```text
Power Off
   ├── Relay 1 → OFF
   ├── Relay 2 → OFF
   ├── Relay 3 → OFF
   └── Relay 4 → OFF
```

---

# 🌐 Web Control

The ESP32-S3 creates a local Wi-Fi SoftAP and hosts the control dashboard directly on the device.

### Control Flow

```text
📱 Phone / Tablet / Laptop
          │
          │ Local Wi-Fi
          ▼
   ESP32-S3 SoftAP
          │
          ▼
   Web Dashboard
          │
          ▼
     WebSocket /ws
          │
          ▼
     StateManager
          │
          ▼
      4 Relays
```

The dashboard is designed to be **mobile-first**, while WebSockets provide real-time state synchronization without requiring manual page refreshes.

---

# 📡 Local Network

The current project configuration uses:

```text
SSID:      ズクト-Project-Voice-Light
Password:  voicelight@benu
```

> 🔐 If this project is deployed beyond a controlled demonstration environment, change the default credentials before use.

---

# 💾 Flashing Model & Firmware

Voice Light requires both the application firmware and the speech-recognition model data.

## 1. Configure the Partition

In Arduino IDE:

1. Select **ESP32S3 Dev Module**.
2. Select an appropriate flash size for the N8R8 board.
3. Enable **OPI PSRAM**.
4. Select or create a partition layout with enough space for the speech model.

## 2. Flash the Speech Model

Flash the `srmodels` / MultiNet model binary into the designated `model` partition using the appropriate ESP-IDF tooling or compatible data-upload workflow.

## 3. Flash the Firmware

1. Connect the ESP32-S3 over USB.
2. Open:

```text
esp-voice.ino
```

3. Select the correct serial port.
4. Upload the firmware.
5. Open the serial monitor and verify startup logs.

---

# 📁 Suggested Project Structure

```text
Voice-Light/
├── esp-voice.ino
├── README.md
├── model/
│   └── MultiNet 7 model data
├── data/
│   └── web dashboard assets
└── partitions.csv
```

> The exact directory structure depends on how the firmware and model partition are packaged in your implementation.

---

# ⚙️ Control Logic

All control paths converge on a shared state-management layer:

```text
                    ┌──────────────────┐
Voice Command ────► │                  │
                    │   StateManager   │ ───► Relay Outputs
Web Button ───────► │                  │
                    └────────┬─────────┘
                             │
                             ▼
                     WebSocket Broadcast
                             │
                             ▼
                     Connected Clients
```

This architecture means a relay state change can be reflected across connected web clients regardless of whether the original action came from **voice** or the **web dashboard**.

---

# 🔐 Privacy & Offline Operation

Voice Light is designed around local processing:

- 🚫 No cloud speech-recognition service
- 🚫 No third-party voice-assistant API
- 🚫 No external server required for normal operation
- ✅ Voice processing runs locally
- ✅ Web dashboard is hosted locally
- ✅ Classroom control remains available without internet access

> **Offline does not automatically mean secure.** Physical access, Wi-Fi credentials, firmware integrity, relay wiring, and electrical safety should still be considered during deployment.

---

# 🌟 Advantages

### 🔒 No Cloud Dependency
The system can operate without an internet connection, avoiding cloud fees and dependence on external services.

### 🎙️ Hands-Free Classroom Control
Teachers can control classroom equipment using short spoken commands.

### 📱 Dual-Mode Redundancy
Voice control and the web interface provide two independent interaction methods.

### 🧠 Optimized Dual-Core Execution
Voice DSP and networking are separated to reduce interference between audio processing and network tasks.

### ⚡ Real-Time State Updates
WebSocket communication allows connected clients to receive relay-state changes immediately.

### 🏫 Built for Real Classrooms
The command set and scene presets are designed around common classroom operations such as starting class, presenting, and shutting down equipment.

---

# 🧪 Testing Checklist

Before connecting real classroom equipment, test the system with low-voltage loads.

- [ ] ESP32-S3 boots correctly
- [ ] OPI PSRAM is detected
- [ ] INMP441 audio input works
- [ ] MultiNet 7 model loads successfully
- [ ] Voice commands are recognized
- [ ] Relay logic matches Active-Low configuration
- [ ] Each relay can be controlled independently
- [ ] Class Start preset works
- [ ] Presentation Mode works
- [ ] Power Off preset works
- [ ] Web dashboard loads from a client device
- [ ] WebSocket connection remains stable
- [ ] Voice-triggered changes appear on the dashboard
- [ ] Web-triggered changes update relay states
- [ ] Multiple connected clients stay synchronized
- [ ] Default Wi-Fi credentials are changed before real deployment
- [ ] Mains-voltage installation has been professionally checked

---

# 🐛 Troubleshooting

### MultiNet model is not found

Check that:

1. The model partition exists.
2. The partition table matches the firmware.
3. The model data was flashed to the expected partition.
4. The ESP-SR components are available to the build.

### Voice recognition is unreliable

Check:

- INMP441 wiring and I2S pin assignments.
- Microphone orientation and placement.
- Background noise.
- Audio gain and normalization.
- MultiNet model configuration.
- Distance between speaker and microphone.

### Relay behaves backwards

The relay module may use different trigger logic.

For an **Active-Low** relay:

```text
LOW  → Relay ON
HIGH → Relay OFF
```

Verify the module before connecting external loads.

### Web dashboard does not update

Check:

- ESP32 SoftAP connection.
- WebSocket `/ws` endpoint.
- Client network connection.
- Serial logs for connection errors.
- Whether the `StateManager` broadcasts state changes.

---

# 🛣️ Future Roadmap

Potential extensions for the project:

- [ ] More classroom voice commands
- [ ] Additional relay channels
- [ ] Custom scene configuration
- [ ] Device status indicators
- [ ] Energy monitoring
- [ ] Temperature / humidity monitoring
- [ ] Scheduled classroom scenes
- [ ] User-configurable Wi-Fi credentials
- [ ] Improved command feedback
- [ ] Multilingual voice-command support

---

# 📜 License

Add the project's chosen license here, for example:

```text
MIT License
```

> Replace this section with the actual license before publishing the repository.

---

# 👨‍💻 Project

**Project:** Voice Light  
**Platform:** ESP32-S3 N8R8  
**Voice Engine:** Espressif ESP-SR MultiNet 7  
**Control:** Offline Voice + Local Web Dashboard  
**Connectivity:** ESP32-S3 Wi-Fi SoftAP  
**Application:** Smart Classroom Automation

---

<p align="center">

### 🎙️ Voice. 🌐 Web. ⚡ Offline.

**Smart classroom control — without the cloud.**

</p>
