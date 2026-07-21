// =======================================================
// Voice Light
// Offline voice-controlled smart classroom automation
// Board: ESP32-S3 N8R8
// Mic:   INMP441 (I2S)
//
// REQUIRED ARDUINO IDE SETTINGS (Tools menu):
//   Board:            ESP32S3 Dev Module
//   PSRAM:             OPI PSRAM
//   Flash Size:        8MB
//   Partition Scheme:  Any scheme that includes a "model" data
//                       partition big enough for ESP-SR (e.g.
//                       "16M Flash (3MB APP/9.9MB FATFS)" style,
//                       or a custom partitions.csv - see notes
//                       at the bottom of this file).
//
// REQUIRED ONE-TIME STEP: the speech models must be uploaded to
// the "model" partition BEFORE this sketch will recognize
// anything. See "MODEL DATA UPLOAD" section at the bottom.
// =======================================================

#include <Arduino.h>
#include <driver/i2s.h>
#include <functional>

// ---- ESP-SR headers (provided by arduino-esp32 core >= 2.0.14 /
// 3.x, installed automatically with the ESP32 board package) ----
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"

// =======================================================
// CONFIG
// =======================================================

// ---- I2S Microphone (INMP441) ----
#define I2S_WS_PIN      15   // Word Select (LRCLK)
#define I2S_SD_PIN      16   // Serial Data (DOUT from mic)
#define I2S_SCK_PIN     17   // Serial Clock (BCLK)
#define I2S_PORT        I2S_NUM_0

// ---- Relay Outputs ----
// Relay 1 -> Smart TV | Relay 2 -> Stage Light
// Relay 3 -> Classroom Light Circuit A | Relay 4 -> Circuit B
// Relay 3 and Relay 4 must ALWAYS switch together.
#define RELAY_TV_PIN        4
#define RELAY_STAGE_PIN     5
#define RELAY_LIGHTS_A_PIN  6
#define RELAY_LIGHTS_B_PIN  7

// true = active-LOW relay module (most common low-cost boards)
#define RELAY_ACTIVE_LOW    true

// ---- Mic gain shift ----
// INMP441 outputs 24-bit samples left-justified in a 32-bit I2S
// frame. Shifting right by 14 gives a usable 16-bit PCM range for
// MultiNet. If detection is too insensitive/too noisy, try 11-16.
#define MIC_SHIFT_BITS   14

// ---- System Constants ----
#define TOTAL_COMMAND_COUNT     9
#define SERIAL_BAUD_RATE        115200

#define DEBUG_ENABLED true
#if DEBUG_ENABLED
  #define LOG(x)       Serial.println(x)
  #define LOGF(...)    Serial.printf(__VA_ARGS__)
#else
  #define LOG(x)
  #define LOGF(...)
#endif

// =======================================================
// COMMAND ENUM
// IDs below (1-9) are what we register with esp_mn_commands_add()
// and must match esp_mn_results_t.command_id at recognition time.
// =======================================================
enum class Command : int {
    UNKNOWN            = -1,

    CLASS_START        = 1,
    CLASS_END          = 2,
    PRESENTATION_MODE  = 3,

    TV_ON               = 4,
    TV_OFF              = 5,
    STAGE_LIGHT_ON      = 6,
    STAGE_LIGHT_OFF     = 7,
    LIGHTS_ON           = 8,
    LIGHTS_OFF          = 9
};

// Phrase text registered with MultiNet. Keep phrases short, clear,
// and phonetically distinct - MultiNet matches on English phonemes,
// not exact wording, so simpler phrasing recognizes more reliably.
struct CommandPhrase {
    Command id;
    const char *phrase;
};

static const CommandPhrase COMMAND_TABLE[TOTAL_COMMAND_COUNT] = {
    { Command::CLASS_START,       "class start" },
    { Command::CLASS_END,         "power off" },
    { Command::PRESENTATION_MODE, "presentation mode" },
    { Command::TV_ON,             "screen activate" },
    { Command::TV_OFF,            "tv off" },
    { Command::STAGE_LIGHT_ON,    "stage on" },
    { Command::STAGE_LIGHT_OFF,   "stage off" },
    { Command::LIGHTS_ON,         "lights on" },
    { Command::LIGHTS_OFF,        "lights off" },
};

