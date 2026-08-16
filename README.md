Project-Voice Light 
An Offline Voice & Web Controlled Smart Classroom

Overview & Purpose

The Voice Light project is an advanced, dual-mode smart classroom automation system powered by an ESP32-S3 (N8R8) microcontroller. The primary purpose of this system is to provide reliable, local, and privacy-focused environmental control in a classroom setting without relying on external cloud servers, internet connectivity, or expensive voice assistant hardware.

Usefulness

* Complete Offline Control: Operates entirely locally using the built-in ESP-SR MultiNet 7 English speech recognition engine, meaning zero latency and complete data privacy.

* Dual Control Architecture: Provides seamless convenience by allowing teachers or users to manage classroom infrastructure either via spoken voice commands or through a responsive, real-time web dashboard hosted directly on the device.

* Robust Hardware Automation: Drives multiple independent relay channels to control heavy classroom utilities like smart projection screens/TVs, stage lights, and primary room lighting circuits simultaneously or through dedicated preset modes (e.g., Presentation Mode or Class Start).

---

System Requirements

Hardware Requirements

* Microcontroller: ESP32-S3 N8R8 (8MB Flash, OPI PSRAM)

* Microphone: INMP441 I2S Digital Microphone

* Relay Module: 4-Channel Relay Board (configured for Active-Low or Active-High triggering)

* Power Supply: Stable 5V DC power source for the ESP32-S3 and relays

* Client Device: Any smartphone, tablet, or laptop with Wi-Fi capabilities to access the web control panel

Software & Library Requirements

* IDE: Arduino IDE (configured for ESP32-S3 development)
* ESP32 Board Package: `esp32` by Espressif Systems (v3.x / ESP-IDF supported)
* Required Arduino Libraries:
	`WiFi` (Built-in)
	`AsyncTCP`
	`ESPAsyncWebServer`

* ESP-SR Framework: Ensure your Arduino environment or partition scheme supports ESP-SR components (specifically `esp_mn_iface.h`, `esp_mn_models.h`, and `model_path.h`).

---

Flashing the Model & Firmware

	Flashing a voice-controlled ESP32-S3 project requires uploading both the application binary code and the speech recognition model partition (`model`) to the flash memory.

1. Partition Scheme Configuration:
	* In your Arduino IDE, select your ESP32-S3 board (e.g., *ESP32S3 Dev Module*).
	* Ensure you select a custom partition scheme that allocates sufficient space for large neural network models (such as an 8M Flash / OPI PSRAM configuration).

2. Model Data Flash:
	* Compile and flash the speech recognition model partition (`srmodels`) using ESP-IDF tools or compatible Arduino plugin data uploaders so the MultiNet 7 binaries are stored in flash memory.


3. Uploading the Firmware:
	* Connect your ESP32-S3 to your computer via USB.
	* Open the firmware sketch (`esp-voice.ino`).
	* Select the correct COM port and hit **Upload**.

---

Algorithm & Architecture

	The system utilizes a Dual-Core Processing and Hybrid Control Algorithm to ensure that audio processing never drops commands while handling web requests:

* Core 0 (Dedicated Voice DSP Pipeline):
	* Continuously captures raw audio frames from the INMP441 microphone over the I2S DMA interface.

	* Passes audio through a high-pass DC-blocker filter ($y[n] = x[n] - x[n-1] + 0.995 \cdot y[n-1]$) and a dynamic digital gain amplifier to eliminate background noise and DC offset.

	* Feeds normalized PCM chunks into the **MultiNet 7** neural network speech engine to match predefined phonetic classroom commands.

* Core 1 (Networking, Web Server & WebSockets):
	* Hosts a local Wi-Fi SoftAP access point (`ズクト-Project-Voice-Light` / `voicelight@benu`).

	* Runs an asynchronous web server and WebSocket manager (`/ws`) that serves a responsive mobile-first HTML/CSS dashboard.

	* Instantly syncs state changes bidirectionally: whether a user speaks a command or taps a button on their phone, the internal `StateManager` updates all relays and broadcasts JSON state packets instantly.

---

Advantages

* No Cloud Dependency: Operates completely offline, making it secure, immune to internet outages, and free from subscription or cloud server fees.

* Dual-Mode Redundancy: Provides two distinct ways to control the classroom (hands-free voice commands or a visual web UI), ensuring maximum usability.

* Optimized Dual-Core Execution: Heavy neural network speech parsing runs strictly on Core 0, preventing Wi-Fi packet drops, server lags, or interface lockups on Core 1.


* Real-Time UI Synchronization: WebSocket implementation ensures that state updates across multiple connected web clients and voice triggers happen in real time without manual page refreshes.
