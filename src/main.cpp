// ============================================================
// FULL PACKAGE: SD card file browser + Nokia 5110 PCD8544 display + buttons
// To compile: change platformio.ini build_src_filter to +<main.cpp>
// Remove board_build.embed_files from platformio.ini (uses SD, not flash)
// ============================================================
//
// Wiring (SPI bus shared on GPIO18/19):
//   LASER PDM  → GPIO0  → 220Ω → MOSFET gate (10kΩ gate-to-GND pulldown)
//   Display SCE → GPIO22
//   Display RST → GPIO21
//   Display DC  → GPIO23 (D5)
//   Display DN  → GPIO18  (shared SPI MOSI)
//   Display SCLK→ GPIO19  (shared SPI SCK)
//   SD CS       → GPIO16 (D6)
//   SD MISO     → GPIO20 (D9)
//   SD MOSI     → GPIO18  (shared SPI MOSI)
//   SD SCLK     → GPIO19  (shared SPI SCK)
//   BTN_PLAY    → GPIO1  (D1, left side)  + other leg to GND
//   BTN_NEXT    → GPIO17 (D7, right side) + other leg to GND
//   LED_PWR     → GPIO5 → 220Ω → LED → GND
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "driver/i2s_pdm.h"

SdFat32 sd;

#define LASER_PIN   0

#define TFT_CS      22   // SCE
#define TFT_RST     21   // RST
#define TFT_DC      23   // D5/GPIO23 — DC
#define TFT_MOSI    18   // shared with SD
#define TFT_SCK     19   // D8 — shared with SD

#define SD_CS       16   // D6/GPIO16 — CS
#define SD_MISO     20   // D9/GPIO20 — MISO

#define BTN_PLAY     1   // D1/GPIO1  — left side of board
#define BTN_NEXT    17   // D7/GPIO17 — right side of board
// LED_PWR: wire directly to 3.3V through 220Ω — no GPIO available

#define SAMPLE_RATE 16000
#define MAX_FILES   20

Adafruit_PCD8544 display = Adafruit_PCD8544(TFT_DC, TFT_CS, TFT_RST);

String   wavFiles[MAX_FILES];
int      numFiles    = 0;
int      currentFile = 0;
bool     playing     = false;

int16_t *audioS16 = nullptr;
size_t   audioLen = 0;
size_t   playPos  = 0;

i2s_chan_handle_t tx_handle = nullptr;
bool i2sReady = false;

// ---- Audio ----------------------------------------------------------------

void freeAudio() {
    free(audioS16);
    audioS16 = nullptr;
    audioLen = 0;
    playPos  = 0;
}

bool loadWavFromSD(const String &filename) {
    freeAudio();
    String path = "/" + filename;
    File32 wav = sd.open(path.c_str(), O_RDONLY);
    if (!wav) { Serial.println("Cannot open: " + filename); return false; }

    size_t wavSize = wav.size();
    if (wavSize <= 44) { wav.close(); return false; }

    audioLen = wavSize - 44;
    uint8_t *raw = (uint8_t *)malloc(audioLen);
    audioS16     = (int16_t *)malloc(audioLen * sizeof(int16_t));

    if (!raw || !audioS16) {
        free(raw); freeAudio(); wav.close();
        Serial.println("Not enough RAM"); return false;
    }

    wav.seek(44);
    wav.read(raw, audioLen);
    wav.close();

    for (size_t i = 0; i < audioLen; i++)
        audioS16[i] = ((int16_t)raw[i] - 128) << 8;

    free(raw);
    Serial.printf("Loaded %s — %d samples (%.1fs)\n",
                  filename.c_str(), (int)audioLen, audioLen / (float)SAMPLE_RATE);
    return true;
}

// ---- SD scan --------------------------------------------------------------

void scanWavFiles() {
    numFiles = 0;
    File32 root = sd.open("/");
    if (!root) { Serial.println("SD root open failed"); return; }

    File32 entry;
    while (numFiles < MAX_FILES && entry.openNext(&root, O_RDONLY)) {
        if (!entry.isDirectory()) {
            char buf[32];
            entry.getName(buf, sizeof(buf));
            String name = String(buf);
            // Skip macOS metadata files (._filename) created on FAT SD cards
            if (!name.startsWith("._")) {
                String upper = name; upper.toUpperCase();
                if (upper.endsWith(".WAV")) wavFiles[numFiles++] = name;
            }
        }
        entry.close();
    }
    root.close();
    Serial.printf("Found %d WAV file(s)\n", numFiles);
}