// =======================================================
// STATE MANAGER
// =======================================================
class StateManager {
public:
    void begin() {
        tvOn = false;
        stageLightOn = false;
        classroomLightsOn = false;
    }

    void setTv(bool on)              { tvOn = on; }
    void setStageLight(bool on)      { stageLightOn = on; }
    void setClassroomLights(bool on) { classroomLightsOn = on; }

    void printState() const {
        LOGF("[STATE] TV=%s STAGE=%s LIGHTS=%s\n",
             tvOn ? "ON" : "OFF",
             stageLightOn ? "ON" : "OFF",
             classroomLightsOn ? "ON" : "OFF");
    }

private:
    bool tvOn = false;
    bool stageLightOn = false;
    bool classroomLightsOn = false;
};

// =======================================================
// RELAY CONTROLLER
// Only class allowed to touch relay GPIO. Enforces Relay 3
// and Relay 4 switching together. Every call forces a known
// state - never toggles blindly.
// =======================================================
class RelayController {
public:
    void begin() {
        pinMode(RELAY_TV_PIN, OUTPUT);
        pinMode(RELAY_STAGE_PIN, OUTPUT);
        pinMode(RELAY_LIGHTS_A_PIN, OUTPUT);
        pinMode(RELAY_LIGHTS_B_PIN, OUTPUT);
        allOff();
        LOG("[RELAY] Initialized, all relays OFF");
    }

    void setTv(bool on) {
        writeRelay(RELAY_TV_PIN, on);
        LOGF("[RELAY] TV -> %s\n", on ? "ON" : "OFF");
    }

    void setStageLight(bool on) {
        writeRelay(RELAY_STAGE_PIN, on);
        LOGF("[RELAY] Stage Light -> %s\n", on ? "ON" : "OFF");
    }

