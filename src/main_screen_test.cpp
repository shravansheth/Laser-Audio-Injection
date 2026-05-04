// ============================================================
// SCREEN TEST — Nokia 5110 / PCD8544 (84x48 monochrome LCD)
// Displays "Lasec" centered on screen.
// Loads cmd1.wav from flash and streams via I2S PDM.
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "driver/i2s_pdm.h"

#define TFT_CS   22   // GPIO22 — SCE
#define TFT_RST  21   // GPIO21 — RST
#define TFT_DC   20   // GPIO20 — DC
#define TFT_MOSI 18   // GPIO18 — DN/MOSI
#define TFT_SCK  19   // GPIO19 — SCLK
#define LED_PWR   5   // GPIO5  — power LED through 220Ω to GND

const int LASER_PIN = 0;
const uint32_t SAMPLE_RATE = 16000;

// Hardware SPI constructor: (dc, cs, rst)
Adafruit_PCD8544 display = Adafruit_PCD8544(TFT_DC, TFT_CS, TFT_RST);

extern const uint8_t cmd1_wav_start[] asm("_binary_cmd1_wav_start");
extern const uint8_t cmd1_wav_end[]   asm("_binary_cmd1_wav_end");

int16_t *audioS16 = nullptr;
size_t   audioLen = 0;

i2s_chan_handle_t tx_handle = nullptr;

// ---- Audio ----------------------------------------------------------------

bool loadWavFromFlash() {
    size_t wavSize = cmd1_wav_end - cmd1_wav_start;
    if (wavSize <= 44) { Serial.println("WAV too small"); return false; }

    audioLen = wavSize - 44;
    const uint8_t *audioU8 = cmd1_wav_start + 44;

    audioS16 = (int16_t *)malloc(audioLen * sizeof(int16_t));
    if (!audioS16) { Serial.println("Not enough RAM"); return false; }

    for (size_t i = 0; i < audioLen; i++)
        audioS16[i] = ((int16_t)audioU8[i] - 128) << 8;

    Serial.printf("Loaded %d samples (%.1fs)\n", (int)audioLen, audioLen / (float)SAMPLE_RATE);
    return true;
}

// ---- Display --------------------------------------------------------------

void setupDisplay() {
    SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
    display.begin();
    display.setContrast(60);  // 0-127 — increase if screen is too faint
    display.clearDisplay();
    display.setTextSize(1);   // 6x8px per char — fits "Lasec" at 30px wide on 84px screen
    display.setTextColor(BLACK);
    // Center: 84px wide, "Lasec" = 5 chars x 6px = 30px. x=(84-30)/2=27. y=(48-8)/2=20.
    display.setCursor(27, 20);
    display.print("Lasec");
    display.display();        // PCD8544 requires explicit push to screen
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
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
}

// ---- Arduino --------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(LED_PWR, OUTPUT);
    digitalWrite(LED_PWR, HIGH);

    setupDisplay();

    if (!loadWavFromFlash()) return;

    setupI2SPdmTx();
    Serial.println("Playing via I2S PDM...");
}

void loop() {
    if (!audioS16 || audioLen == 0) { delay(1000); return; }

    size_t bytesWritten = 0;
    esp_err_t err = i2s_channel_write(
        tx_handle, audioS16, audioLen * sizeof(int16_t),
        &bytesWritten, portMAX_DELAY
    );
    if (err != ESP_OK) {
        Serial.printf("i2s write error: %d\n", err);
        delay(1000);
    }
}
