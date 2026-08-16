// =======================================================
// Voice Light - Dual Voice & Web Controlled Smart Classroom
// Hardware: ESP32-S3 N8R8 (8MB Flash, OPI PSRAM)
// Audio:    INMP441 (I2S) + MultiNet 7 Speech Model (Core 0)
// Network:  Wi-Fi SoftAP + WebSockets Real-Time UI (Core 1)
// =======================================================

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "driver/i2s_std.h"
#include <functional>

// ---- ESP-SR MultiNet Headers ----
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"

// =======================================================
// HARDWARE PIN CONFIGURATION
// =======================================================
#define I2S_WS_PIN          15   // LRCLK / WS
#define I2S_SD_PIN          16   // DOUT / SD
#define I2S_SCK_PIN         17   // BCLK / SCK

#define RELAY_TV_PIN        4    // Smart TV / Screen
#define RELAY_STAGE_PIN     5    // Stage Light
#define RELAY_LIGHTS_A_PIN  6    // Classroom Lights Circuit A
#define RELAY_LIGHTS_B_PIN  7    // Classroom Lights Circuit B

#define RELAY_ACTIVE_LOW    true
#define SERIAL_BAUD_RATE    115200

#define LOG(x)       Serial.println(x)
#define LOGF(...)    Serial.printf(__VA_ARGS__)

// =======================================================
// NETWORK & WEB SERVER CONFIG
// =======================================================
const char* AP_SSID = "ズクト-Project-Voice-Light";
const char* AP_PASS = "voicelight@benu";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Forward declarations
void broadcastState();

// =======================================================
// COMMAND TABLE & ENUM
// =======================================================
enum class Command : int {
    UNKNOWN            = -1,
    CLASS_START        = 1,
    CLASS_END          = 2,
    PRESENTATION_MODE  = 3,
    TV_ON              = 4,
    TV_OFF             = 5,
    STAGE_LIGHT_ON     = 6,
    STAGE_LIGHT_OFF    = 7,
    LIGHTS_ON          = 8,
    LIGHTS_OFF         = 9
};

struct CommandPhrase {
    Command id;
    const char *phrase;
};

// Cleaned phonetic phrases (lowercase, no punctuation)
static const CommandPhrase COMMAND_TABLE[] = {
    // 1. Class Start
    { Command::CLASS_START,       "Hey zuku, class start" },
    { Command::CLASS_START,       "zuku, class start" },

    // 2. Class End / Power Off
    { Command::CLASS_END,         "Hey zuku, power off" },
    { Command::CLASS_END,         "zuku, power off" },

    // 3. Presentation Mode
    { Command::PRESENTATION_MODE, "Hey zuku, presentation mode" },
    { Command::PRESENTATION_MODE, "zuku, presentation mode" },

    // 4. TV / Screen ON
    { Command::TV_ON,             "Hey zuku, screen activate" },
    { Command::TV_ON,             "zuku, screen activate" },

    // 5. TV OFF
    { Command::TV_OFF,            "Hey zuku, tv off" },
    { Command::TV_OFF,            "zuku, tv off" },

    // 6. Stage Light ON
    { Command::STAGE_LIGHT_ON,    "Hey zuku, stage on" },
    { Command::STAGE_LIGHT_ON,    "zuku, stage on" },

    // 7. Stage Light OFF
    { Command::STAGE_LIGHT_OFF,   "Hey zuku, stage off" },
    { Command::STAGE_LIGHT_OFF,   "zuku, stage off" },

    // 8. Classroom Lights ON
    { Command::LIGHTS_ON,         "Hey zuku, lights on" },
    { Command::LIGHTS_ON,         "zuku, lights on" },

    // 9. Classroom Lights OFF
    { Command::LIGHTS_OFF,        "Hey zuku, lights off" },
    { Command::LIGHTS_OFF,        "zuku, lights off" },
};

const int TOTAL_COMMAND_COUNT = sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]);