    void setClassroomLights(bool on) {
        writeRelay(RELAY_LIGHTS_A_PIN, on);
        writeRelay(RELAY_LIGHTS_B_PIN, on);
        LOGF("[RELAY] Classroom Lights (Relay3+4) -> %s\n", on ? "ON" : "OFF");
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

// =======================================================
// COMMAND PARSER
// =======================================================
class CommandParser {
public:
    CommandParser(StateManager &stateMgr, RelayController &relayCtrl)
        : state(stateMgr), relay(relayCtrl) {}

    bool execute(Command cmd) {
        switch (cmd) {
            case Command::CLASS_START:
                LOG("[CMD] Class Start");
                applyTv(true); applyStageLight(true); applyClassroomLights(true);
                break;

            case Command::CLASS_END:
                LOG("[CMD] Class End");
                applyTv(false); applyStageLight(false); applyClassroomLights(false);
                break;

            case Command::PRESENTATION_MODE:
                LOG("[CMD] Presentation Mode");
                applyTv(true); applyStageLight(false); applyClassroomLights(true);
                break;

            case Command::TV_ON:
                LOG("[CMD] TV On"); applyTv(true); break;

            case Command::TV_OFF:
                LOG("[CMD] TV Off"); applyTv(false); break;

            case Command::STAGE_LIGHT_ON:
                LOG("[CMD] Stage Light On"); applyStageLight(true); break;

            case Command::STAGE_LIGHT_OFF:
                LOG("[CMD] Stage Light Off"); applyStageLight(false); break;

            case Command::LIGHTS_ON:
                LOG("[CMD] Lights On (classroom only)"); applyClassroomLights(true); break;

            case Command::LIGHTS_OFF:
                LOG("[CMD] Lights Off (classroom only)"); applyClassroomLights(false); break;

            case Command::UNKNOWN:
            default:
                LOG("[CMD] Unknown command - ignored");
                return false;
        }

        state.printState();
        return true;
    }

private:
    StateManager &state;
    RelayController &relay;

    void applyTv(bool on)              { state.setTv(on); relay.setTv(on); }
    void applyStageLight(bool on)      { state.setStageLight(on); relay.setStageLight(on); }
    void applyClassroomLights(bool on) { state.setClassroomLights(on); relay.setClassroomLights(on); }
};

// =======================================================
// VOICE MANAGER
// Real I2S capture + ESP-SR MultiNet recognition (WakeNet
// disabled - MultiNet listens continuously per project spec).
// =======================================================
class VoiceManager {
public:
    using CommandCallback = std::function<void(Command)>;

    bool begin() {
        if (!initI2S()) return false;
        if (!initEspSr()) return false;
        initialized = true;
        LOG("[VOICE] Voice manager ready - listening continuously");
        return true;
    }

    void onCommand(CommandCallback callback) {
        commandCallback = callback;
    }

    // Non-blocking-ish: reads exactly one MultiNet chunk per call.
    // i2s_read() below uses a short timeout so loop() never stalls
    // for long; call this every iteration of loop().
    void update() {
        if (!initialized) return;

        size_t bytesRead = 0;
        i2s_read(I2S_PORT, i2sRawBuffer, chunkSamples * sizeof(int32_t),
                  &bytesRead, pdMS_TO_TICKS(20));

        size_t samplesRead = bytesRead / sizeof(int32_t);
        if (samplesRead == 0) return; // no data this cycle, try again next loop()

        for (size_t i = 0; i < samplesRead; i++) {
            pcmBuffer[i] = (int16_t)(i2sRawBuffer[i] >> MIC_SHIFT_BITS);
        }
        // Pad if we got a short read (keeps MultiNet chunk size exact)
        for (size_t i = samplesRead; i < chunkSamples; i++) {
            pcmBuffer[i] = 0;
        }

        esp_mn_state_t state = multinet->detect(modelData, pcmBuffer);

        if (state == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *result = multinet->get_results(modelData);
            if (result->num > 0) {
                Command cmd = mapModelIdToCommand(result->command_id[0]);
                LOGF("[VOICE] Detected command_id=%d (prob=%.2f)\n",
                     result->command_id[0], result->prob[0]);
                if (commandCallback) commandCallback(cmd);
            }
        }
        // ESP_MN_STATE_TIMEOUT / ESP_MN_STATE_DETECTING -> keep listening
    }

private:
    CommandCallback commandCallback = nullptr;
    bool initialized = false;

    esp_mn_iface_t *multinet = nullptr;
    model_iface_data_t *modelData = nullptr;
    srmodel_list_t *models = nullptr;

    int32_t *i2sRawBuffer = nullptr;
    int16_t *pcmBuffer = nullptr;
    int chunkSamples = 0;

    bool initI2S() {
        i2s_config_t i2sConfig = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 16000,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = 256,
            .use_apll = false
        };

        i2s_pin_config_t pinConfig = {
            .bck_io_num = I2S_SCK_PIN,
            .ws_io_num = I2S_WS_PIN,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = I2S_SD_PIN
        };

        if (i2s_driver_install(I2S_PORT, &i2sConfig, 0, NULL) != ESP_OK) {
            LOG("[VOICE] I2S driver install failed");
            return false;
        }
        if (i2s_set_pin(I2S_PORT, &pinConfig) != ESP_OK) {
            LOG("[VOICE] I2S pin config failed");
            return false;
        }

        LOG("[VOICE] I2S (INMP441) initialized");
        return true;
    }

    bool initEspSr() {
        models = esp_srmodel_init("model");
        if (models == nullptr) {
            LOG("[VOICE] esp_srmodel_init failed - model partition missing/empty. "
                "Did you upload the model data? See MODEL DATA UPLOAD notes.");
            return false;
        }

        char *mnName = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
        if (mnName == nullptr) {
            LOG("[VOICE] No English MultiNet model found in model partition");
            return false;
        }

        multinet = esp_mn_handle_from_name(mnName);
        modelData = multinet->create(mnName, 6000); // 6000ms max command duration

        chunkSamples = multinet->get_samp_chunksize(modelData);
        i2sRawBuffer = (int32_t *)malloc(chunkSamples * sizeof(int32_t));
        pcmBuffer     = (int16_t *)malloc(chunkSamples * sizeof(int16_t));
        if (i2sRawBuffer == nullptr || pcmBuffer == nullptr) {
            LOG("[VOICE] Failed to allocate audio buffers");
            return false;
        }

        esp_mn_commands_alloc(multinet, modelData);
        esp_mn_commands_clear();
        for (int i = 0; i < TOTAL_COMMAND_COUNT; i++) {
            esp_mn_commands_add((int)COMMAND_TABLE[i].id, (char *)COMMAND_TABLE[i].phrase);
        }
        esp_mn_commands_update();

        LOGF("[VOICE] ESP-SR MultiNet ready: %s (chunk=%d samples)\n",
             mnName, chunkSamples);
        return true;
    }

    Command mapModelIdToCommand(int modelCommandId) const {
        for (int i = 0; i < TOTAL_COMMAND_COUNT; i++) {
            if ((int)COMMAND_TABLE[i].id == modelCommandId) {
                return COMMAND_TABLE[i].id;
            }
        }
        return Command::UNKNOWN;
    }
};

// =======================================================
// GLOBAL INSTANCES
// =======================================================
StateManager stateManager;
RelayController relayController;
CommandParser commandParser(stateManager, relayController);
VoiceManager voiceManager;

void handleCommand(Command cmd) {
    if (!commandParser.execute(cmd)) {
        LOG("[MAIN] Command ignored (unsupported/unknown)");
    }
}

// =======================================================
// SETUP - Boot Sequence
// GPIO -> I2S/Mic -> ESP-SR -> Relays -> all OFF -> listen
// =======================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(200); // one-time settle for serial monitor only
    LOG("=== Voice Light Booting ===");

