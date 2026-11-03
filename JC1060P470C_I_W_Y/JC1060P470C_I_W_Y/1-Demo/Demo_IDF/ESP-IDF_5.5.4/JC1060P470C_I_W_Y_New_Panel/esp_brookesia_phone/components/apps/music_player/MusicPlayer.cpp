/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"

#include "gui_music/lv_demo_music.h"
#include "gui_music/lv_demo_music_main.h"
#include "MusicPlayer.hpp"

#define MUSIC_DIR   BSP_SPIFFS_MOUNT_POINT "/music"

using namespace std;

LV_IMG_DECLARE(img_app_music_player);

static const char *TAG = "MusicPlayer";

MusicPlayer::MusicPlayer():
    ESP_Brookesia_PhoneApp("Music Player", &img_app_music_player, true), // auto_resize_visual_area
    _file_iterator(NULL)
{
}

MusicPlayer::~MusicPlayer()
{
}

bool MusicPlayer::run(void)
{
    lv_demo_music(lv_scr_act(), _file_iterator);

    return true;
}

bool MusicPlayer::pause(void)
{
     ESP_LOGI(TAG, "pause");
    //_lv_demo_music_exit_pause();
    if (audio_player_pause() != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_pause failed");
        return false;
    }
    notifyCoreClosed();
    return true;
}

bool MusicPlayer::back(void)
{
    ESP_LOGI(TAG, "back");
    if (audio_player_pause() != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_pause failed");
        return false;
    }
    notifyCoreClosed();

    return true;
}

bool MusicPlayer::close(void)
{
    ESP_LOGI(TAG, "close");
    if (audio_player_pause() != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_pause failed");
        return false;
    }
    notifyCoreClosed(); 
    return true;
}

bool MusicPlayer::init(void)
{
    bsp_extra_codec_dev_stop();
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_ERROR_CHECK(bsp_extra_codec_init());
        vTaskDelay(100 / portTICK_PERIOD_MS);
    if (bsp_extra_player_init() != ESP_OK) {
        ESP_LOGE(TAG, "Play init with SPIFFS failed");
        return false;
    }

    if (bsp_extra_file_instance_init(MUSIC_DIR, &_file_iterator) != ESP_OK) {
        ESP_LOGE(TAG, "bsp_extra_file_instance_init failed");
        return false;
    }

    
    return true;
}
void MusicPlayer::stop_audio_fully(void)
{
    /* 1. 退出 LV music（停 UI + 停内部状态机） */
    _lv_demo_music_exit_pause();

    /* 2. 停 audio_player 任务 */
    audio_player_stop();

    /* 3. 停 I2S 硬件 */
    bsp_extra_codec_dev_stop();

    /* 4. 静音 codec（防止残音） */
    bsp_extra_codec_mute_set(true);

    /* 5. 给一点时间让 DMA / I2S 真正停干净 */
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "Music audio fully stopped");
}