// =======================================================
// HTML & RESPONSIVE DASHBOARD UI
// =======================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Zuku Smart Classroom</title>
  <style>
    * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .container { max-width: 440px; width: 100%; }
    .header { text-align: center; margin-bottom: 24px; }
    .header h1 { margin: 0; font-size: 26px; color: #38bdf8; }
    .header p { margin: 4px 0 0 0; color: #94a3b8; font-size: 14px; }
    .card { background: #1e293b; border-radius: 16px; padding: 20px; margin-bottom: 16px; box-shadow: 0 4px 12px rgba(0,0,0,0.3); border: 1px solid #334155; }
    .card-title { font-size: 15px; font-weight: 600; color: #cbd5e1; margin-bottom: 12px; text-transform: uppercase; letter-spacing: 0.5px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    button { padding: 14px; font-size: 15px; font-weight: 600; border: none; border-radius: 10px; cursor: pointer; transition: all 0.2s ease; display: flex; justify-content: center; align-items: center; color: white; }
    button:active { transform: scale(0.97); }
    .btn-master-on { background: #10b981; }
    .btn-master-off { background: #ef4444; }
    .btn-preset { background: #8b5cf6; grid-column: span 2; }
    .btn-toggle { background: #334155; }
    .btn-toggle.active { background: #0284c7; box-shadow: 0 0 12px rgba(2, 132, 199, 0.5); }
    .status-bar { margin-top: 10px; font-size: 13px; color: #64748b; text-align: center; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>ズクト ZUKU</h1>
      <p>Smart Classroom Automation</p>
    </div>

    <div class="card">
      <div class="card-title">Quick Actions</div>
      <div class="grid">
        <button class="btn-master-on" onclick="sendCmd(1)">⚡ Class Start</button>
        <button class="btn-master-off" onclick="sendCmd(2)">⏻ Power Off</button>
        <button class="btn-preset" onclick="sendCmd(3)">📽 Presentation Mode</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">Device Controls</div>
      <div class="grid">
        <button id="btn-tv" class="btn-toggle" onclick="toggleDevice('tv')">📺 Smart TV</button>
        <button id="btn-stage" class="btn-toggle" onclick="toggleDevice('stage')">🎭 Stage Light</button>
        <button id="btn-lights" class="btn-toggle" style="grid-column: span 2;" onclick="toggleDevice('lights')">💡 Classroom Lights</button>
      </div>
    </div>

    <div class="status-bar" id="ws-status">Connecting to ESP32...</div>
  </div>

  <script>
    let ws;
    let state = { tv: false, stage: false, lights: false };

    function initWebSocket() {
      ws = new WebSocket(`ws://${window.location.hostname}/ws`);
      ws.onopen = () => { document.getElementById('ws-status').innerText = '● Connected in Real-Time'; document.getElementById('ws-status').style.color = '#10b981'; };
      ws.onclose = () => { document.getElementById('ws-status').innerText = '○ Reconnecting...'; document.getElementById('ws-status').style.color = '#ef4444'; setTimeout(initWebSocket, 2000); };
      ws.onmessage = (e) => {
        try {
          state = JSON.parse(e.data);
          updateUI();
        } catch(err) {}
      };
    }

    function updateUI() {
      document.getElementById('btn-tv').className = state.tv ? 'btn-toggle active' : 'btn-toggle';
      document.getElementById('btn-stage').className = state.stage ? 'btn-toggle active' : 'btn-toggle';
      document.getElementById('btn-lights').className = state.lights ? 'btn-toggle active' : 'btn-toggle';
    }

    function sendCmd(id) { ws.send(id.toString()); }

    function toggleDevice(device) {
      if (device === 'tv') sendCmd(state.tv ? 5 : 4);
      if (device === 'stage') sendCmd(state.stage ? 7 : 6);
      if (device === 'lights') sendCmd(state.lights ? 9 : 8);
    }

    window.onload = initWebSocket;
  </script>
</body>
</html>
)rawliteral";

// =======================================================
// STATE & HARDWARE CONTROLLER
// =======================================================
class StateManager {
public:
    void begin() { tvOn = false; stageLightOn = false; classroomLightsOn = false; }
    void setTv(bool on)              { tvOn = on; }
    void setStageLight(bool on)      { stageLightOn = on; }
    void setClassroomLights(bool on) { classroomLightsOn = on; }

    bool getTv() const              { return tvOn; }
    bool getStageLight() const      { return stageLightOn; }
    bool getClassroomLights() const { return classroomLightsOn; }

    void printState() const {
        LOGF("[STATE] TV=%s | STAGE=%s | LIGHTS=%s\n",
             tvOn ? "ON" : "OFF",
             stageLightOn ? "ON" : "OFF",
             classroomLightsOn ? "ON" : "OFF");
    }
private:
    bool tvOn = false;
    bool stageLightOn = false;
    bool classroomLightsOn = false;
};

class RelayController {
public:
    void begin() {
        pinMode(RELAY_TV_PIN, OUTPUT);
        pinMode(RELAY_STAGE_PIN, OUTPUT);
        pinMode(RELAY_LIGHTS_A_PIN, OUTPUT);
        pinMode(RELAY_LIGHTS_B_PIN, OUTPUT);
        allOff();
        LOG("[RELAY] Relays initialized (All OFF)");
    }

    void setTv(bool on) {
        writeRelay(RELAY_TV_PIN, on);
        LOGF("[RELAY] TV -> %s (GPIO %d)\n", on ? "ON" : "OFF", RELAY_TV_PIN);
    }

    void setStageLight(bool on) {
        writeRelay(RELAY_STAGE_PIN, on);
        LOGF("[RELAY] Stage Light -> %s (GPIO %d)\n", on ? "ON" : "OFF", RELAY_STAGE_PIN);
    }

    void setClassroomLights(bool on) {
        writeRelay(RELAY_LIGHTS_A_PIN, on);
        writeRelay(RELAY_LIGHTS_B_PIN, on);
        LOGF("[RELAY] Classroom Lights -> %s (GPIO %d & %d)\n", on ? "ON" : "OFF", RELAY_LIGHTS_A_PIN, RELAY_LIGHTS_B_PIN);
    }

    void allOff() {
        setTv(false);
        setStageLight(false);
        setClassroomLights(false);
    }

private:
    void writeRelay(int pin, bool on) const {
        bool level = RELAY_ACTIVE_LOW ? !on : on;
        digitalWrite(pin, level ? HIGH : LOW);
    }
};

StateManager stateManager;
RelayController relayController;

// Broadcast relay state JSON to all connected web clients
void broadcastState() {
    String json = "{";
    json += "\"tv\":" + String(stateManager.getTv() ? "true" : "false") + ",";
    json += "\"stage\":" + String(stateManager.getStageLight() ? "true" : "false") + ",";
    json += "\"lights\":" + String(stateManager.getClassroomLights() ? "true" : "false");
    json += "}";
    ws.textAll(json);
}

class CommandParser {
public:
    CommandParser(StateManager &stateMgr, RelayController &relayCtrl)
        : state(stateMgr), relay(relayCtrl) {}

    bool execute(Command cmd) {
        switch (cmd) {
            case Command::CLASS_START:
                LOG("\n[EXEC] Action: CLASS START -> ALL ON");
                applyTv(true); applyStageLight(true); applyClassroomLights(true);
                break;

            case Command::CLASS_END:
                LOG("\n[EXEC] Action: POWER OFF -> ALL OFF");
                applyTv(false); applyStageLight(false); applyClassroomLights(false);
                break;

            case Command::PRESENTATION_MODE:
                LOG("\n[EXEC] Action: PRESENTATION MODE -> TV & Lights ON, Stage OFF");
                applyTv(true); applyStageLight(false); applyClassroomLights(true);
                break;

            case Command::TV_ON:
                LOG("\n[EXEC] Action: TV ON");
                applyTv(true);
                break;

            case Command::TV_OFF:
                LOG("\n[EXEC] Action: TV OFF");
                applyTv(false);
                break;

            case Command::STAGE_LIGHT_ON:
                LOG("\n[EXEC] Action: STAGE ON");
                applyStageLight(true);
                break;

            case Command::STAGE_LIGHT_OFF:
                LOG("\n[EXEC] Action: STAGE OFF");
                applyStageLight(false);
                break;

            case Command::LIGHTS_ON:
                LOG("\n[EXEC] Action: LIGHTS ON");
                applyClassroomLights(true);
                break;

            case Command::LIGHTS_OFF:
                LOG("\n[EXEC] Action: LIGHTS OFF");
                applyClassroomLights(false);
                break;

            default:
                return false;
        }
        state.printState();
        broadcastState(); // Sync all mobile web dashboards
        return true;
    }

private:
    StateManager &state;
    RelayController &relay;

    void applyTv(bool on)              { state.setTv(on); relay.setTv(on); }
    void applyStageLight(bool on)      { state.setStageLight(on); relay.setStageLight(on); }
    void applyClassroomLights(bool on) { state.setClassroomLights(on); relay.setClassroomLights(on); }
};

CommandParser commandParser(stateManager, relayController);

void handleCommand(Command cmd) {
    commandParser.execute(cmd);
}

// WebSocket Event Handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        broadcastState(); // Send current state on connect
    } else if (type == WS_EVT_DATA) {
        String msg = "";
        for (size_t i = 0; i < len; i++) msg += (char)data[i];
        int cmdId = msg.toInt();
        if (cmdId >= 1 && cmdId <= 9) {
            handleCommand((Command)cmdId);
        }
    }
}

// =======================================================
// VOICE MANAGER (Continuous Speech Engine Task - Core 0)
// =======================================================
class VoiceManager {
public:
    using CommandCallback = std::function<void(Command)>;

    bool begin() {
        if (!initI2S()) return false;
        if (!initEspSr()) return false;
        
        xTaskCreatePinnedToCore(voiceTaskEntry, "VoiceTask", 8192, this, 5, NULL, 0);
        return true;
    }

    void onCommand(CommandCallback callback) {
        commandCallback = callback;
    }

private:
    CommandCallback commandCallback = nullptr;
    i2s_chan_handle_t rx_handle = NULL;
    esp_mn_iface_t *multinet = nullptr;
    model_iface_data_t *modelData = nullptr;
    srmodel_list_t *models = nullptr;

    int32_t *i2sRawBuffer = nullptr;
    int16_t *pcmBuffer = nullptr;
    int chunkSamples = 0;

    float dc_prev_in = 0.0f;
    float dc_prev_out = 0.0f;

    bool initI2S() {
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        chan_cfg.dma_desc_num = 6;
        chan_cfg.dma_frame_num = 512;
        
        if (i2s_new_channel(&chan_cfg, NULL, &rx_handle) != ESP_OK) return false;

        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = (gpio_num_t)I2S_SCK_PIN,
                .ws   = (gpio_num_t)I2S_WS_PIN,
                .dout = I2S_GPIO_UNUSED,
                .din  = (gpio_num_t)I2S_SD_PIN,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv   = false,
                },
            },
        };
        std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

        if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) return false;
        if (i2s_channel_enable(rx_handle) != ESP_OK) return false;

        LOG("[VOICE] Native S3 I2S driver initialized");
        return true;
    }

    bool initEspSr() {
        models = esp_srmodel_init("model");
        if (!models) {
            LOG("[VOICE] Error: Model partition not found!");
            return false;
        }

        char *mnName = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
        if (!mnName) {
            LOG("[VOICE] Error: No English MultiNet model found in flash");
            return false;
        }

        multinet = esp_mn_handle_from_name(mnName);
        modelData = multinet->create(mnName, 4000);

        chunkSamples = multinet->get_samp_chunksize(modelData);
        i2sRawBuffer = (int32_t *)malloc(chunkSamples * sizeof(int32_t));
        pcmBuffer    = (int16_t *)malloc(chunkSamples * sizeof(int16_t));

        if (!i2sRawBuffer || !pcmBuffer) {
            LOG("[VOICE] Error: Buffer memory allocation failed");
            return false;
        }

        esp_mn_commands_alloc(multinet, modelData);
        esp_mn_commands_clear();
        for (int i = 0; i < TOTAL_COMMAND_COUNT; i++) {
            esp_mn_commands_add((int)COMMAND_TABLE[i].id, (char *)COMMAND_TABLE[i].phrase);
        }
        esp_mn_commands_update();

        LOGF("[VOICE] MultiNet Ready: %s (Chunk size: %d samples)\n", mnName, chunkSamples);
        return true;
    }

    static void voiceTaskEntry(void *param) {
        VoiceManager *self = static_cast<VoiceManager *>(param);
        self->run();
    }

    void run() {
        size_t bytesRead = 0;
        LOG("[VOICE] Listening task active on Core 0. Ready for commands...\n");

        while (true) {
            i2s_channel_read(rx_handle, i2sRawBuffer, chunkSamples * sizeof(int32_t), &bytesRead, portMAX_DELAY);
            size_t samplesRead = bytesRead / sizeof(int32_t);

            for (size_t i = 0; i < samplesRead; i++) {
                float in = (float)(i2sRawBuffer[i] >> 14);
                float out = in - dc_prev_in + 0.995f * dc_prev_out;
                dc_prev_in = in;
                dc_prev_out = out;

                float amplified = out * 2.5f;
                if (amplified > 32767.0f)  amplified = 32767.0f;
                if (amplified < -32768.0f) amplified = -32768.0f;

                pcmBuffer[i] = (int16_t)amplified;
            }
            for (size_t i = samplesRead; i < (size_t)chunkSamples; i++) {
                pcmBuffer[i] = 0;
            }

            esp_mn_state_t state = multinet->detect(modelData, pcmBuffer);

            if (state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *result = multinet->get_results(modelData);
                if (result && result->num > 0) {
                    Command cmd = (Command)result->command_id[0];
                    LOGF("\n>>> [VOICE DETECTED] ID: %d | Prob: %.2f <<<\n", result->command_id[0], result->prob[0]);
                    if (commandCallback) commandCallback(cmd);
                }
                multinet->clean(modelData);
            } 
            else if (state == ESP_MN_STATE_TIMEOUT) {
                multinet->clean(modelData);
            }
        }
    }
};

VoiceManager voiceManager;

// =======================================================
// SYSTEM SETUP
// =======================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);
    LOG("\n=================================================");
    LOG("   Zuku Smart Classroom: Voice + Web System");
    LOG("=================================================");

    relayController.begin();
    stateManager.begin();
    voiceManager.onCommand(handleCommand);

    // 1. Start Wi-Fi SoftAP Hotspot
    WiFi.softAP(AP_SSID, AP_PASS);
    LOGF("[WIFI] Access Point '%s' Started\n", AP_SSID);
    LOGF("[WIFI] Control Dashboard URL: http://%s\n", WiFi.softAPIP().toString().c_str());

    // 2. Start Web Server & WebSockets
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", INDEX_HTML);
    });
    server.begin();
    LOG("[WEB] Web Server & WebSocket active");

    // 3. Start Voice Engine on Core 0
    if (!voiceManager.begin()) {
        LOG("[SYSTEM] Critical Error: MultiNet setup failed.");
        while (true) delay(1000);
    }

    LOG("=== All Systems Online ===");
}

void loop() {
    ws.cleanupClients();
    vTaskDelay(pdMS_TO_TICKS(100));
}