    relayController.begin();
    stateManager.begin();
    voiceManager.onCommand(handleCommand);

    if (!voiceManager.begin()) {
        LOG("[MAIN] Voice manager init failed - retrying...");
        while (!voiceManager.begin()) {
            LOG("[MAIN] Retry mic/ESP-SR init...");
            delay(500); // acceptable: one-time boot recovery only
        }
    }

    LOG("=== Voice Light Ready - Listening ===");
}

// =======================================================
// LOOP
// =======================================================
void loop() {
    voiceManager.update();
}

// =======================================================
// MODEL DATA UPLOAD (one-time, per board flash) - READ THIS
// =======================================================
// ESP-SR recognizes nothing until its speech model files exist on
// a flash partition named "model". This is separate from uploading
// this sketch, and it's the #1 reason people's ESP-SR code "does
// nothing" even when the code is correct.
//
// 1. Locate the model files. They ship inside the ESP32 Arduino
//    core install, under:
//      <arduino-esp32 core>/tools/esp32-arduino-libs/esp32s3/
//        (or similar) .../esp-sr/model/  (wakenet + multinet .bin files)
//    Exact path varies by core version - search your core install
//    folder for files like "mn5_en.bin" / "wn9_*.bin".
//
// 2. Create a "data" folder next to this .ino file, and copy in
//    only the MultiNet English model files you need (English
//    MultiNet, no WakeNet models needed since wake word is
//    disabled per project spec). Keep total size within your
//    chosen partition's capacity.
//
// 3. Pick (or create) a partition scheme with a partition literally
//    named "model" that's large enough (models are typically a few
//    MB). Arduino IDE > Tools > Partition Scheme. If no built-in
//    scheme fits, create a custom partitions.csv with a line like:
//      model, data, spiffs, , 4M
//
// 4. Flash the model data with "ESP32 Sketch Data Upload" (Arduino
//    IDE menu, or via the equivalent tool for IDE 2.x - search
//    "ESP32 Sketch Data Upload" for your IDE version if you don't
//    see it, it may need installing as a separate plugin).
//
// 5. THEN upload this .ino normally. On boot, esp_srmodel_init("model")
//    should now find the models. If Serial still logs "model
//    partition missing/empty", the data upload step didn't target
//    the right partition - recheck step 3/4.
//
// Recognition accuracy notes:
//   - MultiNet matches English phonemes, not exact spelling - the
//     phrases in COMMAND_TABLE are tuned to be short and distinct.
//     If two commands get confused (e.g. "TV on" vs "TV off"),
//     try more phonetically distinct phrasing.
//   - Keep the mic 0.5-1m from the speaker's mouth, avoid HVAC/fan
//     noise directly at the capsule, and confirm INMP441 L/R pin is
//     tied to GND (left channel) to match I2S_CHANNEL_FMT_ONLY_LEFT.
// =======================================================
