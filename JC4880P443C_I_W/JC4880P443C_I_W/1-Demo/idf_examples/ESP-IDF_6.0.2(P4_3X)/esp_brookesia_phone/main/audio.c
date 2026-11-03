#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "esp_system.h"
#include <stdlib.h>
static const char *TAG = "i2s_es8311";

extern const uint8_t music_pcm_start[] asm("_binary_canon_pcm_start");
extern const uint8_t music_pcm_end[]   asm("_binary_canon_pcm_end");

#define EXAMPLE_RECV_BUF_SIZE (1024)

void i2s_echo(void *args)
{
    esp_err_t ret = ESP_OK;

    // 使用bsp_extra_codec_init初始化所有音频设备
    esp_err_t err = bsp_extra_codec_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio codecs");
        vTaskDelete(NULL);
        return;
    }

    // 确保有足够时间完成初始化
    vTaskDelay(200 / portTICK_PERIOD_MS);

    /* 设置音频参数 */
    ret = bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set audio parameters");
        vTaskDelete(NULL);
        return;
    }

    // 设置默认音量
    int volume_set;
    ret = bsp_extra_codec_volume_set(60, &volume_set);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set volume, this is normal if the codec doesn't support this function");
    }

    // 设置录音增益
    ret = bsp_extra_codec_in_gain_set(CODEC_DEFAULT_ADC_VOLUME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set input gain");
        vTaskDelete(NULL);
        return;
    }

    // 分配缓冲区用于录音和播放
    uint8_t *buffer = (uint8_t *)malloc(EXAMPLE_RECV_BUF_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Starting echo task, read and write audio data");

    while (1) {
        size_t bytes_read = 0;
        
        // 从麦克风读取音频数据
        ret = bsp_extra_i2s_read(buffer, EXAMPLE_RECV_BUF_SIZE, &bytes_read, 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[echo] i2s read failed, %s", esp_err_to_name(ret));
            continue;
        }

        if (bytes_read > 0) {
            size_t bytes_written = 0;
            // 将读取到的数据立即播放回去，实现回声效果
            ret = bsp_extra_i2s_write(buffer, bytes_read, &bytes_written, 1000);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[echo] i2s write failed, %s", esp_err_to_name(ret));
                continue;
            }
            
            ESP_LOGD(TAG, "[echo] Read %d bytes, wrote %d bytes", (int)bytes_read, (int)bytes_written);
        }
    }

    free(buffer);
    vTaskDelete(NULL);
}

void i2s_music(void *args)
{
    esp_err_t ret = ESP_OK;
    uint8_t *data_ptr = (uint8_t *)music_pcm_start;
    size_t music_size = music_pcm_end - music_pcm_start;

    // 使用bsp_extra_codec_init初始化所有音频设备
    esp_err_t err = bsp_extra_codec_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio codecs");
        vTaskDelete(NULL);
        return;
    }

    // 确保有足够时间完成初始化
    vTaskDelay(200 / portTICK_PERIOD_MS);

    /* 设置音频参数 */
    ret = bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set audio parameters");
        vTaskDelete(NULL);
        return;
    }

    /* 设置默认音量 */
    int volume_set;
    ret = bsp_extra_codec_volume_set(60, &volume_set);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set volume, this is normal if the codec doesn't support this function");
    }

    while (1) {
        size_t bytes_written = 0;
        ret = bsp_extra_i2s_write(data_ptr, music_size, &bytes_written, 1000);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[music] i2s music play failed, %s", esp_err_to_name(ret));
            vTaskDelay(1000 / portTICK_PERIOD_MS); // 延迟1秒后重试
            continue;
        }
        
        ESP_LOGI(TAG, "[music] i2s music played, %d bytes are written.", (int)bytes_written);
        
        vTaskDelay(2000 / portTICK_PERIOD_MS); // 延迟2秒再播放
    }
    vTaskDelete(NULL);
}