// ---- Display (84x48 monochrome) ------------------------------------------
// y=0  : "LASEC" title
// y=9  : separator
// y=11 : file row 0  (9px per row)
// y=20 : file row 1
// y=29 : file row 2
// y=38 : separator
// y=40 : status "PLY/STP  1/5"

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK);

    display.setCursor(27, 0);
    display.print("LASEC");
    display.drawFastHLine(0, 9, 84, BLACK);

    if (numFiles == 0) {
        display.setCursor(0, 20);
        display.print("No WAV on SD");
        display.display();
        return;
    }

    int start = currentFile - 1;
    if (start < 0) start = 0;
    if (start + 3 > numFiles) start = max(0, numFiles - 3);

    for (int i = 0; i < 3 && (start + i) < numFiles; i++) {
        int idx = start + i;
        int y   = 11 + i * 9;

        if (idx == currentFile) {
            display.fillRect(0, y - 1, 84, 9, BLACK);
            display.setTextColor(WHITE);
        } else {
            display.setTextColor(BLACK);
        }

        display.setCursor(0, y);
        display.print(idx == currentFile ? ">" : " ");

        String name = wavFiles[idx];
        if (name.length() > 12) name = name.substring(0, 12);
        display.print(name);
    }

    display.setTextColor(BLACK);
    display.drawFastHLine(0, 38, 84, BLACK);
    display.setCursor(0, 40);
    display.print(playing ? "PLY" : "STP");

    char buf[8];
    snprintf(buf, sizeof(buf), " %d/%d", currentFile + 1, numFiles);
    display.print(buf);

    display.display();
}

// ---- I2S PDM TX -----------------------------------------------------------

void setupI2SPdmTx() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_pdm_tx_config_t pdm_cfg = {
        .clk_cfg  = I2S_PDM_TX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk  = I2S_GPIO_UNUSED,
            .dout = (gpio_num_t)LASER_PIN,
            .invert_flags = { .clk_inv = false },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(tx_handle, &pdm_cfg));
}

// ---- Playback control -----------------------------------------------------

// Disables and fully deletes the I2S channel, then forces GPIO0 LOW.
// i2s_channel_disable alone leaves GPIO0 at its last PDM state (often HIGH),
// keeping the MOSFET and laser on. Deleting the channel releases the pin
// from the GPIO matrix so we can drive it LOW directly.
void killI2S() {
    if (i2sReady && tx_handle) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
        tx_handle = nullptr;
        i2sReady = false;
    }
    pinMode(LASER_PIN, OUTPUT);
    digitalWrite(LASER_PIN, LOW);
}

void startPlay() {
    if (numFiles == 0) return;
    if (!audioS16 && !loadWavFromSD(wavFiles[currentFile])) return;
    if (!i2sReady) {
        setupI2SPdmTx();
        i2sReady = true;
    }
    playPos = 0;
    playing = true;
    i2s_channel_enable(tx_handle);
    updateDisplay();
}

void stopPlay() {
    playing = false;
    killI2S();
    updateDisplay();
}

void togglePlay() { if (playing) stopPlay(); else startPlay(); }

void nextFile() {
    bool wasPlaying = playing;
    if (playing) {
        killI2S();
        playing = false;
    }
    freeAudio();
    currentFile = (currentFile + 1) % numFiles;
    updateDisplay();
    if (wasPlaying) startPlay();
}

// ---- Buttons --------------------------------------------------------------

void checkButtons() {
    static unsigned long lastPlay = 0, lastNext = 0;
    unsigned long now = millis();
    if (digitalRead(BTN_PLAY) == LOW && now - lastPlay > 200) {
        lastPlay = now; togglePlay();
    }
    if (digitalRead(BTN_NEXT) == LOW && now - lastNext > 200) {
        lastNext = now; nextFile();
    }
}

// ---- Arduino --------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);

    SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS);

    display.begin();
    display.setContrast(60);
    display.clearDisplay();
    display.setTextColor(BLACK);
    display.setCursor(27, 20);
    display.print("LASEC");
    display.display();
    delay(500);

    if (!sd.begin(SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(4), &SPI))) {
        Serial.println("SD init failed");
        display.clearDisplay();
        display.setCursor(0, 20);
        display.print("SD failed!");
        display.display();
        return;
    }

    scanWavFiles();
    updateDisplay();
    // I2S is initialized lazily on first play press so GPIO0 stays low until then

    Serial.println("Ready");
}

void loop() {
    checkButtons();

    if (playing && audioS16) {
        size_t left  = audioLen - playPos;
        size_t chunk = min(left, (size_t)256);
        size_t written = 0;
        i2s_channel_write(tx_handle, audioS16 + playPos,
                          chunk * sizeof(int16_t), &written, pdMS_TO_TICKS(20));
        playPos += written / sizeof(int16_t);
        if (playPos >= audioLen) playPos = 0;
    }
}
