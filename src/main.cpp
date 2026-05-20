#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "driver/i2s_pdm.h"

SdFat32 sd;

#define LASER_PIN   0
#define BTN_PLAY    1
#define TFT_CS     22
#define TFT_RST    21
#define TFT_DC     20
#define TFT_MOSI   18
#define TFT_SCK    19
#define SD_CS       4
#define SD_MISO    20
#define SAMPLE_RATE 16000
#define MAX_FILES   20

Adafruit_PCD8544 display = Adafruit_PCD8544(TFT_DC, TFT_CS, TFT_RST);

String wavFiles[MAX_FILES];
int    numFiles    = 0;
int    currentFile = 0;
bool   playing     = false;

int16_t *audioS16 = nullptr;
size_t   audioLen = 0;
size_t   playPos  = 0;
i2s_chan_handle_t tx_handle = nullptr;
bool i2sReady = false;

void freeAudio() { free(audioS16); audioS16 = nullptr; audioLen = 0; playPos = 0; }

bool loadWavFromSD(const String &filename) {
    freeAudio();
    String path = "/" + filename;
    File32 f = sd.open(path.c_str(), O_RDONLY);
    if (!f) return false;
    size_t sz = f.size();
    if (sz <= 44) { f.close(); return false; }
    audioLen = sz - 44;
    uint8_t *raw = (uint8_t *)malloc(audioLen);
    audioS16     = (int16_t *)malloc(audioLen * sizeof(int16_t));
    if (!raw || !audioS16) { free(raw); freeAudio(); f.close(); return false; }
    f.seek(44); f.read(raw, audioLen); f.close();
    for (size_t i = 0; i < audioLen; i++)
        audioS16[i] = ((int16_t)raw[i] - 128) << 8;
    free(raw);
    return true;
}

void scanWavFiles() {
    numFiles = 0;
    File32 root = sd.open("/");
    File32 entry;
    while (numFiles < MAX_FILES && entry.openNext(&root, O_RDONLY)) {
        if (!entry.isDirectory()) {
            char buf[32];
            entry.getName(buf, sizeof(buf));
            String name(buf);
            String upper = name; upper.toUpperCase();
            if (upper.endsWith(".WAV")) wavFiles[numFiles++] = name;
        }
        entry.close();
    }
    root.close();
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(27, 0); display.print("LASEC");
    display.drawFastHLine(0, 9, 84, BLACK);
    if (numFiles == 0) {
        display.setCursor(0, 20); display.print("No WAV on SD");
        display.display(); return;
    }
    int start = currentFile - 1;
    if (start < 0) start = 0;
    if (start + 3 > numFiles) start = max(0, numFiles - 3);
    for (int i = 0; i < 3 && (start+i) < numFiles; i++) {
        int idx = start + i;
        int y   = 11 + i * 9;
        if (idx == currentFile) {
            display.fillRect(0, y-1, 84, 9, BLACK);
            display.setTextColor(WHITE);
        } else display.setTextColor(BLACK);
        display.setCursor(0, y);
        String name = wavFiles[idx];
        if (name.length() > 12) name = name.substring(0, 12);
        display.print(idx == currentFile ? ">" : " ");
        display.print(name);
    }
    display.setTextColor(BLACK);
    display.drawFastHLine(0, 38, 84, BLACK);
    display.setCursor(0, 40);
    display.print(playing ? "PLY" : "STP");
    char buf[8]; snprintf(buf, sizeof(buf), " %d/%d", currentFile+1, numFiles);
    display.print(buf);
    display.display();
}

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

void startPlay() {
    if (numFiles == 0) return;
    if (!audioS16 && !loadWavFromSD(wavFiles[currentFile])) return;
    if (!i2sReady) { setupI2SPdmTx(); i2sReady = true; }
    playPos = 0; playing = true;
    i2s_channel_enable(tx_handle);
    updateDisplay();
}

void stopPlay() {
    playing = false;
    if (i2sReady && tx_handle) i2s_channel_disable(tx_handle);
    updateDisplay();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    pinMode(BTN_PLAY, INPUT_PULLUP);
    SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS);
    display.begin();
    display.setContrast(60);
    display.clearDisplay();
    display.setCursor(27, 20); display.print("LASEC");
    display.display();
    delay(500);
    if (!sd.begin(SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(4), &SPI))) {
        Serial.println("SD init failed"); return;
    }
    scanWavFiles();
    updateDisplay();
}

void loop() {
    static unsigned long lastPlay = 0;
    unsigned long now = millis();
    if (digitalRead(BTN_PLAY) == LOW && now - lastPlay > 200) {
        lastPlay = now;
        if (playing) stopPlay(); else startPlay();
    }
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
