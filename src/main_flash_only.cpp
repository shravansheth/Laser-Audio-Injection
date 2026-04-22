// ============================================================
// FLASH-ONLY PDM — loads cmd1.wav embedded in firmware flash.
// Requires in platformio.ini: board_build.embed_files = cmd1.wav
// No SD card, no display. Continuous loop playback via I2S PDM.
// ============================================================

#include <Arduino.h>
#include "driver/i2s_pdm.h"

const int LASER_PIN = 0;
const uint32_t SAMPLE_RATE = 16000;
#define LED_PWR 5   // power indicator LED — GPIO5 through 220Ω to GND

// cmd1.wav is embedded into flash by board_build.embed_files in platformio.ini.
// The linker exposes the raw bytes via these symbols.
extern const uint8_t cmd1_wav_start[] asm("_binary_cmd1_wav_start");
extern const uint8_t cmd1_wav_end[]   asm("_binary_cmd1_wav_end");

int16_t *audioS16 = nullptr;
size_t audioLen = 0;

i2s_chan_handle_t tx_handle = nullptr;

bool loadWavFromFlash() {
    size_t wavSize = cmd1_wav_end - cmd1_wav_start;

    if (wavSize <= 44) {
        Serial.println("Embedded WAV too small");
        return false;
    }

    audioLen = wavSize - 44;
    const uint8_t *audioU8 = cmd1_wav_start + 44;

    audioS16 = (int16_t *)malloc(audioLen * sizeof(int16_t));
    if (!audioS16) {
        Serial.println("Not enough RAM for S16 buffer");
        return false;
    }

    for (size_t i = 0; i < audioLen; i++) {
        audioS16[i] = ((int16_t)audioU8[i] - 128) << 8;
    }

    Serial.print("Loaded samples: ");
    Serial.println(audioLen);
    Serial.print("Duration: ");
    Serial.print(audioLen / (float)SAMPLE_RATE);
    Serial.println("s");

    return true;
}

void setupI2SPdmTx() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_pdm_tx_config_t pdm_cfg = {
        .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = I2S_GPIO_UNUSED,
            .dout = (gpio_num_t)LASER_PIN,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(tx_handle, &pdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(LED_PWR, OUTPUT);
    digitalWrite(LED_PWR, HIGH);

    if (!loadWavFromFlash()) {
        return;
    }

    Serial.println("Starting I2S PDM TX...");
    setupI2SPdmTx();

    Serial.println("Playing via I2S PDM DMA...");
}

void loop() {
    if (!audioS16 || audioLen == 0) {
        delay(1000);
        return;
    }

    size_t bytesWritten = 0;
    esp_err_t err = i2s_channel_write(
        tx_handle,
        audioS16,
        audioLen * sizeof(int16_t),
        &bytesWritten,
        portMAX_DELAY
    );

    if (err != ESP_OK) {
        Serial.print("i2s_channel_write error: ");
        Serial.println(err);
        delay(1000);
    }
}
