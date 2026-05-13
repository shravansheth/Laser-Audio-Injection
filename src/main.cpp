#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "driver/i2s_pdm.h"

SdFat32 sd;

#define LASER_PIN   0
#define SD_CS       4
#define SD_MISO    20
#define SAMPLE_RATE 16000

int16_t *audioS16 = nullptr;
size_t   audioLen = 0;
i2s_chan_handle_t tx_handle = nullptr;

void freeAudio() {
    free(audioS16);
    audioS16 = nullptr;
    audioLen = 0;
}

bool loadWav(const char *path) {
    freeAudio();
    File32 f = sd.open(path, O_RDONLY);
    if (!f) { Serial.println("open failed"); return false; }
    size_t sz = f.size();
    if (sz <= 44) { f.close(); return false; }
    audioLen = sz - 44;
    uint8_t *raw  = (uint8_t *)malloc(audioLen);
    audioS16      = (int16_t *)malloc(audioLen * sizeof(int16_t));
    if (!raw || !audioS16) { free(raw); freeAudio(); f.close(); return false; }
    f.seek(44);
    f.read(raw, audioLen);
    f.close();
    for (size_t i = 0; i < audioLen; i++)
        audioS16[i] = ((int16_t)raw[i] - 128) << 8;
    free(raw);
    Serial.printf("loaded %d samples\n", (int)audioLen);
    return true;
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

void setup() {
    Serial.begin(115200);
    delay(500);
    SPI.begin(19, SD_MISO, 18, SD_CS);
    if (!sd.begin(SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(4), &SPI))) {
        Serial.println("SD init failed");
        return;
    }
    File32 root = sd.open("/");
    File32 entry;
    while (entry.openNext(&root, O_RDONLY)) {
        if (!entry.isDirectory()) {
            char buf[32];
            entry.getName(buf, sizeof(buf));
            String name(buf);
            String upper = name; upper.toUpperCase();
            if (upper.endsWith(".WAV")) {
                entry.close();
                if (loadWav(("/" + name).c_str())) {
                    setupI2SPdmTx();
                    i2s_channel_enable(tx_handle);
                    break;
                }
            }
        }
        entry.close();
    }
    root.close();
}

void loop() {
    if (!audioS16 || audioLen == 0) { delay(1000); return; }
    size_t written = 0;
    i2s_channel_write(tx_handle, audioS16, audioLen * sizeof(int16_t), &written, portMAX_DELAY);
}
